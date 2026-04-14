#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " [OPTIONS] [VALUE ...]\n"
        << "\n"
        << "Convert between Unix epoch timestamps and human-readable dates.\n"
        << "If no VALUE is given, lines are read from stdin.\n"
        << "\n"
        << "Options:\n"
        << "  -e, --to-epoch     Parse date/time input and print epoch seconds\n"
        << "  -d, --from-epoch   Parse epoch seconds and print formatted date/time\n"
        << "  -u, --utc          Use UTC (default: local time)\n"
        << "  -f, --format <F>   strftime/strptime format [default: %Y-%m-%d %H:%M:%S]\n"
        << "  -h, --help         Show this help\n"
        << "\n"
        << "Examples:\n"
        << "  epoch 1713043200\n"
        << "  epoch --to-epoch '2026-04-13 12:00:00'\n"
        << "  printf '1713043200\\n' | epoch --utc\n";
}

static std::string trim(std::string s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
    {
        s.pop_back();
    }
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])))
    {
        ++i;
    }
    return s.substr(i);
}

static bool is_integer(const std::string &s)
{
    if (s.empty())
    {
        return false;
    }
    std::size_t i = (s[0] == '-' || s[0] == '+') ? 1 : 0;
    if (i >= s.size())
    {
        return false;
    }
    for (; i < s.size(); ++i)
    {
        if (!std::isdigit(static_cast<unsigned char>(s[i])))
        {
            return false;
        }
    }
    return true;
}

static bool parse_time_str(const std::string &input, const std::string &fmt, bool utc, std::time_t &out)
{
    std::tm tm{};
    std::istringstream iss(input);
    iss >> std::get_time(&tm, fmt.c_str());
    if (!iss.fail())
    {
        out = utc ? timegm(&tm) : std::mktime(&tm);
        return out != static_cast<std::time_t>(-1);
    }

    const std::vector<std::string> fallbacks = {
        "%Y-%m-%dT%H:%M:%S",
        "%Y-%m-%d %H:%M:%S",
        "%Y-%m-%d",
        "%Y/%m/%d %H:%M:%S",
        "%Y/%m/%d"};

    for (const auto &f : fallbacks)
    {
        std::tm t2{};
        std::istringstream is2(input);
        is2 >> std::get_time(&t2, f.c_str());
        if (!is2.fail())
        {
            out = utc ? timegm(&t2) : std::mktime(&t2);
            return out != static_cast<std::time_t>(-1);
        }
    }

    return false;
}

static bool format_time(std::time_t t, const std::string &fmt, bool utc, std::string &out)
{
    std::tm tm{};
    if (utc)
    {
        if (!gmtime_r(&t, &tm))
        {
            return false;
        }
    }
    else
    {
        if (!localtime_r(&t, &tm))
        {
            return false;
        }
    }

    char buf[256] = {0};
    if (std::strftime(buf, sizeof(buf), fmt.c_str(), &tm) == 0)
    {
        return false;
    }
    out = buf;
    return true;
}

int main(int argc, char **argv)
{
    bool to_epoch = false;
    bool from_epoch = false;
    bool utc = false;
    std::string format = "%Y-%m-%d %H:%M:%S";
    std::vector<std::string> values;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            print_help(argv[0]);
            return 0;
        }
        if (arg == "-e" || arg == "--to-epoch")
        {
            to_epoch = true;
            continue;
        }
        if (arg == "-d" || arg == "--from-epoch")
        {
            from_epoch = true;
            continue;
        }
        if (arg == "-u" || arg == "--utc")
        {
            utc = true;
            continue;
        }
        if (arg == "-f" || arg == "--format")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "epoch: " << arg << " requires a value\n";
                return 1;
            }
            format = argv[++i];
            continue;
        }
        if (!arg.empty() && arg[0] == '-')
        {
            std::cerr << "epoch: unknown option '" << arg << "'\n";
            return 1;
        }
        values.push_back(arg);
    }

    if (to_epoch && from_epoch)
    {
        std::cerr << "epoch: choose either --to-epoch or --from-epoch\n";
        return 1;
    }

    if (values.empty())
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            line = trim(line);
            if (!line.empty())
            {
                values.push_back(line);
            }
        }
    }

    if (values.empty())
    {
        print_help(argv[0]);
        return 1;
    }

    int failed = 0;
    for (const auto &v : values)
    {
        bool infer_from_epoch = is_integer(v);
        bool use_from_epoch = from_epoch || (!to_epoch && infer_from_epoch);

        if (use_from_epoch)
        {
            try
            {
                std::time_t t = static_cast<std::time_t>(std::stoll(v));
                std::string out;
                if (!format_time(t, format, utc, out))
                {
                    throw std::runtime_error("format failure");
                }
                std::cout << out << '\n';
            }
            catch (...)
            {
                std::cerr << "epoch: invalid epoch value: " << v << '\n';
                ++failed;
            }
        }
        else
        {
            std::time_t t = 0;
            if (!parse_time_str(v, format, utc, t))
            {
                std::cerr << "epoch: could not parse datetime: " << v << '\n';
                ++failed;
                continue;
            }
            std::cout << static_cast<long long>(t) << '\n';
        }
    }

    return failed == 0 ? 0 : 1;
}
