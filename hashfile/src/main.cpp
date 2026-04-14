#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " [OPTIONS] FILE\n"
        << "\n"
        << "Compute md5/sha1/sha256/sha512/blake2 hashes for a file.\n"
        << "\n"
        << "Options:\n"
        << "      --check <HASH>       Compare against expected hash\n"
        << "      --check <ALGO:HASH>  Compare only specific algorithm\n"
        << "  -h, --help               Show this help\n";
}

static std::string shell_quote(const std::string &s)
{
    std::string out = "'";
    for (char c : s)
    {
        if (c == '\'')
            out += "'\\''";
        else
            out.push_back(c);
    }
    out += "'";
    return out;
}

static std::string run_cmd(const std::string &cmd)
{
    FILE *fp = popen(cmd.c_str(), "r");
    if (!fp)
        return "";

    std::string out;
    char buf[1024];
    while (fgets(buf, sizeof(buf), fp))
        out += buf;
    pclose(fp);
    return out;
}

static std::string first_token(const std::string &s)
{
    std::istringstream iss(s);
    std::string tok;
    iss >> tok;
    return tok;
}

static std::string to_lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
                   { return static_cast<char>(std::tolower(c)); });
    return s;
}

int main(int argc, char **argv)
{
    std::string check;
    fs::path file;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            print_help(argv[0]);
            return 0;
        }
        if (arg == "--check")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "hashfile: --check requires a value\n";
                return 1;
            }
            check = argv[++i];
            continue;
        }
        if (!arg.empty() && arg[0] == '-')
        {
            std::cerr << "hashfile: unknown option '" << arg << "'\n";
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
        std::cerr << "hashfile: file not found: " << file.string() << "\n";
        return 1;
    }

    const std::array<std::pair<std::string, std::string>, 5> algos = {
        std::make_pair("md5", "md5sum"),
        std::make_pair("sha1", "sha1sum"),
        std::make_pair("sha256", "sha256sum"),
        std::make_pair("sha512", "sha512sum"),
        std::make_pair("blake2", "b2sum")};

    std::map<std::string, std::string> hashes;
    std::string quoted = shell_quote(file.string());

    for (const auto &a : algos)
    {
        std::string out = run_cmd(a.second + " " + quoted + " 2>/dev/null");
        std::string h = first_token(out);
        if (h.empty())
            h = "(unavailable)";
        hashes[a.first] = to_lower(h);
    }

    for (const auto &a : algos)
    {
        std::cout << a.first << "\t" << hashes[a.first] << "\n";
    }

    if (!check.empty())
    {
        std::string c = to_lower(check);
        auto p = c.find(':');
        bool ok = false;

        if (p != std::string::npos)
        {
            std::string algo = c.substr(0, p);
            std::string want = c.substr(p + 1);
            auto it = hashes.find(algo);
            if (it == hashes.end())
            {
                std::cerr << "hashfile: unknown algorithm in --check: " << algo << "\n";
                return 1;
            }
            ok = (it->second == want);
            std::cout << "check(" << algo << "): " << (ok ? "OK" : "MISMATCH") << "\n";
            return ok ? 0 : 1;
        }

        for (const auto &kv : hashes)
        {
            if (kv.second == c)
            {
                ok = true;
                break;
            }
        }
        std::cout << "check(any): " << (ok ? "OK" : "MISMATCH") << "\n";
        return ok ? 0 : 1;
    }

    return 0;
}
