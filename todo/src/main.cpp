#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Hit
{
    std::string tag;
    fs::path file;
    int line = 0;
    std::string text;
};

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " [OPTIONS] [PATH]\n"
        << "\n"
        << "Scan a codebase for TODO/FIXME/HACK/XXX comments.\n"
        << "\n"
        << "Options:\n"
        << "  -t, --tag <TAG>   Filter by tag (TODO/FIXME/HACK/XXX)\n"
        << "  -h, --help        Show this help\n";
}

static bool should_skip_dir(const fs::path &p)
{
    const std::string n = p.filename().string();
    return n == ".git" || n == "build" || n == "bin" || n == "CMakeFiles";
}

int main(int argc, char **argv)
{
    std::string tag_filter;
    fs::path root = ".";

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            print_help(argv[0]);
            return 0;
        }
        if (arg == "-t" || arg == "--tag")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "todo: " << arg << " requires a value\n";
                return 1;
            }
            tag_filter = argv[++i];
            std::transform(tag_filter.begin(), tag_filter.end(), tag_filter.begin(), ::toupper);
            continue;
        }
        if (!arg.empty() && arg[0] == '-')
        {
            std::cerr << "todo: unknown option '" << arg << "'\n";
            return 1;
        }
        root = arg;
    }

    if (!fs::exists(root) || !fs::is_directory(root))
    {
        std::cerr << "todo: not a directory: " << root.string() << "\n";
        return 1;
    }

    std::regex re("(TODO|FIXME|HACK|XXX)\\s*[:\\-]?\\s*(.*)", std::regex::icase);
    std::vector<Hit> hits;

    std::error_code ec;
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
    fs::recursive_directory_iterator end;

    while (it != end)
    {
        if (ec)
        {
            ec.clear();
            ++it;
            continue;
        }

        const auto p = it->path();
        if (it->is_directory(ec) && should_skip_dir(p))
        {
            it.disable_recursion_pending();
            ++it;
            continue;
        }

        if (!it->is_regular_file(ec))
        {
            ++it;
            continue;
        }

        std::ifstream in(p);
        if (!in)
        {
            ++it;
            continue;
        }

        std::string line;
        int ln = 0;
        while (std::getline(in, line))
        {
            ++ln;
            std::smatch m;
            if (!std::regex_search(line, m, re))
                continue;

            std::string tag = m[1].str();
            std::transform(tag.begin(), tag.end(), tag.begin(), ::toupper);
            if (!tag_filter.empty() && tag != tag_filter)
                continue;

            std::string text = m[2].str();
            hits.push_back({tag, fs::relative(p, root, ec), ln, text});
        }

        ++it;
    }

    std::sort(hits.begin(), hits.end(), [](const Hit &a, const Hit &b)
              {
                  if (a.tag != b.tag)
                      return a.tag < b.tag;
                  if (a.file != b.file)
                      return a.file.string() < b.file.string();
                  return a.line < b.line;
              });

    for (const auto &h : hits)
    {
        std::cout << h.tag << "  " << h.file.string() << ":" << h.line;
        if (!h.text.empty())
            std::cout << "  " << h.text;
        std::cout << "\n";
    }

    if (hits.empty())
        return 1;
    return 0;
}
