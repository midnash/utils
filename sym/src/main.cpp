#include <cxxabi.h>
#include <elf.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
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

    static std::string wrap(std::string_view s, const char *code)
    {
        if (!enabled)
            return std::string(s);
        return std::string(code) + std::string(s) + "\033[0m";
    }

    static std::string global_s(std::string_view s) { return wrap(s, "\033[1;32m"); }
    static std::string weak_s(std::string_view s) { return wrap(s, "\033[1;33m"); }
    static std::string local_s(std::string_view s) { return wrap(s, "\033[0;37m"); }
}

struct Config
{
    bool dyn_only = false;
    bool imports_only = false;
    bool exports_only = false;
    bool include_local = true;
    bool include_global = true;
    bool include_weak = true;
    bool demangle = true;
};

struct SymbolRow
{
    std::string table;
    std::string bind;
    std::string type;
    std::string vis;
    bool undefined = false;
    uint64_t value = 0;
    uint64_t size = 0;
    std::string name;
};

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " [OPTIONS] ELF_FILE\n"
        << "\n"
        << "Readable ELF symbol table viewer with C++ demangling.\n"
        << "\n"
        << "Options:\n"
        << "  -D, --dyn           Show only dynamic symbols (.dynsym)\n"
        << "      --imports       Show only undefined/imported symbols\n"
        << "      --exports       Show only exported/defined symbols\n"
        << "  -l, --locals        Include local symbols\n"
        << "  -g, --globals       Include global symbols\n"
        << "  -w, --weaks         Include weak symbols\n"
        << "      --no-demangle   Disable C++ demangling\n"
        << "  -h, --help          Show this help\n";
}

static bool read_all(const fs::path &p, std::vector<unsigned char> &buf)
{
    std::ifstream in(p, std::ios::binary);
    if (!in)
        return false;
    in.seekg(0, std::ios::end);
    std::streamsize n = in.tellg();
    in.seekg(0, std::ios::beg);
    if (n <= 0)
        return false;
    buf.resize(static_cast<std::size_t>(n));
    in.read(reinterpret_cast<char *>(buf.data()), n);
    return in.good() || in.gcount() == n;
}

static std::string demangle_name(const std::string &name)
{
    int status = 0;
    char *dm = abi::__cxa_demangle(name.c_str(), nullptr, nullptr, &status);
    if (status == 0 && dm)
    {
        std::string s(dm);
        std::free(dm);
        return s;
    }
    if (dm)
    {
        std::free(dm);
    }
    return name;
}

static std::string bind_name(unsigned b)
{
    switch (b)
    {
    case STB_LOCAL:
        return "LOCAL";
    case STB_GLOBAL:
        return "GLOBAL";
    case STB_WEAK:
        return "WEAK";
    default:
        return "OTHER";
    }
}

static std::string type_name(unsigned t)
{
    switch (t)
    {
    case STT_NOTYPE:
        return "NOTYPE";
    case STT_OBJECT:
        return "OBJECT";
    case STT_FUNC:
        return "FUNC";
    case STT_SECTION:
        return "SECTION";
    case STT_FILE:
        return "FILE";
    case STT_TLS:
        return "TLS";
    default:
        return "OTHER";
    }
}

static std::string vis_name(unsigned v)
{
    switch (v)
    {
    case STV_DEFAULT:
        return "DEFAULT";
    case STV_INTERNAL:
        return "INTERNAL";
    case STV_HIDDEN:
        return "HIDDEN";
    case STV_PROTECTED:
        return "PROTECTED";
    default:
        return "OTHER";
    }
}

static bool keep_bind(unsigned b, const Config &cfg)
{
    if (b == STB_LOCAL)
        return cfg.include_local;
    if (b == STB_GLOBAL)
        return cfg.include_global;
    if (b == STB_WEAK)
        return cfg.include_weak;
    return true;
}

