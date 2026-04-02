#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

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
    return trim(out);
}

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " [REPO_PATH]\n"
        << "\n"
        << "Print summary stats for a git repository.\n"
        << "\n"
        << "Includes:\n"
        << "  - total commits\n"
        << "  - contributor count\n"
        << "  - first/last commit dates\n"
        << "  - most changed files\n";
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

    fs::path repo = ".";
    if (argc >= 2)
    {
        repo = argv[1];
    }
    if (argc > 2)
    {
        std::cerr << "git-summary: too many arguments\n";
        return 1;
    }

    std::string qrepo = shell_quote(fs::absolute(repo).string());
    std::string check = run_cmd("cd " + qrepo + " && git rev-parse --is-inside-work-tree 2>/dev/null");
    if (check != "true")
    {
        std::cerr << "git-summary: not a git repository: " << repo.string() << "\n";
        return 1;
    }

    std::string total_commits = run_cmd("cd " + qrepo + " && git rev-list --count --all");
    std::string contributors = run_cmd("cd " + qrepo + " && git shortlog -sne --all | wc -l");
    std::string first_date = run_cmd("cd " + qrepo + " && git log --reverse --date=short --format=%ad --all | head -n 1");
    std::string last_date = run_cmd("cd " + qrepo + " && git log -1 --date=short --format=%ad --all");
    std::string top_files = run_cmd(
        "cd " + qrepo +
        " && git log --name-only --pretty=format: --all "
        "| sed '/^$/d' | sort | uniq -c | sort -nr | head -n 10");

    std::cout << "Repository: " << fs::absolute(repo).string() << "\n";
    std::cout << "Total commits: " << (total_commits.empty() ? "0" : total_commits) << "\n";
    std::cout << "Contributors: " << (contributors.empty() ? "0" : contributors) << "\n";
    std::cout << "First commit: " << (first_date.empty() ? "n/a" : first_date) << "\n";
    std::cout << "Last commit: " << (last_date.empty() ? "n/a" : last_date) << "\n";

    std::cout << "\nMost changed files:\n";
    if (top_files.empty())
    {
        std::cout << "  (no file history found)\n";
    }
    else
    {
        std::istringstream iss(top_files);
        std::string line;
        while (std::getline(iss, line))
        {
            if (!trim(line).empty())
            {
                std::cout << "  " << line << "\n";
            }
        }
    }

    return 0;
}
