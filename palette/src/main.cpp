#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <unistd.h>

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " [OPTIONS]\n"
        << "\n"
        << "Print terminal color palettes (256-color and truecolor).\n"
        << "\n"
        << "Options:\n"
        << "      --256         Show 256-color palette only\n"
        << "      --truecolor   Show truecolor gradient only\n"
        << "      --codes       Also print escape code references\n"
        << "  -h, --help        Show this help\n";
}

static void print_256_grid(bool show_codes)
{
    std::cout << "256-color palette\n";
    for (int row = 0; row < 16; ++row)
    {
        for (int col = 0; col < 16; ++col)
        {
            int n = row * 16 + col;
            std::cout << "\033[48;5;" << n << "m"
                      << std::setw(4) << n
                      << "\033[0m";
            if (col != 15)
                std::cout << ' ';
        }
        std::cout << '\n';
    }

    if (show_codes)
    {
        std::cout << "\nEscape codes:\n";
        std::cout << "  foreground: ESC[38;5;<n>m\n";
        std::cout << "  background: ESC[48;5;<n>m\n";
        std::cout << "  reset:      ESC[0m\n";
    }
}

static void print_truecolor(bool show_codes)
{
    std::cout << "truecolor gradient\n";

    // Horizontal rainbow-ish gradient.
    for (int i = 0; i < 72; ++i)
    {
        int r = (i < 36) ? 255 : (255 - (i - 36) * 7);
        int g = (i < 36) ? (i * 7) : 255;
        int b = (i < 36) ? 0 : ((i - 36) * 7);
        if (r < 0)
            r = 0;
        if (g < 0)
            g = 0;
        if (b < 0)
            b = 0;
        if (r > 255)
            r = 255;
        if (g > 255)
            g = 255;
        if (b > 255)
            b = 255;

        std::cout << "\033[48;2;" << r << ';' << g << ';' << b << "m  \033[0m";
    }
    std::cout << '\n';

    // Grayscale ramp.
    for (int i = 0; i < 72; ++i)
    {
        int c = static_cast<int>((255.0 * i) / 71.0);
        std::cout << "\033[48;2;" << c << ';' << c << ';' << c << "m  \033[0m";
    }
    std::cout << '\n';

    if (show_codes)
    {
        std::cout << "\nEscape codes:\n";
        std::cout << "  foreground: ESC[38;2;<r>;<g>;<b>m\n";
        std::cout << "  background: ESC[48;2;<r>;<g>;<b>m\n";
        std::cout << "  reset:      ESC[0m\n";
    }
}

int main(int argc, char **argv)
{
    bool show_256 = true;
    bool show_truecolor = true;
    bool show_codes = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            print_help(argv[0]);
            return 0;
        }
        if (arg == "--256")
        {
            show_256 = true;
            show_truecolor = false;
            continue;
        }
        if (arg == "--truecolor")
        {
            show_truecolor = true;
            show_256 = false;
            continue;
        }
        if (arg == "--codes")
        {
            show_codes = true;
            continue;
        }

        std::cerr << "palette: unknown option '" << arg << "'\n";
        return 1;
    }

    if (!isatty(STDOUT_FILENO) && !std::getenv("FORCE_COLOR"))
    {
        std::cerr << "palette: stdout is not a TTY (set FORCE_COLOR=1 to force output)\n";
    }

    if (show_256)
    {
        print_256_grid(show_codes);
        if (show_truecolor)
            std::cout << '\n';
    }
    if (show_truecolor)
    {
        print_truecolor(show_codes);
    }

    return 0;
}
