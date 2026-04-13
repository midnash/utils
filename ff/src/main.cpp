#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <vector>
#include <unistd.h>
#include <fnmatch.h>

namespace fs = std::filesystem;

namespace col
{
    static bool enabled = false;

    void init()
    {
        enabled = isatty(STDOUT_FILENO) && !std::getenv("NO_COLOR");
        const char *term = std::getenv("TERM");
        if (term && std::string_view(term) == "dumb")
            enabled = false;
    }

    std::string dir(std::string_view s) { return enabled ? "\033[1;34m" + std::string(s) + "\033[0m" : std::string(s); }
    std::string link(std::string_view s) { return enabled ? "\033[1;36m" + std::string(s) + "\033[0m" : std::string(s); }
    std::string exec_(std::string_view s) { return enabled ? "\033[1;32m" + std::string(s) + "\033[0m" : std::string(s); }
    std::string file(std::string_view s) { return std::string(s); }
}

struct Config
{
    fs::path root = ".";
    std::string pattern = "";
    bool use_regex = false;
    bool ignore_case = false;
    char type_filter = 0;       // 'f'=file, 'd'=dir, 'l'=symlink, 'x'=exec
    std::string extension = ""; // without dot, e.g. "cpp"
    bool show_hidden = false;
    int max_depth = -1; // -1 = unlimited
    bool no_ignore = false;
    bool absolute = false;
    bool count_only = false;
    bool null_sep = false; // for xargs -0
};

struct IgnorePattern
{
    std::string pattern;
    bool negate = false;
    bool dir_only = false;
    bool anchored = false; // pattern contains '/', match against full relative path
};

class GitIgnore
{
public:
    void load(const fs::path &path)
    {
        std::ifstream f(path);
        if (!f)
            return;
        base_ = path.parent_path();

        std::string line;
        while (std::getline(f, line))
        {
            while (!line.empty() && (line.back() == ' ' || line.back() == '\r'))
                line.pop_back();

            if (line.empty() || line[0] == '#')
                continue;

            IgnorePattern p;
            p.negate = (line[0] == '!');
            if (p.negate)
                line = line.substr(1);

            p.dir_only = (line.back() == '/');
            if (p.dir_only)
                line.pop_back();

            if (line.empty())
                continue;

            std::string_view sv = line;
            if (sv.front() == '/')
                sv.remove_prefix(1);
            p.anchored = (sv.find('/') != std::string_view::npos) || (line.front() == '/');
            if (line.front() == '/')
                line = line.substr(1);

            p.pattern = line;
            patterns_.push_back(std::move(p));
        }
        loaded_ = true;
    }

    bool is_ignored(const fs::path &abs_path, bool is_dir) const
    {
        if (!loaded_ || patterns_.empty())
            return false;

        std::error_code ec;
        std::string rel = fs::relative(abs_path, base_, ec).string();
        if (ec)
            return false;
        std::string name = abs_path.filename().string();

        bool ignored = false;
        for (const auto &p : patterns_)
        {
            if (p.dir_only && !is_dir)
                continue;

            bool matched = false;
            if (p.anchored)
            {
                matched = (fnmatch(p.pattern.c_str(), rel.c_str(), FNM_PATHNAME) == 0);
            }
            else
            {
                matched = (fnmatch(p.pattern.c_str(), name.c_str(), 0) == 0);
                if (!matched)
                    matched = (fnmatch(p.pattern.c_str(), rel.c_str(), FNM_PATHNAME) == 0);
            }

            if (matched)
                ignored = !p.negate;
        }
        return ignored;
    }

private:
    std::vector<IgnorePattern> patterns_;
    fs::path base_;
    bool loaded_ = false;
};

class Matcher
{
public:
    explicit Matcher(const Config &cfg) : cfg_(cfg)
    {
        if (cfg_.use_regex && !cfg_.pattern.empty())
        {
            auto flags = std::regex::ECMAScript | std::regex::optimize;
            if (cfg_.ignore_case)
                flags |= std::regex::icase;
            try
            {
                regex_ = std::regex(cfg_.pattern, flags);
            }
            catch (const std::regex_error &e)
            {
                std::cerr << "ff: invalid regex: " << e.what() << '\n';
                std::exit(1);
            }
        }
    }

    bool matches_name(const std::string &name) const
    {
        if (cfg_.pattern.empty())
            return true;

        if (cfg_.use_regex)
        {
            return std::regex_search(name, *regex_);
        }

        if (cfg_.ignore_case)
        {
            std::string lo_name = name, lo_pat = cfg_.pattern;
            std::transform(lo_name.begin(), lo_name.end(), lo_name.begin(), ::tolower);
            std::transform(lo_pat.begin(), lo_pat.end(), lo_pat.begin(), ::tolower);
            return lo_name.find(lo_pat) != std::string::npos;
        }

        return name.find(cfg_.pattern) != std::string::npos;
    }

