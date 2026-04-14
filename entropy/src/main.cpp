#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Block
{
    std::size_t offset = 0;
    std::size_t size = 0;
    double entropy = 0.0;
};

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " [OPTIONS] FILE\n"
        << "\n"
        << "Compute Shannon entropy per block and print a chart.\n"
        << "\n"
        << "Options:\n"
        << "  -b, --block <N>   Block size in bytes (default: 4096)\n"
        << "  -w, --width <N>   Chart width (default: 40)\n"
        << "  -h, --help        Show this help\n";
}

static double block_entropy(const unsigned char *data, std::size_t n)
{
    if (n == 0)
        return 0.0;

    std::array<std::size_t, 256> freq{};
    for (std::size_t i = 0; i < n; ++i)
        ++freq[data[i]];

    double h = 0.0;
    for (std::size_t f : freq)
    {
        if (f == 0)
            continue;
        double p = static_cast<double>(f) / static_cast<double>(n);
        h -= p * (std::log(p) / std::log(2.0));
    }
    return h;
}

int main(int argc, char **argv)
{
    std::size_t block_size = 4096;
    int width = 40;
    fs::path file;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            print_help(argv[0]);
            return 0;
        }
        if (arg == "-b" || arg == "--block")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "entropy: " << arg << " requires a value\n";
                return 1;
            }
            try
            {
                block_size = static_cast<std::size_t>(std::stoull(argv[++i]));
                if (block_size == 0)
                    throw std::invalid_argument("zero");
            }
            catch (...)
            {
                std::cerr << "entropy: --block must be a positive integer\n";
                return 1;
            }
            continue;
        }
        if (arg == "-w" || arg == "--width")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "entropy: " << arg << " requires a value\n";
                return 1;
            }
            try
            {
                width = std::stoi(argv[++i]);
                if (width <= 0 || width > 120)
                    throw std::invalid_argument("range");
            }
            catch (...)
            {
                std::cerr << "entropy: --width must be in [1, 120]\n";
                return 1;
            }
            continue;
        }
        if (!arg.empty() && arg[0] == '-')
        {
            std::cerr << "entropy: unknown option '" << arg << "'\n";
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
        std::cerr << "entropy: cannot open file: " << file.string() << "\n";
        return 1;
    }

    std::vector<unsigned char> buf(block_size);
    std::vector<Block> blocks;

    std::size_t offset = 0;
    while (in)
    {
        in.read(reinterpret_cast<char *>(buf.data()), static_cast<std::streamsize>(buf.size()));
        std::streamsize got = in.gcount();
        if (got <= 0)
            break;

        double h = block_entropy(buf.data(), static_cast<std::size_t>(got));
        blocks.push_back({offset, static_cast<std::size_t>(got), h});
        offset += static_cast<std::size_t>(got);
    }

    if (blocks.empty())
    {
        std::cerr << "entropy: empty file\n";
        return 1;
    }

    std::cout << "BLOCK\tOFFSET\tSIZE\tENTROPY\tCHART\n";
    for (std::size_t i = 0; i < blocks.size(); ++i)
    {
        const auto &b = blocks[i];
        int n = static_cast<int>((b.entropy / 8.0) * width);
        if (n < 0)
            n = 0;
        if (n > width)
            n = width;

        std::cout << i << '\t' << b.offset << '\t' << b.size << '\t'
                  << std::fixed << std::setprecision(3) << b.entropy << '\t'
                  << std::string(static_cast<std::size_t>(n), '#') << '\n';
    }

    return 0;
}
