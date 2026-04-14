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
    std::string type;
    std::size_t offset = 0;
    std::string text;
};

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " [OPTIONS] FILE\n"
        << "\n"
        << "Extract printable strings and classify them (path/url/ip/version/error/uuid).\n"
        << "\n"
        << "Options:\n"
        << "  -m, --min <N>     Minimum string length (default: 4)\n"
        << "  -h, --help        Show this help\n";
}

static bool is_print(unsigned char c)
{
    return c >= 32 && c <= 126;
}

static std::string classify(const std::string &s)
{
    static const std::regex re_url(R"((https?|ftp)://[^\s]+)", std::regex::icase);
    static const std::regex re_ipv4(R"(\b((25[0-5]|2[0-4]\d|1?\d?\d)\.){3}(25[0-5]|2[0-4]\d|1?\d?\d)\b)");
    static const std::regex re_uuid(R"(\b[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[1-5][0-9a-fA-F]{3}-[89abAB][0-9a-fA-F]{3}-[0-9a-fA-F]{12}\b)");
    static const std::regex re_version(R"(\bv?\d+\.\d+(\.\d+)?([-.][A-Za-z0-9]+)?\b)");
    static const std::regex re_path(R"((^|\s)(\.{1,2}/|/|~/?)[^\s]+)");
    static const std::regex re_error(R"((error|failed|exception|denied|invalid|fatal|panic))", std::regex::icase);

    if (std::regex_search(s, re_url))
        return "url";
    if (std::regex_search(s, re_ipv4))
        return "ip";
    if (std::regex_search(s, re_uuid))
        return "uuid";
    if (std::regex_search(s, re_path))
        return "path";
    if (std::regex_search(s, re_version))
        return "version";
    if (std::regex_search(s, re_error))
        return "error";
    return "text";
}

int main(int argc, char **argv)
{
    int min_len = 4;
    fs::path file;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            print_help(argv[0]);
            return 0;
        }
        if (arg == "-m" || arg == "--min")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "nstr: " << arg << " requires a value\n";
                return 1;
            }
            try
            {
                min_len = std::stoi(argv[++i]);
                if (min_len <= 0)
                    throw std::invalid_argument("non-positive");
            }
            catch (...)
            {
                std::cerr << "nstr: --min must be a positive integer\n";
                return 1;
            }
            continue;
        }
        if (!arg.empty() && arg[0] == '-')
        {
            std::cerr << "nstr: unknown option '" << arg << "'\n";
            return 1;
        }
        file = arg;
    }

    if (file.empty())
    {
        print_help(argv[0]);
        return 1;
    }

    std::ifstream in(file, std::ios::binary);
    if (!in)
    {
        std::cerr << "nstr: cannot open file: " << file.string() << "\n";
        return 1;
    }

    std::vector<Hit> hits;
    std::string cur;
    std::size_t cur_off = 0;
    std::size_t pos = 0;

    char ch = 0;
    while (in.get(ch))
    {
        unsigned char c = static_cast<unsigned char>(ch);
        if (is_print(c))
        {
            if (cur.empty())
                cur_off = pos;
            cur.push_back(static_cast<char>(c));
        }
        else
        {
            if (static_cast<int>(cur.size()) >= min_len)
                hits.push_back({classify(cur), cur_off, cur});
            cur.clear();
        }
        ++pos;
    }

    if (static_cast<int>(cur.size()) >= min_len)
        hits.push_back({classify(cur), cur_off, cur});

    std::cout << "TYPE\tOFFSET\tTEXT\n";
    for (const auto &h : hits)
    {
        std::cout << h.type << "\t" << h.offset << "\t" << h.text << "\n";
    }

    return hits.empty() ? 1 : 0;
}