    bool matches_extension(const fs::path &path) const
    {
        if (cfg_.extension.empty())
            return true;
        std::string ext = path.extension().string();
        if (!ext.empty() && ext[0] == '.')
            ext = ext.substr(1);

        if (cfg_.ignore_case)
        {
            std::string lo_ext = ext, lo_want = cfg_.extension;
            std::transform(lo_ext.begin(), lo_ext.end(), lo_ext.begin(), ::tolower);
            std::transform(lo_want.begin(), lo_want.end(), lo_want.begin(), ::tolower);
            return lo_ext == lo_want;
        }
        return ext == cfg_.extension;
    }

    bool matches_type(const fs::path &path, const fs::file_status &st, bool is_link) const
    {
        if (cfg_.type_filter == 0)
            return true;
        switch (cfg_.type_filter)
        {
        case 'f':
            return fs::is_regular_file(st) && !is_link;
        case 'd':
            return fs::is_directory(st);
        case 'l':
            return is_link;
        case 'x':
            return fs::is_regular_file(st) && (access(path.c_str(), X_OK) == 0);
        }
        return true;
    }

private:
    const Config &cfg_;
    std::optional<std::regex> regex_;
};

class Walker
{
public:
    explicit Walker(const Config &cfg) : cfg_(cfg), matcher_(cfg)
    {
        if (!cfg_.no_ignore)
        {
            auto gi = cfg_.root / ".gitignore";
            if (fs::exists(gi))
                ignore_.load(gi);
        }
    }

    long run()
    {
        long count = 0;
        walk(cfg_.root, 0, count);
        return count;
    }

private:
    void walk(const fs::path &dir_path, int depth, long &count)
    {
        if (cfg_.max_depth >= 0 && depth > cfg_.max_depth)
            return;

        std::error_code ec;
        fs::directory_iterator it(dir_path, fs::directory_options::skip_permission_denied, ec);
        if (ec)
            return;

        std::vector<fs::directory_entry> entries;
        for (auto &e : it)
            entries.push_back(e);
        std::sort(entries.begin(), entries.end(),
                  [](const auto &a, const auto &b)
                  {
                      return a.path().filename().string() < b.path().filename().string();
                  });

        for (const auto &entry : entries)
        {
            std::error_code sec;
            auto lstatus = entry.symlink_status(sec);
            if (sec)
                continue;

            const bool is_link = fs::is_symlink(lstatus);
            const bool is_dir = fs::is_directory(lstatus);
            const std::string name = entry.path().filename().string();

            if (!cfg_.show_hidden && !name.empty() && name[0] == '.')
                continue;

            if (!cfg_.no_ignore && ignore_.is_ignored(entry.path(), is_dir))
                continue;

            fs::file_status effective = lstatus;
            if (is_link)
            {
                effective = entry.status(sec);
                if (sec)
                    effective = lstatus; // broken symlink — use lstat
            }

            const bool matched =
                matcher_.matches_name(name) &&
                matcher_.matches_extension(entry.path()) &&
                matcher_.matches_type(entry.path(), effective, is_link);

            if (matched)
            {
                print(entry.path(), effective, is_link);
                ++count;
            }

            if (is_dir && !is_link)
            {
                walk(entry.path(), depth + 1, count);
            }
        }
    }

    void print(const fs::path &path, const fs::file_status &st, bool is_link) const
    {
        if (cfg_.count_only)
            return;

        std::string s;
        if (cfg_.absolute)
        {
            std::error_code ec;
            s = fs::canonical(path, ec).string();
            if (ec)
                s = fs::absolute(path).string();
        }
        else
        {
            s = path.string();
            if (s.size() > 2 && s[0] == '.' && s[1] == '/')
                s = s.substr(2);
        }

        std::string colored;
        if (is_link)
            colored = col::link(s);
        else if (fs::is_directory(st))
            colored = col::dir(s);
        else if (fs::is_regular_file(st) && access(path.c_str(), X_OK) == 0)
            colored = col::exec_(s);
        else
            colored = col::file(s);

        std::cout << colored;
        std::cout.put(cfg_.null_sep ? '\0' : '\n');
    }

    const Config &cfg_;
    Matcher matcher_;
    GitIgnore ignore_;
};

