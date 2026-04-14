#include <cctype>
#include <iostream>
#include <string>
#include <vector>

static const char *B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " [OPTIONS] [TEXT]\n"
        << "\n"
        << "Base64 encode/decode from argument or stdin.\n"
        << "\n"
        << "Options:\n"
        << "  -d, --decode      Decode input\n"
        << "  -w, --wrap <N>    Wrap encoded output every N chars (0 = no wrap)\n"
        << "  -h, --help        Show this help\n";
}

static std::string read_all_stdin()
{
    std::string data;
    std::string line;
    while (std::getline(std::cin, line))
    {
        data += line;
        if (!std::cin.eof())
        {
            data.push_back('\n');
        }
    }
    return data;
}

static std::string encode_b64(const std::string &input, int wrap)
{
    std::string out;
    out.reserve(((input.size() + 2) / 3) * 4);

    int col = 0;
    for (std::size_t i = 0; i < input.size(); i += 3)
    {
        unsigned int v = (static_cast<unsigned char>(input[i]) << 16);
        bool b1 = (i + 1 < input.size());
        bool b2 = (i + 2 < input.size());
        if (b1)
            v |= (static_cast<unsigned char>(input[i + 1]) << 8);
        if (b2)
            v |= static_cast<unsigned char>(input[i + 2]);

        char c0 = B64[(v >> 18) & 0x3F];
        char c1 = B64[(v >> 12) & 0x3F];
        char c2 = b1 ? B64[(v >> 6) & 0x3F] : '=';
        char c3 = b2 ? B64[v & 0x3F] : '=';

        out.push_back(c0);
        out.push_back(c1);
        out.push_back(c2);
        out.push_back(c3);
        col += 4;

        if (wrap > 0 && col >= wrap)
        {
            out.push_back('\n');
            col = 0;
        }
    }

    return out;
}

static int b64_value(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 26;
    if (c >= '0' && c <= '9')
        return c - '0' + 52;
    if (c == '+')
        return 62;
    if (c == '/')
        return 63;
    return -1;
}

static bool decode_b64(const std::string &input, std::string &out)
{
    std::string clean;
    clean.reserve(input.size());
    for (char c : input)
    {
        if (!std::isspace(static_cast<unsigned char>(c)))
        {
            clean.push_back(c);
        }
    }

    if (clean.size() % 4 != 0)
    {
        return false;
    }

    out.clear();
    for (std::size_t i = 0; i < clean.size(); i += 4)
    {
        int v0 = b64_value(clean[i]);
        int v1 = b64_value(clean[i + 1]);
        int v2 = (clean[i + 2] == '=') ? -2 : b64_value(clean[i + 2]);
        int v3 = (clean[i + 3] == '=') ? -2 : b64_value(clean[i + 3]);

        if (v0 < 0 || v1 < 0 || (v2 < 0 && v2 != -2) || (v3 < 0 && v3 != -2))
        {
            return false;
        }

        unsigned int block = (static_cast<unsigned int>(v0) << 18) |
                             (static_cast<unsigned int>(v1) << 12) |
                             (static_cast<unsigned int>(v2 > 0 ? v2 : 0) << 6) |
                             static_cast<unsigned int>(v3 > 0 ? v3 : 0);

        out.push_back(static_cast<char>((block >> 16) & 0xFF));
        if (v2 != -2)
        {
            out.push_back(static_cast<char>((block >> 8) & 0xFF));
        }
        if (v3 != -2)
        {
            out.push_back(static_cast<char>(block & 0xFF));
        }
    }

    return true;
}

int main(int argc, char **argv)
{
    bool decode = false;
    int wrap = 0;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            print_help(argv[0]);
            return 0;
        }
        if (arg == "-d" || arg == "--decode")
        {
            decode = true;
            continue;
        }
        if (arg == "-w" || arg == "--wrap")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "b64: " << arg << " requires a value\n";
                return 1;
            }
            try
            {
                wrap = std::stoi(argv[++i]);
                if (wrap < 0)
                    throw std::invalid_argument("negative");
            }
            catch (...)
            {
                std::cerr << "b64: --wrap must be a non-negative integer\n";
                return 1;
            }
            continue;
        }
        if (!arg.empty() && arg[0] == '-')
        {
            std::cerr << "b64: unknown option '" << arg << "'\n";
            return 1;
        }
        positional.push_back(arg);
    }

    std::string input;
    if (positional.empty())
    {
        input = read_all_stdin();
    }
    else
    {
        for (std::size_t i = 0; i < positional.size(); ++i)
        {
            if (i)
            {
                input.push_back(' ');
            }
            input += positional[i];
        }
    }

    if (decode)
    {
        std::string out;
        if (!decode_b64(input, out))
        {
            std::cerr << "b64: invalid base64 input\n";
            return 1;
        }
        std::cout << out;
        return 0;
    }

    std::string out = encode_b64(input, wrap);
    std::cout << out;
    if (wrap == 0 && (out.empty() || out.back() != '\n'))
    {
        std::cout << '\n';
    }
    return 0;
}
