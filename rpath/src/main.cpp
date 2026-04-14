#include <elf.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Segment
{
    uint64_t vaddr = 0;
    uint64_t offset = 0;
    uint64_t filesz = 0;
    uint32_t type = 0;
};

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " ELF_FILE\n"
        << "\n"
        << "Print RPATH/RUNPATH entries from ELF dynamic section.\n";
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

static bool vaddr_to_offset(uint64_t va, const std::vector<Segment> &loads, uint64_t &off)
{
    for (const auto &s : loads)
    {
        if (s.type != PT_LOAD)
            continue;
        if (va >= s.vaddr && va < s.vaddr + s.filesz)
        {
            off = s.offset + (va - s.vaddr);
            return true;
        }
    }
    return false;
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

    std::vector<unsigned char> b;
    fs::path file = argv[1];
    if (!read_all(file, b))
    {
        std::cerr << "rpath: cannot read file: " << file.string() << "\n";
        return 1;
    }

    if (b.size() < EI_NIDENT || b[0] != 0x7F || b[1] != 'E' || b[2] != 'L' || b[3] != 'F')
    {
        std::cerr << "rpath: not an ELF file\n";
        return 1;
    }

    if (b[EI_DATA] != ELFDATA2LSB)
    {
        std::cerr << "rpath: only little-endian ELF is supported\n";
        return 1;
    }

    bool found_any = false;

    if (b[EI_CLASS] == ELFCLASS64)
    {
        if (b.size() < sizeof(Elf64_Ehdr))
            return 1;
        const auto *eh = reinterpret_cast<const Elf64_Ehdr *>(b.data());
        if (eh->e_phoff + eh->e_phnum * sizeof(Elf64_Phdr) > b.size())
            return 1;

        std::vector<Segment> segs;
        uint64_t dyn_off = 0;
        uint64_t dyn_size = 0;
        for (uint16_t i = 0; i < eh->e_phnum; ++i)
        {
            const auto *ph = reinterpret_cast<const Elf64_Phdr *>(b.data() + eh->e_phoff + i * sizeof(Elf64_Phdr));
            segs.push_back({ph->p_vaddr, ph->p_offset, ph->p_filesz, ph->p_type});
            if (ph->p_type == PT_DYNAMIC)
            {
                dyn_off = ph->p_offset;
                dyn_size = ph->p_filesz;
            }
        }

        if (dyn_off == 0 || dyn_off + dyn_size > b.size())
        {
            std::cout << "(no dynamic section)\n";
            return 0;
        }

        uint64_t strtab_va = 0;
        uint64_t strsz = 0;
        std::vector<uint64_t> rpath_offs;
        std::vector<uint64_t> runpath_offs;

        for (uint64_t o = dyn_off; o + sizeof(Elf64_Dyn) <= dyn_off + dyn_size; o += sizeof(Elf64_Dyn))
        {
            const auto *d = reinterpret_cast<const Elf64_Dyn *>(b.data() + o);
            if (d->d_tag == DT_NULL)
                break;
            if (d->d_tag == DT_STRTAB)
                strtab_va = d->d_un.d_ptr;
            else if (d->d_tag == DT_STRSZ)
                strsz = d->d_un.d_val;
            else if (d->d_tag == DT_RPATH)
                rpath_offs.push_back(d->d_un.d_val);
            else if (d->d_tag == DT_RUNPATH)
                runpath_offs.push_back(d->d_un.d_val);
        }

        uint64_t strtab_off = 0;
        if (!vaddr_to_offset(strtab_va, segs, strtab_off) || strtab_off + strsz > b.size())
        {
            std::cerr << "rpath: failed to locate dynamic string table\n";
            return 1;
        }

        auto print_entry = [&](const char *kind, uint64_t rel)
        {
            if (rel >= strsz)
                return;
            const char *s = reinterpret_cast<const char *>(b.data() + strtab_off + rel);
            std::cout << kind << ": " << s << "\n";
            found_any = true;
        };

        for (uint64_t rel : rpath_offs)
            print_entry("RPATH", rel);
        for (uint64_t rel : runpath_offs)
            print_entry("RUNPATH", rel);
    }
    else if (b[EI_CLASS] == ELFCLASS32)
    {
        if (b.size() < sizeof(Elf32_Ehdr))
            return 1;
        const auto *eh = reinterpret_cast<const Elf32_Ehdr *>(b.data());
        if (eh->e_phoff + eh->e_phnum * sizeof(Elf32_Phdr) > b.size())
            return 1;

        std::vector<Segment> segs;
        uint64_t dyn_off = 0;
        uint64_t dyn_size = 0;
        for (uint16_t i = 0; i < eh->e_phnum; ++i)
        {
            const auto *ph = reinterpret_cast<const Elf32_Phdr *>(b.data() + eh->e_phoff + i * sizeof(Elf32_Phdr));
            segs.push_back({ph->p_vaddr, ph->p_offset, ph->p_filesz, ph->p_type});
            if (ph->p_type == PT_DYNAMIC)
            {
                dyn_off = ph->p_offset;
                dyn_size = ph->p_filesz;
            }
        }

        if (dyn_off == 0 || dyn_off + dyn_size > b.size())
        {
            std::cout << "(no dynamic section)\n";
            return 0;
        }

        uint64_t strtab_va = 0;
        uint64_t strsz = 0;
        std::vector<uint64_t> rpath_offs;
        std::vector<uint64_t> runpath_offs;

        for (uint64_t o = dyn_off; o + sizeof(Elf32_Dyn) <= dyn_off + dyn_size; o += sizeof(Elf32_Dyn))
        {
            const auto *d = reinterpret_cast<const Elf32_Dyn *>(b.data() + o);
            if (d->d_tag == DT_NULL)
                break;
            if (d->d_tag == DT_STRTAB)
                strtab_va = d->d_un.d_ptr;
            else if (d->d_tag == DT_STRSZ)
                strsz = d->d_un.d_val;
            else if (d->d_tag == DT_RPATH)
                rpath_offs.push_back(d->d_un.d_val);
            else if (d->d_tag == DT_RUNPATH)
                runpath_offs.push_back(d->d_un.d_val);
        }

        uint64_t strtab_off = 0;
        if (!vaddr_to_offset(strtab_va, segs, strtab_off) || strtab_off + strsz > b.size())
        {
            std::cerr << "rpath: failed to locate dynamic string table\n";
            return 1;
        }

        auto print_entry = [&](const char *kind, uint64_t rel)
        {
            if (rel >= strsz)
                return;
            const char *s = reinterpret_cast<const char *>(b.data() + strtab_off + rel);
            std::cout << kind << ": " << s << "\n";
            found_any = true;
        };

        for (uint64_t rel : rpath_offs)
            print_entry("RPATH", rel);
        for (uint64_t rel : runpath_offs)
            print_entry("RUNPATH", rel);
    }
    else
    {
        std::cerr << "rpath: unsupported ELF class\n";
        return 1;
    }

    if (!found_any)
    {
        std::cout << "(no RPATH/RUNPATH)\n";
    }

    return 0;
}