static void print_help(const char *prog)
{
    std::cout << "Usage: " << prog << " [OPTIONS] [PATTERN] [PATH]\n"
                                      "\n"
                                      "A fast, friendly file finder with sane defaults.\n"
                                      "\n"
                                      "Arguments:\n"
                                      "  PATTERN               Substring to match in filenames\n"
                                      "  PATH                  Directory to search [default: .]\n"
                                      "\n"
                                      "Options:\n"
                                      "  -r, --regex           Treat PATTERN as a regular expression\n"
                                      "  -i, --ignore-case     Case-insensitive matching\n"
                                      "  -t, --type <TYPE>     Filter by type:\n"
                                      "                          f  regular file\n"
                                      "                          d  directory\n"
                                      "                          l  symbolic link\n"
                                      "                          x  executable\n"
                                      "  -e, --extension <EXT> Filter by extension (e.g. cpp, rs, py)\n"
                                      "  -H, --hidden          Include hidden files and directories\n"
                                      "  -d, --max-depth <N>   Limit search depth (0 = immediate children)\n"
                                      "  -a, --absolute        Print absolute paths\n"
                                      "  -c, --count           Print total match count instead of paths\n"
                                      "  -0, --null            Separate results with NUL byte (for xargs -0)\n"
                                      "      --no-ignore       Ignore .gitignore rules\n"
                                      "  -h, --help            Show this help\n"
                                      "\n"
                                      "Examples:\n"
                                      "  ff                    List all (non-hidden) files recursively\n"
                                      "  ff main               Find files whose name contains 'main'\n"
                                      "  ff -e cpp             Find all .cpp files\n"
                                      "  ff -t d src           Find directories whose name contains 'src'\n"
                                      "  ff -r '\\.test\\.'      Use a regex pattern\n"
                                      "  ff -H -d 1            List all files one level deep, including hidden\n"
                                      "  ff -t x /usr/bin      Find executables in /usr/bin\n"
                                      "  ff -0 .cpp | xargs -0 wc -l   Count lines in all .cpp files\n";
}

static Config parse_args(int argc, char **argv)
{
    Config cfg;
    std::vector<std::string> positional;

    auto expand_tilde = [](const std::string &p) -> fs::path
    {
        if (!p.empty() && p[0] == '~')
        {
            const char *home = std::getenv("HOME");
            if (home && (p.size() == 1 || p[1] == '/'))
            {
                if (p.size() == 1)
                    return fs::path(home);
                return fs::path(home) / p.substr(2);
            }
        }
        return fs::path(p);
    };

    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];

        auto require_next = [&]() -> std::string
        {
            if (i + 1 >= argc)
            {
                std::cerr << "ff: option '" << arg << "' requires an argument\n";
                std::exit(1);
            }
            return argv[++i];
        };

        if (arg == "-h" || arg == "--help")
        {
            print_help(argv[0]);
            std::exit(0);
        }
        else if (arg == "-r" || arg == "--regex")
            cfg.use_regex = true;
        else if (arg == "-i" || arg == "--ignore-case")
            cfg.ignore_case = true;
        else if (arg == "-H" || arg == "--hidden")
            cfg.show_hidden = true;
        else if (arg == "-a" || arg == "--absolute")
            cfg.absolute = true;
        else if (arg == "-c" || arg == "--count")
            cfg.count_only = true;
        else if (arg == "-0" || arg == "--null")
            cfg.null_sep = true;
        else if (arg == "--no-ignore")
            cfg.no_ignore = true;
        else if (arg == "-t" || arg == "--type")
        {
            std::string t = require_next();
            if (t != "f" && t != "d" && t != "l" && t != "x")
            {
                std::cerr << "ff: unknown type '" << t << "' (expected f, d, l, or x)\n";
                std::exit(1);
            }
            cfg.type_filter = t[0];
        }
        else if (arg == "-e" || arg == "--extension")
        {
            cfg.extension = require_next();
            if (!cfg.extension.empty() && cfg.extension[0] == '.')
                cfg.extension = cfg.extension.substr(1);
        }
        else if (arg == "-d" || arg == "--max-depth")
        {
            try
            {
                cfg.max_depth = std::stoi(require_next());
                if (cfg.max_depth < 0)
                    throw std::invalid_argument("negative");
            }
            catch (...)
            {
                std::cerr << "ff: --max-depth requires a non-negative integer\n";
                std::exit(1);
            }
        }
        else if (!arg.empty() && arg[0] == '-')
        {
            std::cerr << "ff: unknown option '" << arg << "' (try --help)\n";
            std::exit(1);
        }
        else
        {
            positional.push_back(std::string(arg));
        }
    }

    if (positional.size() == 1)
    {
        fs::path one = expand_tilde(positional[0]);
        if (fs::is_directory(one))
            cfg.root = one;
        else
            cfg.pattern = positional[0];
    }
    else if (positional.size() >= 2)
    {
        cfg.pattern = positional[0];
        cfg.root = expand_tilde(positional[1]);
        if (!fs::is_directory(cfg.root))
        {
            std::cerr << "ff: '" << positional[1] << "' is not a directory\n";
            std::exit(1);
        }
    }
    if (positional.size() > 2)
    {
        std::cerr << "ff: too many arguments (try --help)\n";
        std::exit(1);
    }

    return cfg;
}

int main(int argc, char **argv)
{
    col::init();

    Config cfg = parse_args(argc, argv);

    std::error_code ec;
    if (!fs::exists(cfg.root, ec) || ec)
    {
        std::cerr << "ff: path '" << cfg.root.string() << "' does not exist\n";
        return 1;
    }

    Walker walker(cfg);
    long count = walker.run();

    if (cfg.count_only)
        std::cout << count << '\n';

    return (count > 0) ? 0 : 1;
}
