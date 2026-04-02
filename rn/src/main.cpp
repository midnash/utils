#include <filesystem>
#include <fnmatch.h>
#include <iostream>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

struct Config
{
    bool dry_run = false;
    bool force = false;
};

struct Substitution
{
    char delim = '/';
    std::string pattern;
    std::string replacement;
    bool global = false;
    bool ignore_case = false;
};

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " [OPTIONS] 's/pattern/replacement/[flags]' FILE...\n"
        << "\n"
        << "Bulk rename files using a regex substitution.\n"
        << "\n"
        << "Options:\n"
        << "  -n, --dry-run    Show what would be renamed without changing files\n"
        << "  -f, --force      Overwrite destination if it already exists\n"
        << "  -h, --help       Show this help\n"
        << "\n"
        << "Flags:\n"
        << "  g                Replace all matches in each filename\n"
        << "  i                Case-insensitive regex matching\n"
        << "\n"
        << "Example:\n"
        << "  rn 's/\\.jpeg$/.jpg/' *.jpeg\n";
}

static bool is_escaped(const std::string &s, std::size_t pos)
{
    if (pos == 0)
    {
        return false;
    }
    std::size_t backslashes = 0;
    for (std::size_t i = pos; i > 0; --i)
    {
        if (s[i - 1] != '\\')
        {
            break;
        }
        ++backslashes;
    }
    return (backslashes % 2) == 1;
}

static std::optional<Substitution> parse_substitution(std::string_view expr)
{
    if (expr.size() < 4 || expr[0] != 's')
    {
        return std::nullopt;
    }

    Substitution sub;
    sub.delim = expr[1];
    if (sub.delim == '\0')
    {
        return std::nullopt;
    }

    auto read_field = [&](std::size_t start, std::string &out) -> std::optional<std::size_t>
    {
        std::string source(expr);
        for (std::size_t i = start; i < source.size(); ++i)
        {
            if (source[i] == sub.delim && !is_escaped(source, i))
            {
                out = source.substr(start, i - start);
                return i + 1;
            }
        }
        return std::nullopt;
    };

    auto after_pat = read_field(2, sub.pattern);
    if (!after_pat)
    {
        return std::nullopt;
    }
    auto after_repl = read_field(*after_pat, sub.replacement);
    if (!after_repl)
    {
        return std::nullopt;
    }

    for (std::size_t i = *after_repl; i < expr.size(); ++i)
    {
        char f = expr[i];
        if (f == 'g')
        {
            sub.global = true;
        }
        else if (f == 'i')
        {
            sub.ignore_case = true;
        }
        else
        {
            return std::nullopt;
        }
    }

    return sub;
}

static std::optional<std::regex> build_regex(const Substitution &sub)
{
    auto flags = std::regex::ECMAScript | std::regex::optimize;
    if (sub.ignore_case)
    {
        flags |= std::regex::icase;
    }

    try
    {
        return std::regex(sub.pattern, flags);
    }
    catch (const std::regex_error &e)
    {
        std::cerr << "rn: invalid regex: " << e.what() << '\n';
        return std::nullopt;
    }
}

static std::string rewrite_name(const std::string &name, const std::regex &rx, const Substitution &sub)
{
    if (sub.global)
    {
        return std::regex_replace(name, rx, sub.replacement);
    }
    return std::regex_replace(name, rx, sub.replacement, std::regex_constants::format_first_only);
}

static bool has_glob_meta(std::string_view s)
{
    return s.find_first_of("*?[") != std::string_view::npos;
}

static std::vector<fs::path> expand_input_path(const std::string &raw)
{
    fs::path p(raw);
    std::string filename = p.filename().string();

    if (!has_glob_meta(filename))
    {
        return {p};
    }

    fs::path dir = p.parent_path();
    if (dir.empty())
    {
        dir = ".";
    }

    std::vector<fs::path> out;
    std::error_code ec;
    for (const auto &entry : fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec))
    {
        if (ec)
        {
            break;
        }

        const std::string name = entry.path().filename().string();
        if (fnmatch(filename.c_str(), name.c_str(), FNM_PERIOD) == 0)
        {
            if (p.has_parent_path())
            {
                out.push_back(p.parent_path() / name);
            }
            else
            {
                out.push_back(name);
            }
        }
    }

    std::sort(out.begin(), out.end());
    return out;
}

int main(int argc, char **argv)
{
    Config cfg;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            print_help(argv[0]);
            return 0;
        }
        if (arg == "-n" || arg == "--dry-run")
        {
            cfg.dry_run = true;
            continue;
        }
        if (arg == "-f" || arg == "--force")
        {
            cfg.force = true;
            continue;
        }
        if (!arg.empty() && arg[0] == '-')
        {
            std::cerr << "rn: unknown option '" << arg << "'\n";
            return 1;
        }
        positional.emplace_back(arg);
    }

    if (positional.size() < 2)
    {
        print_help(argv[0]);
        return 1;
    }

    auto sub = parse_substitution(positional[0]);
    if (!sub)
    {
        std::cerr << "rn: substitution must look like s/pattern/replacement/[flags]\n";
        return 1;
    }

    auto rx = build_regex(*sub);
    if (!rx)
    {
        return 1;
    }

    int renamed = 0;
    int skipped = 0;
    int failed = 0;

    std::vector<fs::path> inputs;
    for (std::size_t i = 1; i < positional.size(); ++i)
    {
        auto expanded = expand_input_path(positional[i]);
        if (expanded.empty())
        {
            std::cerr << "rn: no matches for pattern: " << positional[i] << '\n';
            ++failed;
            continue;
        }
        inputs.insert(inputs.end(), expanded.begin(), expanded.end());
    }

    for (const auto &src : inputs)
    {
        std::error_code ec;

        if (!fs::exists(src, ec) || ec)
        {
            std::cerr << "rn: not found: " << src.string() << '\n';
            ++failed;
            continue;
        }
        if (!fs::is_regular_file(src, ec) || ec)
        {
            std::cerr << "rn: not a regular file: " << src.string() << '\n';
            ++failed;
            continue;
        }

        std::string old_name = src.filename().string();
        std::string new_name = rewrite_name(old_name, *rx, *sub);

        if (new_name == old_name)
        {
            ++skipped;
            continue;
        }

        fs::path dst = src.parent_path() / new_name;
        if (!cfg.force && fs::exists(dst, ec) && !ec)
        {
            std::cerr << "rn: destination exists (use --force): " << dst.string() << '\n';
            ++failed;
            continue;
        }

        std::cout << src.string() << " -> " << dst.string() << '\n';

        if (!cfg.dry_run)
        {
            fs::rename(src, dst, ec);
            if (ec)
            {
                std::cerr << "rn: rename failed: " << ec.message() << '\n';
                ++failed;
                continue;
            }
        }
        ++renamed;
    }

    if (cfg.dry_run)
    {
        std::cout << "\nDry run: " << renamed << " rename(s), " << skipped << " unchanged, " << failed << " failed\n";
    }
    else
    {
        std::cout << "\nDone: " << renamed << " rename(s), " << skipped << " unchanged, " << failed << " failed\n";
    }

    return failed == 0 ? 0 : 1;
}
