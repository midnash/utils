#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <linux/fs.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " PATH\n"
        << "\n"
        << "Inspect inode metadata, link count, blocks, and inode flags.\n";
}

static std::string mode_to_string(mode_t m)
{
    std::string s(10, '-');
    if (S_ISDIR(m))
        s[0] = 'd';
    else if (S_ISLNK(m))
        s[0] = 'l';
    else if (S_ISCHR(m))
        s[0] = 'c';
    else if (S_ISBLK(m))
        s[0] = 'b';
    else if (S_ISSOCK(m))
        s[0] = 's';
    else if (S_ISFIFO(m))
        s[0] = 'p';

    const char chars[] = {'r', 'w', 'x'};
    for (int i = 0; i < 9; ++i)
    {
        if ((m >> (8 - i)) & 1)
            s[i + 1] = chars[i % 3];
    }
    return s;
}

static void print_flag(const std::string &name, unsigned int flags, unsigned int mask)
{
    if (flags & mask)
        std::cout << "  - " << name << "\n";
}

int main(int argc, char **argv)
{
    if (argc >= 2)
    {
        std::string a = argv[1];
        if (a == "-h" || a == "--help")
        {
            print_help(argv[0]);
            return 0;
        }
    }

    if (argc != 2)
    {
        print_help(argv[0]);
        return 1;
    }

    fs::path p = argv[1];
    struct stat st
    {
    };
    if (lstat(p.c_str(), &st) != 0)
    {
        std::cerr << "inode: lstat failed: " << std::strerror(errno) << "\n";
        return 1;
    }

    std::cout << "path: " << p.string() << "\n";
    std::cout << "inode: " << st.st_ino << "\n";
    std::cout << "mode: " << mode_to_string(st.st_mode) << " (" << std::oct << (st.st_mode & 07777) << std::dec << ")\n";
    std::cout << "uid: " << st.st_uid << "\n";
    std::cout << "gid: " << st.st_gid << "\n";
    std::cout << "size: " << st.st_size << "\n";
    std::cout << "blocks: " << st.st_blocks << "\n";
    std::cout << "links: " << st.st_nlink << "\n";
    std::cout << "atime: " << st.st_atime << "\n";
    std::cout << "mtime: " << st.st_mtime << "\n";
    std::cout << "ctime: " << st.st_ctime << "\n";

    int fd = open(p.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0)
    {
        std::cout << "flags: (unavailable: cannot open file)\n";
        return 0;
    }

    unsigned int flags = 0;
    if (ioctl(fd, FS_IOC_GETFLAGS, &flags) != 0)
    {
        std::cout << "flags: (unavailable: ioctl failed)\n";
        close(fd);
        return 0;
    }
    close(fd);

    std::cout << "flags_raw: 0x" << std::hex << flags << std::dec << "\n";
    std::cout << "flags:\n";
    print_flag("immutable", flags, FS_IMMUTABLE_FL);
    print_flag("append-only", flags, FS_APPEND_FL);
    print_flag("nodump", flags, FS_NODUMP_FL);
    print_flag("noatime", flags, FS_NOATIME_FL);
    print_flag("sync", flags, FS_SYNC_FL);
    print_flag("dirsync", flags, FS_DIRSYNC_FL);

    return 0;
}
