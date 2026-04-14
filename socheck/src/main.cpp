#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " TARGET\n"
        << "\n"
        << "Check ELF binaries/.so files for unresolvable shared library dependencies.\n"
        << "TARGET can be a single file or a directory.\n";
}

static bool is_elf(const fs::path &p)
{
    std::ifstream in(p, std::ios::binary);
    if (!in)
        return false;
    unsigned char m[4] = {0};
    in.read(reinterpret_cast<char *>(m), 4);
    return in.gcount() == 4 && m[0] == 0x7F && m[1] == 'E' && m[2] == 'L' && m[3] == 'F';
}

static std::string run_ldd(const fs::path &p)
{
    std::string cmd = "ldd '" + p.string() + "' 2>/dev/null";
    FILE *fp = popen(cmd.c_str(), "r");
    if (!fp)
        return "";

    std::string out;
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp))
    {
        out += buf;
    }
    pclose(fp);
    return out;
}

static std::vector<std::string> unresolved(const std::string &ldd)
{
    std::vector<std::string> out;
    std::istringstream iss(ldd);
    std::string line;
    while (std::getline(iss, line))
    {
        if (line.find("=> not found") != std::string::npos)
        {
            std::size_t p = line.find("=>");
            if (p != std::string::npos)
            {
                std::string name = line.substr(0, p);
                while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back())))
                    name.pop_back();
                while (!name.empty() && std::isspace(static_cast<unsigned char>(name.front())))
                    name.erase(name.begin());
                out.push_back(name);
            }
        }
    }
    return out;
}

int main(int argc, char **argv)
{
    if (argc >= 2)
    {
        std::string arg = argv[1];
        if (arg == "-h" || arg == "--help")
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

    fs::path target = argv[1];
    if (!fs::exists(target))
    {
        std::cerr << "socheck: target not found: " << target.string() << "\n";
        return 1;
    }

    std::vector<fs::path> files;
    if (fs::is_regular_file(target))
    {
        files.push_back(target);
    }
    else if (fs::is_directory(target))
    {
        for (const auto &e : fs::recursive_directory_iterator(target, fs::directory_options::skip_permission_denied))
        {
            if (!e.is_regular_file())
                continue;
            if (e.path().extension() == ".so" || e.path().filename().string().find(".so.") != std::string::npos || is_elf(e.path()))
            {
                files.push_back(e.path());
            }
        }
    }
    else
    {
        std::cerr << "socheck: unsupported target type\n";
        return 1;
    }

    int broken = 0;
    for (const auto &f : files)
    {
        if (!is_elf(f))
            continue;
        auto miss = unresolved(run_ldd(f));
        if (!miss.empty())
        {
            ++broken;
            std::cout << f.string() << "\n";
            for (const auto &m : miss)
            {
                std::cout << "  - " << m << " (not found)\n";
            }
        }
    }

    if (broken == 0)
    {
        std::cout << "socheck: all checked ELF files resolved successfully\n";
        return 0;
    }

    std::cout << "socheck: " << broken << " file(s) have unresolved dependencies\n";
    return 1;
}
