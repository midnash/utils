#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct DiffSpan
{
    std::size_t start = 0;
    std::size_t end = 0; // exclusive
};

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " [OPTIONS] FILE_A FILE_B\n"
        << "\n"
        << "Byte-level binary diff with offset context.\n"
        << "\n"
        << "Options:\n"
        << "  -c, --context <N>   Context bytes before/after each diff (default: 8)\n"
        << "  -n, --max <N>       Maximum diff spans to print (default: 100)\n"
        << "  -h, --help          Show this help\n";
}

static bool read_file(const fs::path &p, std::vector<unsigned char> &buf)
{
    std::ifstream in(p, std::ios::binary);
    if (!in)
        return false;

    in.seekg(0, std::ios::end);
    std::streamsize n = in.tellg();
    in.seekg(0, std::ios::beg);
    if (n < 0)
        return false;

    buf.resize(static_cast<std::size_t>(n));
    if (n > 0)
        in.read(reinterpret_cast<char *>(buf.data()), n);
    return in.good() || in.gcount() == n;
}

static std::string hex_slice(const std::vector<unsigned char> &b, std::size_t begin, std::size_t end)
{
    if (begin >= b.size() || begin >= end)
        return "";
    end = std::min(end, b.size());

    std::ostringstream oss;
    for (std::size_t i = begin; i < end; ++i)
    {
        if (i != begin)
            oss << ' ';
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b[i]);
    }
    return oss.str();
}

int main(int argc, char **argv)
{
    std::size_t context = 8;
    std::size_t max_spans = 100;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            print_help(argv[0]);
            return 0;
        }
        if (arg == "-c" || arg == "--context")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "bindiff: " << arg << " requires a value\n";
                return 1;
            }
            try
            {
                context = static_cast<std::size_t>(std::stoull(argv[++i]));
            }
            catch (...)
            {
                std::cerr << "bindiff: invalid context value\n";
                return 1;
            }
            continue;
        }
        if (arg == "-n" || arg == "--max")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "bindiff: " << arg << " requires a value\n";
                return 1;
            }
            try
            {
                max_spans = static_cast<std::size_t>(std::stoull(argv[++i]));
                if (max_spans == 0)
                    throw std::invalid_argument("zero");
            }
            catch (...)
            {
                std::cerr << "bindiff: --max must be a positive integer\n";
                return 1;
            }
            continue;
        }

        if (!arg.empty() && arg[0] == '-')
        {
            std::cerr << "bindiff: unknown option '" << arg << "'\n";
            return 1;
        }
        positional.push_back(arg);
    }

    if (positional.size() != 2)
    {
        print_help(argv[0]);
        return 1;
    }

    fs::path a_path = positional[0];
    fs::path b_path = positional[1];

    std::vector<unsigned char> a;
    std::vector<unsigned char> b;
    if (!read_file(a_path, a))
    {
        std::cerr << "bindiff: cannot read file: " << a_path.string() << "\n";
        return 1;
    }
    if (!read_file(b_path, b))
    {
        std::cerr << "bindiff: cannot read file: " << b_path.string() << "\n";
        return 1;
    }

    std::size_t n = std::max(a.size(), b.size());
    std::vector<DiffSpan> spans;

    bool in_span = false;
    DiffSpan cur{};
    for (std::size_t i = 0; i < n; ++i)
    {
        unsigned char av = (i < a.size()) ? a[i] : 0;
        unsigned char bv = (i < b.size()) ? b[i] : 0;
        bool different = (i >= a.size()) || (i >= b.size()) || (av != bv);

        if (different && !in_span)
        {
            in_span = true;
            cur.start = i;
            cur.end = i + 1;
        }
        else if (different && in_span)
        {
            cur.end = i + 1;
        }
        else if (!different && in_span)
        {
            spans.push_back(cur);
            in_span = false;
        }
    }
    if (in_span)
        spans.push_back(cur);

    if (spans.empty())
    {
        std::cout << "files are identical\n";
        return 0;
    }

    std::cout << "diff spans: " << spans.size() << "\n";
    std::size_t printed = 0;
    for (const auto &s : spans)
    {
        if (printed >= max_spans)
            break;

        std::size_t begin = (s.start > context) ? (s.start - context) : 0;
        std::size_t end = s.end + context;

        std::cout << "\n@0x" << std::hex << s.start << "..0x" << s.end << std::dec
                  << " (len=" << (s.end - s.start) << ")\n";
        std::cout << "A: " << hex_slice(a, begin, end) << "\n";
        std::cout << "B: " << hex_slice(b, begin, end) << "\n";
        ++printed;
    }

    if (printed < spans.size())
    {
        std::cout << "\n... " << (spans.size() - printed) << " more spans omitted (use --max)\n";
    }

    return 1;
}
