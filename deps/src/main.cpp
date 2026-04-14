#include <cstdio>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Dep
{
    std::string name;
    std::string resolved;
    bool missing = false;
};

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " [OPTIONS] ELF_FILE\n"
        << "\n"
        << "Show shared library dependency tree (uses ldd).\n"
        << "\n"
        << "Options:\n"
        << "  -r, --recurse   Recurse into transitive dependencies\n"
        << "  -h, --help      Show this help\n";
}

static std::string run_cmd(const std::string &cmd)
{
    std::string out;
    FILE *fp = popen(cmd.c_str(), "r");
    if (!fp)
        return out;

    char buf[4096];
    while (fgets(buf, sizeof(buf), fp))
    {
        out += buf;
    }
    pclose(fp);
    return out;
}

static std::string trim(const std::string &s)
{
    std::size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b])))
        ++b;
    std::size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
        --e;
    return s.substr(b, e - b);
}

static std::vector<Dep> parse_ldd(const std::string &text)
{
    std::vector<Dep> deps;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line))
    {
        line = trim(line);
        if (line.empty())
            continue;

        if (line.find("=>") != std::string::npos)
        {
            auto p = line.find("=>");
            std::string left = trim(line.substr(0, p));
            std::string right = trim(line.substr(p + 2));
            auto lp = right.find('(');
            if (lp != std::string::npos)
            {
                right = trim(right.substr(0, lp));
            }

            Dep d;
            d.name = left;
            d.missing = (right == "not found");
            d.resolved = d.missing ? "" : right;
            deps.push_back(d);
            continue;
        }

        if (!line.empty() && line[0] == '/')
        {
            auto lp = line.find('(');
            std::string path = (lp == std::string::npos) ? line : trim(line.substr(0, lp));
            Dep d;
            d.name = path;
            d.resolved = path;
            deps.push_back(d);
        }
    }
    return deps;
}

static void print_tree(const fs::path &root, bool recurse, std::set<std::string> &seen, int depth = 0)
{
    std::string label = root.string();
    std::cout << std::string(depth * 2, ' ') << label << "\n";

    std::string cmd = "ldd '" + root.string() + "' 2>/dev/null";
    auto deps = parse_ldd(run_cmd(cmd));

    for (const auto &d : deps)
    {
        std::cout << std::string((depth + 1) * 2, ' ') << "|- " << d.name;
        if (d.missing)
        {
            std::cout << " => not found";
        }
        else if (!d.resolved.empty() && d.resolved != d.name)
        {
            std::cout << " => " << d.resolved;
        }
        std::cout << "\n";

        if (recurse && !d.missing && !d.resolved.empty() && d.resolved[0] == '/')
        {
            if (seen.insert(d.resolved).second)
            {
                print_tree(d.resolved, true, seen, depth + 2);
            }
        }
    }
}

int main(int argc, char **argv)
{
    bool recurse = false;
    std::optional<fs::path> file;

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
        if (!arg.empty() && arg[0] == '-')
        {
            std::cerr << "deps: unknown option '" << arg << "'\n";
            return 1;
        }
        if (file)
        {
            std::cerr << "deps: too many input files\n";
            return 1;
        }
        file = fs::path(arg);
    }

    if (!file)
    {
        print_help(argv[0]);
        return 1;
    }

    if (!fs::exists(*file))
    {
        std::cerr << "deps: file not found: " << file->string() << "\n";
        return 1;
    }

    std::set<std::string> seen;
    seen.insert(file->string());
    print_tree(*file, recurse, seen);
    return 0;
}
