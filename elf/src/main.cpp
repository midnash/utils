#include <elf.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct FileData
{
    std::vector<unsigned char> bytes;
};

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " ELF_FILE\n"
        << "\n"
        << "Human-readable ELF inspector (header, segments, sections, interp).\n";
}

static bool read_file(const fs::path &p, FileData &out)
{
    std::ifstream in(p, std::ios::binary);
    if (!in)
    {
        return false;
    }
    in.seekg(0, std::ios::end);
    std::streamsize n = in.tellg();
    in.seekg(0, std::ios::beg);
    if (n <= 0)
    {
        return false;
    }
    out.bytes.resize(static_cast<std::size_t>(n));
    in.read(reinterpret_cast<char *>(out.bytes.data()), n);
    return in.good() || in.gcount() == n;
}

static const char *type_name(uint16_t t)
{
    switch (t)
    {
    case ET_NONE:
        return "NONE";
    case ET_REL:
        return "REL";
    case ET_EXEC:
        return "EXEC";
    case ET_DYN:
        return "DYN";
    case ET_CORE:
        return "CORE";
    default:
        return "OTHER";
    }
}

static const char *machine_name(uint16_t m)
{
    switch (m)
    {
    case EM_X86_64:
        return "x86_64";
    case EM_386:
        return "x86";
    case EM_AARCH64:
        return "aarch64";
    case EM_ARM:
        return "arm";
    case EM_RISCV:
        return "riscv";
    default:
        return "unknown";
    }
}

static const char *phdr_type_name(uint32_t t)
{
    switch (t)
    {
    case PT_NULL:
        return "NULL";
    case PT_LOAD:
        return "LOAD";
    case PT_DYNAMIC:
        return "DYNAMIC";
    case PT_INTERP:
        return "INTERP";
    case PT_NOTE:
        return "NOTE";
    case PT_PHDR:
        return "PHDR";
    case PT_TLS:
        return "TLS";
    default:
        return "OTHER";
    }
}

static const char *shdr_type_name(uint32_t t)
{
    switch (t)
    {
    case SHT_NULL:
        return "NULL";
    case SHT_PROGBITS:
        return "PROGBITS";
    case SHT_SYMTAB:
        return "SYMTAB";
    case SHT_STRTAB:
        return "STRTAB";
    case SHT_RELA:
        return "RELA";
    case SHT_HASH:
        return "HASH";
    case SHT_DYNAMIC:
        return "DYNAMIC";
    case SHT_NOTE:
        return "NOTE";
    case SHT_NOBITS:
        return "NOBITS";
    case SHT_DYNSYM:
        return "DYNSYM";
    default:
        return "OTHER";
    }
}

static bool inspect64(const FileData &d)
{
    if (d.bytes.size() < sizeof(Elf64_Ehdr))
    {
        return false;
    }

    const auto *eh = reinterpret_cast<const Elf64_Ehdr *>(d.bytes.data());
    std::cout << "class: ELF64\n";
    std::cout << "type: " << type_name(eh->e_type) << "\n";
    std::cout << "machine: " << machine_name(eh->e_machine) << "\n";
    std::cout << "entry: 0x" << std::hex << eh->e_entry << std::dec << "\n";
    std::cout << "sections: " << eh->e_shnum << "\n";
    std::cout << "segments: " << eh->e_phnum << "\n";

    if (eh->e_phoff + eh->e_phnum * sizeof(Elf64_Phdr) <= d.bytes.size())
    {
        std::cout << "\nprogram headers:\n";
        std::cout << "  TYPE      OFF        VADDR      FILESZ     MEMSZ\n";
        for (uint16_t i = 0; i < eh->e_phnum; ++i)
        {
            const auto *ph = reinterpret_cast<const Elf64_Phdr *>(d.bytes.data() + eh->e_phoff + i * sizeof(Elf64_Phdr));
            std::cout << "  " << std::left << std::setw(8) << phdr_type_name(ph->p_type)
                      << " 0x" << std::hex << std::setw(8) << ph->p_offset
                      << " 0x" << std::setw(8) << ph->p_vaddr
                      << " 0x" << std::setw(8) << ph->p_filesz
                      << " 0x" << std::setw(8) << ph->p_memsz << std::dec << "\n";

            if (ph->p_type == PT_INTERP && ph->p_offset + ph->p_filesz <= d.bytes.size())
            {
                std::string interp(reinterpret_cast<const char *>(d.bytes.data() + ph->p_offset),
                                   reinterpret_cast<const char *>(d.bytes.data() + ph->p_offset + ph->p_filesz));
                auto nul = interp.find('\0');
                if (nul != std::string::npos)
                    interp.resize(nul);
                std::cout << "interpreter: " << interp << "\n";
            }
        }
    }

    if (eh->e_shoff + eh->e_shnum * sizeof(Elf64_Shdr) <= d.bytes.size() && eh->e_shstrndx < eh->e_shnum)
    {
        const auto *shstr = reinterpret_cast<const Elf64_Shdr *>(d.bytes.data() + eh->e_shoff + eh->e_shstrndx * sizeof(Elf64_Shdr));
        if (shstr->sh_offset + shstr->sh_size <= d.bytes.size())
        {
            const char *names = reinterpret_cast<const char *>(d.bytes.data() + shstr->sh_offset);
            std::cout << "\nsections:\n";
            std::cout << "  NAME                 TYPE       OFF        SIZE\n";
            for (uint16_t i = 0; i < eh->e_shnum; ++i)
            {
                const auto *sh = reinterpret_cast<const Elf64_Shdr *>(d.bytes.data() + eh->e_shoff + i * sizeof(Elf64_Shdr));
                const char *nm = (sh->sh_name < shstr->sh_size) ? names + sh->sh_name : "?";
                std::cout << "  " << std::left << std::setw(20) << nm
                          << " " << std::setw(10) << shdr_type_name(sh->sh_type)
                          << " 0x" << std::hex << std::setw(8) << sh->sh_offset
                          << " 0x" << std::setw(8) << sh->sh_size << std::dec << "\n";
            }
        }
    }

    return true;
}

