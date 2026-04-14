#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/statvfs.h>
#include <vector>

namespace fs = std::filesystem;

struct MountRow
{
    std::string mount_point;
    std::string options;
    std::string fs_type;
    std::string source;
    std::string super_options;
};

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " PATH\n"
        << "\n"
        << "Print filesystem type, device, and mount options for a path.\n";
}

static std::vector<std::string> split(const std::string &s, char delim)
{
    std::vector<std::string> out;
    std::string cur;
    for (char c : s)
    {
        if (c == delim)
        {
            out.push_back(cur);
            cur.clear();
        }
        else
        {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    return out;
}

static std::string unescape_mount_field(const std::string &s)
{
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '\\' && i + 3 < s.size() && std::isdigit(static_cast<unsigned char>(s[i + 1])) &&
            std::isdigit(static_cast<unsigned char>(s[i + 2])) && std::isdigit(static_cast<unsigned char>(s[i + 3])))
        {
            int v = (s[i + 1] - '0') * 64 + (s[i + 2] - '0') * 8 + (s[i + 3] - '0');
            out.push_back(static_cast<char>(v));
            i += 3;
        }
        else
        {
            out.push_back(s[i]);
        }
    }
    return out;
}

static bool is_path_prefix(const std::string &prefix, const std::string &path)
{
    if (prefix == "/")
        return true;
    if (path.size() < prefix.size())
        return false;
    if (path.compare(0, prefix.size(), prefix) != 0)
        return false;
    return path.size() == prefix.size() || path[prefix.size()] == '/';
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

    fs::path p = fs::weakly_canonical(argv[1]);
    std::string target = p.string();

    std::ifstream in("/proc/self/mountinfo");
    if (!in)
    {
        std::cerr << "fstype: cannot read /proc/self/mountinfo\n";
        return 1;
    }

    std::string line;
    MountRow best;
    std::size_t best_len = 0;

    while (std::getline(in, line))
    {
        auto fields = split(line, ' ');
        auto dash = std::find(fields.begin(), fields.end(), "-");
        if (dash == fields.end())
            continue;

        std::size_t i_dash = static_cast<std::size_t>(dash - fields.begin());
        if (fields.size() < i_dash + 4 || fields.size() < 7)
            continue;

        std::string mount_point = unescape_mount_field(fields[4]);
        if (!is_path_prefix(mount_point, target))
            continue;

        if (mount_point.size() < best_len)
            continue;

        best_len = mount_point.size();
        best.mount_point = mount_point;
        best.options = fields[5];
        best.fs_type = fields[i_dash + 1];
        best.source = unescape_mount_field(fields[i_dash + 2]);
        best.super_options = fields[i_dash + 3];
    }

    if (best_len == 0)
    {
        std::cerr << "fstype: mount point not found\n";
        return 1;
    }

    struct statvfs sv
    {
    };
    if (statvfs(target.c_str(), &sv) != 0)
    {
        std::cerr << "fstype: statvfs failed for path\n";
        return 1;
    }

    std::cout << "path: " << target << "\n";
    std::cout << "mount: " << best.mount_point << "\n";
    std::cout << "device: " << best.source << "\n";
    std::cout << "fstype: " << best.fs_type << "\n";
    std::cout << "mount_options: " << best.options << "\n";
    std::cout << "super_options: " << best.super_options << "\n";
    std::cout << "block_size: " << sv.f_bsize << "\n";
    return 0;
}
