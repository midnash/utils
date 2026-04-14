#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " TOKEN\n"
        << "\n"
        << "Decode and pretty-print JWT header/payload (no signature validation).\n";
}

static int b64url_value(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 26;
    if (c >= '0' && c <= '9')
        return c - '0' + 52;
    if (c == '-')
        return 62;
    if (c == '_')
        return 63;
    return -1;
}

static bool decode_b64url(const std::string &in, std::string &out)
{
    std::string s = in;
    std::size_t mod = s.size() % 4;
    if (mod == 2)
        s += "==";
    else if (mod == 3)
        s += "=";
    else if (mod == 1)
        return false;

    out.clear();
    for (std::size_t i = 0; i < s.size(); i += 4)
    {
        int v0 = b64url_value(s[i]);
        int v1 = b64url_value(s[i + 1]);
        int v2 = (s[i + 2] == '=') ? -2 : b64url_value(s[i + 2]);
        int v3 = (s[i + 3] == '=') ? -2 : b64url_value(s[i + 3]);

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
            out.push_back(static_cast<char>((block >> 8) & 0xFF));
        if (v3 != -2)
            out.push_back(static_cast<char>(block & 0xFF));
    }

    return true;
}

static std::string pretty_json(const std::string &raw)
{
    std::ostringstream out;
    int indent = 0;
    bool in_string = false;
    bool escape = false;

    for (char c : raw)
    {
        if (in_string)
        {
            out << c;
            if (escape)
                escape = false;
            else if (c == '\\')
                escape = true;
            else if (c == '"')
                in_string = false;
            continue;
        }

        if (c == '"')
        {
            in_string = true;
            out << c;
            continue;
        }

        if (c == '{' || c == '[')
        {
            out << c << '\n';
            ++indent;
            out << std::string(indent * 2, ' ');
            continue;
        }
        if (c == '}' || c == ']')
        {
            out << '\n';
            indent = std::max(0, indent - 1);
            out << std::string(indent * 2, ' ') << c;
            continue;
        }
        if (c == ',')
        {
            out << c << '\n' << std::string(indent * 2, ' ');
            continue;
        }
        if (c == ':')
        {
            out << ": ";
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c)))
        {
            continue;
        }

        out << c;
    }

    return out.str();
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

    if (argc != 2)
    {
        print_help(argv[0]);
        return 1;
    }

    std::string token = argv[1];
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (true)
    {
        std::size_t pos = token.find('.', start);
        if (pos == std::string::npos)
        {
            parts.push_back(token.substr(start));
            break;
        }
        parts.push_back(token.substr(start, pos - start));
        start = pos + 1;
    }

    if (parts.size() < 2)
    {
        std::cerr << "jwt: invalid token format\n";
        return 1;
    }

    std::string header;
    std::string payload;
    if (!decode_b64url(parts[0], header) || !decode_b64url(parts[1], payload))
    {
        std::cerr << "jwt: failed to decode token\n";
        return 1;
    }

    std::cout << "header:\n" << pretty_json(header) << "\n\n";
    std::cout << "payload:\n" << pretty_json(payload) << "\n";

    if (parts.size() >= 3)
    {
        std::cout << "\nsignature: " << parts[2].size() << " base64url chars\n";
        std::cout << "(not validated)\n";
    }

    return 0;
}