static bool inspect32(const FileData &d)
{
    if (d.bytes.size() < sizeof(Elf32_Ehdr))
    {
        return false;
    }

    const auto *eh = reinterpret_cast<const Elf32_Ehdr *>(d.bytes.data());
    std::cout << "class: ELF32\n";
    std::cout << "type: " << type_name(eh->e_type) << "\n";
    std::cout << "machine: " << machine_name(eh->e_machine) << "\n";
    std::cout << "entry: 0x" << std::hex << eh->e_entry << std::dec << "\n";
    std::cout << "sections: " << eh->e_shnum << "\n";
    std::cout << "segments: " << eh->e_phnum << "\n";

    if (eh->e_phoff + eh->e_phnum * sizeof(Elf32_Phdr) <= d.bytes.size())
    {
        std::cout << "\nprogram headers:\n";
        std::cout << "  TYPE      OFF        VADDR      FILESZ     MEMSZ\n";
        for (uint16_t i = 0; i < eh->e_phnum; ++i)
        {
            const auto *ph = reinterpret_cast<const Elf32_Phdr *>(d.bytes.data() + eh->e_phoff + i * sizeof(Elf32_Phdr));
            std::cout << "  " << std::left << std::setw(8) << phdr_type_name(ph->p_type)
                      << " 0x" << std::hex << std::setw(8) << ph->p_offset
                      << " 0x" << std::setw(8) << ph->p_vaddr
                      << " 0x" << std::setw(8) << ph->p_filesz
                      << " 0x" << std::setw(8) << ph->p_memsz << std::dec << "\n";

            if (ph->p_type == PT_INTERP && ph->p_offset + ph->p_filesz <= d.bytes.size())
            {
                std::string interp(reinterpret_cast<const char *>(d.bytes.data() + ph->p_offset),
                                   reinterpret_cast<const char *>(d.bytes.data() + ph->p_offset + ph->p_filesz));
                auto nul = interp.find('\0');
                if (nul != std::string::npos)
                    interp.resize(nul);
                std::cout << "interpreter: " << interp << "\n";
            }
        }
    }

    return true;
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

    FileData d;
    fs::path file = argv[1];
    if (!read_file(file, d))
    {
        std::cerr << "elf: cannot read file: " << file.string() << "\n";
        return 1;
    }

    if (d.bytes.size() < EI_NIDENT || d.bytes[0] != 0x7F || d.bytes[1] != 'E' || d.bytes[2] != 'L' || d.bytes[3] != 'F')
    {
        std::cerr << "elf: not an ELF file\n";
        return 1;
    }
    if (d.bytes[EI_DATA] != ELFDATA2LSB)
    {
        std::cerr << "elf: only little-endian ELF is supported\n";
        return 1;
    }

    bool ok = false;
    if (d.bytes[EI_CLASS] == ELFCLASS64)
    {
        ok = inspect64(d);
    }
    else if (d.bytes[EI_CLASS] == ELFCLASS32)
    {
        ok = inspect32(d);
    }

    if (!ok)
    {
        std::cerr << "elf: failed to parse ELF\n";
        return 1;
    }

    return 0;
}
