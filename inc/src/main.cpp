#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct IncludeEdge
{
    std::string raw;
    bool angled = false;
    fs::path resolved;
    bool found = false;
};

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " [OPTIONS] FILE\n"
        << "\n"
        << "Find #include dependencies without invoking a compiler.\n"
        << "\n"
        << "Options:\n"
        << "  -I <DIR>          Add include search directory\n"
        << "  -r, --recurse     Recurse includes\n"
        << "  -h, --help        Show this help\n";
}

static std::vector<IncludeEdge> parse_includes(const fs::path &file,
                                               const std::vector<fs::path> &search_dirs)
{
    std::vector<IncludeEdge> out;
    std::ifstream in(file);
    if (!in)
        return out;

    std::regex re("^\\s*#\\s*include\\s*([<\"])([^>\"]+)[>\"]");
    std::string line;
    while (std::getline(in, line))
    {
        std::smatch m;
        if (!std::regex_search(line, m, re))
            continue;

        bool angled = m[1].str() == "<";
        std::string name = m[2].str();

        IncludeEdge e;
        e.raw = name;
        e.angled = angled;

        std::vector<fs::path> probes;
        if (!angled)
            probes.push_back(file.parent_path() / name);
        for (const auto &d : search_dirs)
            probes.push_back(d / name);

        for (const auto &p : probes)
        {
            std::error_code ec;
            if (fs::exists(p, ec) && fs::is_regular_file(p, ec))
            {
                e.found = true;
                e.resolved = fs::weakly_canonical(p, ec);
                if (ec)
                    e.resolved = p;
                break;
            }
        }

        out.push_back(e);
    }

    return out;
}

static void print_tree(const fs::path &file,
                       const std::vector<fs::path> &search_dirs,
                       bool recurse,
                       std::set<std::string> &seen,
                       int depth)
{
    std::cout << std::string(depth * 2, ' ') << file.string() << "\n";
    auto edges = parse_includes(file, search_dirs);

    for (const auto &e : edges)
    {
        std::cout << std::string((depth + 1) * 2, ' ') << "|- " << e.raw;
        if (e.found)
            std::cout << " => " << e.resolved.string();
        else
            std::cout << " (not found)";
        std::cout << "\n";

        if (recurse && e.found)
        {
            std::string key = e.resolved.string();
            if (seen.insert(key).second)
                print_tree(e.resolved, search_dirs, true, seen, depth + 2);
        }
    }
}

int main(int argc, char **argv)
{
    bool recurse = false;
    std::vector<fs::path> search_dirs = {"/usr/include", "/usr/local/include"};
    fs::path file;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            print_help(argv[0]);
            return 0;
        }
        if (arg == "-r" || arg == "--recurse")
        {
            recurse = true;
            continue;
        }
        if (arg == "-I")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "inc: -I requires a directory\n";
                return 1;
            }
            search_dirs.push_back(argv[++i]);
            continue;
        }
        if (!arg.empty() && arg[0] == '-')
        {
            std::cerr << "inc: unknown option '" << arg << "'\n";
            return 1;
        }
        if (!file.empty())
        {
            std::cerr << "inc: too many input files\n";
            return 1;
        }
        file = arg;
    }

    if (file.empty())
    {
        print_help(argv[0]);
        return 1;
    }

    if (!fs::exists(file) || !fs::is_regular_file(file))
    {
        std::cerr << "inc: file not found: " << file.string() << "\n";
        return 1;
    }

    std::error_code ec;
    file = fs::weakly_canonical(file, ec);
    std::set<std::string> seen;
    seen.insert(file.string());
    print_tree(file, search_dirs, recurse, seen, 0);
    return 0;
}