template <typename ShdrT, typename SymT>
static void collect_from_table(const std::vector<unsigned char> &b,
                               const std::vector<ShdrT> &shdrs,
                               const ShdrT &symtab,
                               const char *shstr,
                               const Config &cfg,
                               std::vector<SymbolRow> &rows)
{
    if (symtab.sh_entsize == 0 || symtab.sh_link >= shdrs.size())
        return;
    if (symtab.sh_offset + symtab.sh_size > b.size())
        return;

    const ShdrT &strtab = shdrs[symtab.sh_link];
    if (strtab.sh_offset + strtab.sh_size > b.size())
        return;

    const char *sym_names = reinterpret_cast<const char *>(b.data() + strtab.sh_offset);
    const char *table_name = (symtab.sh_name < shdrs[0].sh_size * 1024) ? (shstr + symtab.sh_name) : "?";

    std::size_t n = static_cast<std::size_t>(symtab.sh_size / symtab.sh_entsize);
    for (std::size_t i = 0; i < n; ++i)
    {
        const auto *s = reinterpret_cast<const SymT *>(b.data() + symtab.sh_offset + i * symtab.sh_entsize);
        unsigned bind = ELF64_ST_BIND(s->st_info);
        unsigned type = ELF64_ST_TYPE(s->st_info);
        unsigned vis = ELF64_ST_VISIBILITY(s->st_other);

        if (!keep_bind(bind, cfg))
            continue;

        bool undefined = (s->st_shndx == SHN_UNDEF);
        if (cfg.imports_only && !undefined)
            continue;
        if (cfg.exports_only && undefined)
            continue;

        if (s->st_name >= strtab.sh_size)
            continue;
        std::string name = sym_names + s->st_name;
        if (name.empty())
            continue;

        if (cfg.demangle)
            name = demangle_name(name);

        rows.push_back(SymbolRow{table_name, bind_name(bind), type_name(type), vis_name(vis), undefined, s->st_value, s->st_size, name});
    }
}

static bool parse64(const std::vector<unsigned char> &b, const Config &cfg, std::vector<SymbolRow> &rows)
{
    if (b.size() < sizeof(Elf64_Ehdr))
        return false;
    const auto *eh = reinterpret_cast<const Elf64_Ehdr *>(b.data());
    if (eh->e_shoff + eh->e_shnum * sizeof(Elf64_Shdr) > b.size())
        return false;
    if (eh->e_shstrndx >= eh->e_shnum)
        return false;

    std::vector<Elf64_Shdr> shdrs(eh->e_shnum);
    for (uint16_t i = 0; i < eh->e_shnum; ++i)
    {
        shdrs[i] = *reinterpret_cast<const Elf64_Shdr *>(b.data() + eh->e_shoff + i * sizeof(Elf64_Shdr));
    }

    const auto &shstr = shdrs[eh->e_shstrndx];
    if (shstr.sh_offset + shstr.sh_size > b.size())
        return false;
    const char *shstr_names = reinterpret_cast<const char *>(b.data() + shstr.sh_offset);

    for (const auto &sh : shdrs)
    {
        if (sh.sh_type == SHT_SYMTAB || sh.sh_type == SHT_DYNSYM)
        {
            if (cfg.dyn_only && sh.sh_type != SHT_DYNSYM)
                continue;
            collect_from_table<Elf64_Shdr, Elf64_Sym>(b, shdrs, sh, shstr_names, cfg, rows);
        }
    }

    return true;
}

static bool parse32(const std::vector<unsigned char> &b, const Config &cfg, std::vector<SymbolRow> &rows)
{
    if (b.size() < sizeof(Elf32_Ehdr))
        return false;
    const auto *eh = reinterpret_cast<const Elf32_Ehdr *>(b.data());
    if (eh->e_shoff + eh->e_shnum * sizeof(Elf32_Shdr) > b.size())
        return false;
    if (eh->e_shstrndx >= eh->e_shnum)
        return false;

    std::vector<Elf32_Shdr> shdrs(eh->e_shnum);
    for (uint16_t i = 0; i < eh->e_shnum; ++i)
    {
        shdrs[i] = *reinterpret_cast<const Elf32_Shdr *>(b.data() + eh->e_shoff + i * sizeof(Elf32_Shdr));
    }

    const auto &shstr = shdrs[eh->e_shstrndx];
    if (shstr.sh_offset + shstr.sh_size > b.size())
        return false;
    const char *shstr_names = reinterpret_cast<const char *>(b.data() + shstr.sh_offset);

    for (const auto &sh : shdrs)
    {
        if (sh.sh_type == SHT_SYMTAB || sh.sh_type == SHT_DYNSYM)
        {
            if (cfg.dyn_only && sh.sh_type != SHT_DYNSYM)
                continue;
            collect_from_table<Elf32_Shdr, Elf32_Sym>(b, shdrs, sh, shstr_names, cfg, rows);
        }
    }

    return true;
}

