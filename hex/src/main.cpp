#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

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
        {
            return s;
        }
        return std::string(code) + s + "\033[0m";
    }

    static std::string null_b(const std::string &s) { return wrap(s, "\033[1;31m"); }
    static std::string printable_b(const std::string &s) { return wrap(s, "\033[1;32m"); }
    static std::string high_b(const std::string &s) { return wrap(s, "\033[1;33m"); }
    static std::string normal_b(const std::string &s) { return s; }
}

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " [OPTIONS] FILE\n"
        << "\n"
        << "Readable hex dump with grouped bytes and ASCII column.\n"
        << "\n"
        << "Options:\n"
        << "  -w, --width <N>  Bytes per row [default: 16]\n"
        << "  -h, --help       Show this help\n";
}

static std::string byte_hex(unsigned char b)
{
    std::ostringstream oss;
    oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    return oss.str();
}

static std::string color_hex(unsigned char b)
{
    std::string hx = byte_hex(b);
    if (b == 0x00)
    {
        return col::null_b(hx);
    }
    if (std::isprint(static_cast<unsigned char>(b)))
    {
        return col::printable_b(hx);
    }
    if (b >= 0x80)
    {
        return col::high_b(hx);
    }
    return col::normal_b(hx);
}

static std::string color_ascii(unsigned char b)
{
    char c = std::isprint(static_cast<unsigned char>(b)) ? static_cast<char>(b) : '.';
    std::string s(1, c);

    if (b == 0x00)
    {
        return col::null_b(s);
    }
    if (std::isprint(static_cast<unsigned char>(b)))
    {
        return col::printable_b(s);
    }
    if (b >= 0x80)
    {
        return col::high_b(s);
    }
    return col::normal_b(s);
}

int main(int argc, char **argv)
{
    int width = 16;
    fs::path file;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            print_help(argv[0]);
            return 0;
        }
        if (arg == "-w" || arg == "--width")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "hex: " << arg << " requires a value\n";
                return 1;
            }
            try
            {
                width = std::stoi(argv[++i]);
            }
            catch (...)
            {
                std::cerr << "hex: width must be an integer\n";
                return 1;
            }
            if (width <= 0 || width > 64)
            {
                std::cerr << "hex: width must be between 1 and 64\n";
                return 1;
            }
            continue;
        }
        if (!arg.empty() && arg[0] == '-')
        {
            std::cerr << "hex: unknown option '" << arg << "'\n";
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
        std::cerr << "hex: cannot open file: " << file.string() << "\n";
        return 1;
    }

    col::init();

    std::vector<unsigned char> buf(static_cast<std::size_t>(width));
    std::size_t offset = 0;

    while (in)
    {
        in.read(reinterpret_cast<char *>(buf.data()), width);
        std::streamsize n = in.gcount();
        if (n <= 0)
        {
            break;
        }

        std::cout << std::hex << std::setw(8) << std::setfill('0') << offset << "  ";
        std::cout << std::dec << std::setfill(' ');

        for (int i = 0; i < width; ++i)
        {
            if (i < n)
            {
                std::cout << color_hex(buf[static_cast<std::size_t>(i)]) << ' ';
            }
            else
            {
                std::cout << "   ";
            }

            if (i == (width / 2) - 1)
            {
                std::cout << ' ';
            }
        }

        std::cout << " |";
        for (int i = 0; i < n; ++i)
        {
            std::cout << color_ascii(buf[static_cast<std::size_t>(i)]);
        }
        std::cout << "|\n";

        offset += static_cast<std::size_t>(n);
    }

    return 0;
}
