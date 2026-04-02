#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::string trim(const std::string &s)
{
    std::size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos)
    {
        return "";
    }
    std::size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

static std::string shell_quote(const std::string &s)
{
    std::string out = "'";
    for (char c : s)
    {
        if (c == '\'')
        {
            out += "'\\''";
        }
        else
        {
            out.push_back(c);
        }
    }
    out += "'";
    return out;
}

static std::string run_cmd(const std::string &cmd)
{
    std::array<char, 4096> buf{};
    std::string out;
    FILE *p = popen(cmd.c_str(), "r");
    if (!p)
    {
        return "";
    }
    while (fgets(buf.data(), static_cast<int>(buf.size()), p))
    {
        out += buf.data();
    }
    pclose(p);
    return out;
}

static std::string human_size(long long bytes)
{
    static const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    double value = static_cast<double>(bytes);
    int idx = 0;
    while (value >= 1024.0 && idx < 4)
    {
        value /= 1024.0;
        ++idx;
    }

    std::ostringstream oss;
    if (idx == 0)
    {
        oss << static_cast<long long>(value) << ' ' << units[idx];
    }
    else
    {
        oss << std::fixed << std::setprecision(2) << value << ' ' << units[idx];
    }
    return oss.str();
}

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " [OPTIONS] [REPO_PATH]\n"
        << "\n"
        << "Show git object sizes sorted largest-first.\n"
        << "\n"
        << "Options:\n"
        << "  -n, --top <N>  Number of rows to show [default: 20]\n"
        << "  -h, --help     Show this help\n";
}

int main(int argc, char **argv)
{
    int top_n = 20;
    fs::path repo = ".";

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            print_help(argv[0]);
            return 0;
        }
        if (arg == "-n" || arg == "--top")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "git-size: " << arg << " requires a value\n";
                return 1;
            }
            try
            {
                top_n = std::stoi(argv[++i]);
            }
            catch (...)
            {
                std::cerr << "git-size: --top must be an integer\n";
                return 1;
            }
            if (top_n <= 0)
            {
                std::cerr << "git-size: --top must be positive\n";
                return 1;
            }
            continue;
        }
        if (!arg.empty() && arg[0] == '-')
        {
            std::cerr << "git-size: unknown option '" << arg << "'\n";
            return 1;
        }
        repo = arg;
    }

    std::string qrepo = shell_quote(fs::absolute(repo).string());
    std::string check = trim(run_cmd("cd " + qrepo + " && git rev-parse --is-inside-work-tree 2>/dev/null"));
    if (check != "true")
    {
        std::cerr << "git-size: not a git repository: " << repo.string() << "\n";
        return 1;
    }

    std::string cmd =
        "cd " + qrepo +
        " && git rev-list --objects --all "
        "| git cat-file --batch-check='%(objectname) %(objecttype) %(objectsize) %(rest)' "
        "| sort -k3 -n -r | head -n " +
        std::to_string(top_n);

    std::string out = run_cmd(cmd);
    if (trim(out).empty())
    {
        std::cout << "No objects found.\n";
        return 0;
    }

    std::cout << std::left << std::setw(12) << "SIZE" << std::setw(12) << "TYPE" << std::setw(41) << "OBJECT" << "PATH\n";

    std::istringstream iss(out);
    std::string line;
    while (std::getline(iss, line))
    {
        line = trim(line);
        if (line.empty())
        {
            continue;
        }

        std::istringstream row(line);
        std::string object;
        std::string type;
        long long size = 0;
        row >> object >> type >> size;

        std::string path;
        std::getline(row, path);
        path = trim(path);

        std::cout << std::left << std::setw(12) << human_size(size)
                  << std::setw(12) << type
                  << std::setw(41) << object
                  << path << "\n";
    }

    return 0;
}