static std::string color_bind(const std::string &bind)
{
    if (bind == "GLOBAL")
        return col::global_s(bind);
    if (bind == "WEAK")
        return col::weak_s(bind);
    if (bind == "LOCAL")
        return col::local_s(bind);
    return bind;
}

int main(int argc, char **argv)
{
    Config cfg;
    std::optional<fs::path> file;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            print_help(argv[0]);
            return 0;
        }
        if (arg == "-D" || arg == "--dyn")
        {
            cfg.dyn_only = true;
            continue;
        }
        if (arg == "--imports")
        {
            cfg.imports_only = true;
            continue;
        }
        if (arg == "--exports")
        {
            cfg.exports_only = true;
            continue;
        }
        if (arg == "--no-demangle")
        {
            cfg.demangle = false;
            continue;
        }
        if (arg == "-l" || arg == "--locals")
        {
            cfg.include_local = true;
            continue;
        }
        if (arg == "-g" || arg == "--globals")
        {
            cfg.include_global = true;
            continue;
        }
        if (arg == "-w" || arg == "--weaks")
        {
            cfg.include_weak = true;
            continue;
        }
        if (!arg.empty() && arg[0] == '-')
        {
            std::cerr << "sym: unknown option '" << arg << "'\n";
            return 1;
        }
        if (file)
        {
            std::cerr << "sym: too many input files\n";
            return 1;
        }
        file = fs::path(arg);
    }

    if (!file)
    {
        print_help(argv[0]);
        return 1;
    }

    if (cfg.imports_only && cfg.exports_only)
    {
        std::cerr << "sym: cannot combine --imports and --exports\n";
        return 1;
    }

    std::vector<unsigned char> b;
    if (!read_all(*file, b))
    {
        std::cerr << "sym: cannot read file: " << file->string() << "\n";
        return 1;
    }
    if (b.size() < EI_NIDENT || b[0] != 0x7F || b[1] != 'E' || b[2] != 'L' || b[3] != 'F')
    {
        std::cerr << "sym: not an ELF file\n";
        return 1;
    }
    if (b[EI_DATA] != ELFDATA2LSB)
    {
        std::cerr << "sym: only little-endian ELF is supported\n";
        return 1;
    }

    col::init();

    std::vector<SymbolRow> rows;
    bool ok = false;
    if (b[EI_CLASS] == ELFCLASS64)
        ok = parse64(b, cfg, rows);
    else if (b[EI_CLASS] == ELFCLASS32)
        ok = parse32(b, cfg, rows);

    if (!ok)
    {
        std::cerr << "sym: failed to parse symbol tables\n";
        return 1;
    }

    std::cout << std::left << std::setw(9) << "TABLE"
              << std::setw(9) << "BIND"
              << std::setw(10) << "TYPE"
              << std::setw(10) << "VIS"
              << std::setw(8) << "SCOPE"
              << std::setw(12) << "VALUE"
              << std::setw(8) << "SIZE"
              << "NAME\n";

    for (const auto &r : rows)
    {
        std::ostringstream val;
        val << "0x" << std::hex << r.value;
        std::cout << std::left << std::setw(9) << r.table
                  << std::setw(9) << color_bind(r.bind)
                  << std::setw(10) << r.type
                  << std::setw(10) << r.vis
                  << std::setw(8) << (r.undefined ? "import" : "export")
                  << std::setw(12) << val.str()
                  << std::setw(8) << r.size
                  << r.name << "\n";
    }

    return 0;
}
