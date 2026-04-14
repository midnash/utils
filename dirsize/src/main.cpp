#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

struct Row
{
    fs::path path;
    uintmax_t size = 0;
};

namespace col
{
    static bool enabled = false;

    static void init()
    {
        enabled = isatty(STDOUT_FILENO) && !std::getenv("NO_COLOR");
    }

    static std::string wrap(const std::string &s, const char *code)
    {
        if (!enabled)
            return s;
        return std::string(code) + s + "\033[0m";
    }

    static std::string dir(const std::string &s) { return wrap(s, "\033[1;34m"); }
    static std::string bar(const std::string &s) { return wrap(s, "\033[1;32m"); }
}

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " [OPTIONS] [PATH]\n"
        << "\n"
        << "Show immediate child directory/file sizes sorted descending.\n"
        << "\n"
        << "Options:\n"
        << "  -a, --all         Include hidden entries\n"
    << "  -z, --zero        Include zero-size entries\n"
        << "  -n, --top <N>     Show top N entries (default: all)\n"
    << "  -w, --width <N>   Bar width in characters (default: 36)\n"
        << "  -h, --help        Show this help\n";
}

static std::string human_size(uintmax_t n)
{
    static const char *units[] = {"B", "K", "M", "G", "T", "P"};
    double v = static_cast<double>(n);
    std::size_t u = 0;
    while (v >= 1024.0 && u + 1 < (sizeof(units) / sizeof(units[0])))
    {
        v /= 1024.0;
        ++u;
    }

    std::ostringstream oss;
    if (u == 0)
        oss << static_cast<uintmax_t>(v) << units[u];
    else
        oss << std::fixed << std::setprecision(1) << v << units[u];
    return oss.str();
}

static uintmax_t path_size(const fs::path &p)
{
    std::error_code ec;
    if (fs::is_regular_file(p, ec))
        return fs::file_size(p, ec);
    if (!fs::is_directory(p, ec))
        return 0;

    uintmax_t total = 0;
    for (const auto &e : fs::recursive_directory_iterator(p, fs::directory_options::skip_permission_denied, ec))
    {
        if (ec)
            break;
        if (e.is_regular_file(ec) && !ec)
        {
            total += e.file_size(ec);
            ec.clear();
        }
    }
    return total;
}

int main(int argc, char **argv)
{
    bool show_hidden = false;
    bool include_zero = false;
    int top_n = -1;
    int bar_width = 36;
    fs::path root = ".";

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            print_help(argv[0]);
            return 0;
        }
        if (arg == "-a" || arg == "--all")
        {
            show_hidden = true;
            continue;
        }
        if (arg == "-z" || arg == "--zero")
        {
            include_zero = true;
            continue;
        }
        if (arg == "-n" || arg == "--top")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "dirsize: " << arg << " requires a value\n";
                return 1;
            }
            try
            {
                top_n = std::stoi(argv[++i]);
                if (top_n <= 0)
                    throw std::invalid_argument("non-positive");
            }
            catch (...)
            {
                std::cerr << "dirsize: --top must be a positive integer\n";
                return 1;
            }
            continue;
        }
        if (arg == "-w" || arg == "--width")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "dirsize: " << arg << " requires a value\n";
                return 1;
            }
            try
            {
                bar_width = std::stoi(argv[++i]);
                if (bar_width <= 0 || bar_width > 120)
                    throw std::invalid_argument("range");
            }
            catch (...)
            {
                std::cerr << "dirsize: --width must be an integer in [1, 120]\n";
                return 1;
            }
            continue;
        }
        if (!arg.empty() && arg[0] == '-')
        {
            std::cerr << "dirsize: unknown option '" << arg << "'\n";
            return 1;
        }
        root = arg;
    }

    if (!fs::exists(root) || !fs::is_directory(root))
    {
        std::cerr << "dirsize: not a directory: " << root.string() << "\n";
        return 1;
    }

    std::vector<Row> rows;
    std::error_code ec;
    std::size_t scanned = 0;
    for (const auto &e : fs::directory_iterator(root, fs::directory_options::skip_permission_denied, ec))
    {
        if (ec)
            break;
        ++scanned;

        const std::string name = e.path().filename().string();
        if (!show_hidden && !name.empty() && name[0] == '.')
            continue;

        uintmax_t size = path_size(e.path());
        if (!include_zero && size == 0)
            continue;

        rows.push_back({e.path(), size});
    }

    std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b)
              { return a.size > b.size; });

    if (top_n > 0 && static_cast<int>(rows.size()) > top_n)
        rows.resize(static_cast<std::size_t>(top_n));

    uintmax_t max_size = 0;
    uintmax_t sum_size = 0;
    for (const auto &r : rows)
    {
        max_size = std::max(max_size, r.size);
        sum_size += r.size;
    }

    col::init();

    std::cout << std::right << std::setw(9) << "SIZE"
              << "  " << std::setw(6) << "SHARE"
              << "  " << std::left << std::setw(bar_width) << "BAR"
              << " NAME\n";

    std::cout << std::string(static_cast<std::size_t>(9 + 2 + 6 + 2 + bar_width + 5), '-') << "\n";

    for (const auto &r : rows)
    {
        int bar_w = 0;
        if (max_size > 0)
            bar_w = static_cast<int>((static_cast<double>(bar_width) * static_cast<double>(r.size)) / static_cast<double>(max_size));
        std::string bar(static_cast<std::size_t>(bar_w), '#');

        std::string name = r.path.filename().string();
        if (fs::is_directory(r.path))
            name = col::dir(name + "/");

        double pct = (sum_size == 0) ? 0.0 : (100.0 * static_cast<double>(r.size) / static_cast<double>(sum_size));

        std::ostringstream pct_ss;
        pct_ss << std::fixed << std::setprecision(1) << pct << "%";

        std::cout << std::right << std::setw(9) << human_size(r.size) << "  "
                  << std::setw(6) << pct_ss.str() << "  "
                  << std::left << std::setw(bar_width) << col::bar(bar)
                  << " " << name << "\n";
    }

    std::cout << "\n"
              << "shown " << rows.size() << " of " << scanned
              << " entries, total " << human_size(sum_size) << "\n";

    return 0;
}
