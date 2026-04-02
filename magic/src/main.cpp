#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct MagicRule
{
    std::vector<unsigned char> bytes;
    std::string type;
};

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " FILE...\n"
        << "\n"
        << "Identify file type from magic bytes (not extension).\n";
}

static std::string identify(const std::vector<unsigned char> &buf)
{
    const std::vector<MagicRule> rules = {
        {{0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A}, "PNG image"},
        {{0xFF, 0xD8, 0xFF}, "JPEG image"},
        {{0x47, 0x49, 0x46, 0x38}, "GIF image"},
        {{0x42, 0x4D}, "BMP image"},
        {{0x49, 0x49, 0x2A, 0x00}, "TIFF image (little-endian)"},
        {{0x4D, 0x4D, 0x00, 0x2A}, "TIFF image (big-endian)"},
        {{0x38, 0x42, 0x50, 0x53}, "Photoshop PSD"},
        {{0x00, 0x00, 0x01, 0x00}, "ICO icon"},
        {{0x00, 0x00, 0x02, 0x00}, "CUR cursor"},
        {{0x00, 0x00, 0x00, 0x0C, 0x6A, 0x50, 0x20, 0x20}, "JPEG 2000 image"},
        {{0xFF, 0x4F, 0xFF, 0x51}, "JPEG 2000 codestream"},
        {{0x46, 0x4C, 0x49, 0x46}, "FLIF image"},
        {{0x67, 0x69, 0x6D, 0x70, 0x20, 0x58, 0x43, 0x46}, "GIMP XCF image"},
        {{0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB}, "KTX texture"},
        {{0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB}, "KTX2 texture"},
        {{0x77, 0x4F, 0x46, 0x46}, "WOFF font"},
        {{0x77, 0x4F, 0x46, 0x32}, "WOFF2 font"},
        {{0x4F, 0x54, 0x54, 0x4F}, "OpenType font (CFF)"},
        {{0x00, 0x01, 0x00, 0x00, 0x00}, "TrueType font"},
        {{0x49, 0x44, 0x33}, "MP3 audio (ID3)"},
        {{0xFF, 0xFB}, "MP3 audio"},
        {{0xFF, 0xF3}, "MP3 audio"},
        {{0xFF, 0xF2}, "MP3 audio"},
        {{0x66, 0x4C, 0x61, 0x43}, "FLAC audio"},
        {{0x4F, 0x67, 0x67, 0x53}, "Ogg container"},
        {{0x4D, 0x54, 0x68, 0x64}, "MIDI audio"},
        {{0x4D, 0x41, 0x43, 0x20}, "APE audio"},
        {{0x77, 0x76, 0x70, 0x6B}, "WavPack audio"},
        {{0xFF, 0xF1}, "AAC audio (ADTS)"},
        {{0xFF, 0xF9}, "AAC audio (ADTS)"},
        {{0x23, 0x21, 0x41, 0x4D, 0x52}, "AMR audio"},
        {{0x30, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11}, "ASF/WMA/WMV"},
        // NOTE: MP4/MOV (ftyp box) is handled below via offset-4 check
        {{0x1A, 0x45, 0xDF, 0xA3}, "Matroska/WebM container"},
        {{0x46, 0x4C, 0x56, 0x01}, "Flash Video (FLV)"},
        {{0x00, 0x00, 0x01, 0xBA}, "MPEG program stream"},
        {{0x00, 0x00, 0x01, 0xB3}, "MPEG video stream"},
        {{0x50, 0x4B, 0x03, 0x04}, "ZIP archive"},
        {{0x50, 0x4B, 0x05, 0x06}, "ZIP archive (empty)"},
        {{0x50, 0x4B, 0x07, 0x08}, "ZIP archive (spanned)"},
        {{0x1F, 0x8B, 0x08}, "GZIP archive"},
        {{0x42, 0x5A, 0x68}, "BZIP2 archive"},
        {{0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x01, 0x00}, "RAR archive (v5+)"},
        {{0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x00}, "RAR archive (v1.5+)"},
        {{0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C}, "7-Zip archive"},
        {{0xFD, 0x37, 0x7A, 0x58, 0x5A, 0x00}, "XZ archive"},
        {{0x28, 0xB5, 0x2F, 0xFD}, "Zstandard archive"},
        {{0x04, 0x22, 0x4D, 0x18}, "LZ4 archive"},
        {{0x89, 0x4C, 0x5A, 0x4F, 0x00, 0x0D, 0x0A, 0x1A}, "LZOP archive"},
        {{0x5D, 0x00, 0x00, 0x80}, "LZMA archive"},
        {{0x1F, 0x9D}, "compress'd data"},
        {{0x1F, 0xA0}, "LZH compressed data"},
        {{0x78, 0xDA}, "zlib (best compression)"},
        {{0x78, 0x9C}, "zlib (default compression)"},
        {{0x78, 0x01}, "zlib (no compression)"},
        {{0x21, 0x3C, 0x61, 0x72, 0x63, 0x68, 0x3E}, "ar/Debian package"},
        {{0xED, 0xAB, 0xEE, 0xDB}, "RPM package"},
        {{0x25, 0x50, 0x44, 0x46}, "PDF document"},
        {{0x7B, 0x5C, 0x72, 0x74, 0x66}, "RTF document"},
        {{0x25, 0x21}, "PostScript document"},
        {{0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1}, "OLE2 compound document (DOC/XLS/PPT)"},
        {{0x7F, 0x45, 0x4C, 0x46}, "ELF executable/object"},
        {{0x4D, 0x5A}, "Windows PE/MZ executable"},
        {{0xCF, 0xFA, 0xED, 0xFE}, "Mach-O 64-bit executable"},
        {{0xCE, 0xFA, 0xED, 0xFE}, "Mach-O 32-bit executable"},
        {{0xFE, 0xED, 0xFA, 0xCF}, "Mach-O 64-bit executable (BE)"},
        {{0xFE, 0xED, 0xFA, 0xCE}, "Mach-O 32-bit executable (BE)"},
        {{0xCA, 0xFE, 0xBA, 0xBE}, "Java class / Mach-O fat binary"},
        {{0x00, 0x61, 0x73, 0x6D}, "WebAssembly binary"},
        {{0x1B, 0x4C, 0x75, 0x61}, "Lua bytecode"},
        {{0x42, 0x43, 0xC0, 0xDE}, "LLVM IR bitcode"},
        {{0x53, 0x51, 0x4C, 0x69, 0x74, 0x65, 0x20, 0x66,
          0x6F, 0x72, 0x6D, 0x61, 0x74, 0x20, 0x33, 0x00},
         "SQLite database"},
        {{0x51, 0x46, 0x49, 0xFB}, "QCOW2 disk image"},
        {{0x4B, 0x44, 0x4D, 0x56}, "VMDK disk image"},
        {{0x30, 0x82}, "DER-encoded certificate/key"},
    };

    for (const auto &r : rules)
    {
        if (buf.size() < r.bytes.size())
            continue;
        bool ok = true;
        for (std::size_t i = 0; i < r.bytes.size(); ++i)
        {
            if (buf[i] != r.bytes[i])
            {
                ok = false;
                break;
            }
        }
        if (ok)
            return r.type;
    }

    if (buf.size() >= 12 &&
        buf[0] == 0x52 && buf[1] == 0x49 && buf[2] == 0x46 && buf[3] == 0x46)
    {
        auto riff4 = [&](const char *s)
        {
            return buf[8] == (unsigned char)s[0] && buf[9] == (unsigned char)s[1] &&
                   buf[10] == (unsigned char)s[2] && buf[11] == (unsigned char)s[3];
        };
        if (riff4("WAVE"))
            return "WAV audio";
        if (riff4("AVI "))
            return "AVI video";
        if (riff4("WEBP"))
            return "WebP image";
        return "RIFF container";
    }

    if (buf.size() >= 12 &&
        buf[4] == 0x66 && buf[5] == 0x74 && buf[6] == 0x79 && buf[7] == 0x70)
    {
        auto brand = [&](const char *s)
        {
            return buf[8] == (unsigned char)s[0] && buf[9] == (unsigned char)s[1] &&
                   buf[10] == (unsigned char)s[2] && buf[11] == (unsigned char)s[3];
        };
        if (brand("avif") || brand("avis"))
            return "AVIF image";
        if (brand("heic") || brand("heix") ||
            brand("mif1") || brand("msf1"))
            return "HEIC/HEIF image";
        if (brand("M4A ") || brand("M4P "))
            return "M4A audio (iTunes)";
        if (brand("M4V ") || brand("M4VH") || brand("M4VP"))
            return "M4V video (iTunes)";
        if (brand("qt  "))
            return "QuickTime MOV";
        if (brand("3gp4") || brand("3gp5") || brand("3gp6"))
            return "3GPP video";
        if (brand("3g2a") || brand("3g2b") || brand("3g2c"))
            return "3GPP2 video";
        if (brand("f4v ") || brand("f4p "))
            return "Flash MP4";
        if (brand("dash"))
            return "MPEG-DASH video";
        return "MP4/MOV container";
    }

    if (buf.size() >= 262)
    {
        if (buf[257] == 0x75 && buf[258] == 0x73 && buf[259] == 0x74 &&
            buf[260] == 0x61 && buf[261] == 0x72)
            return "TAR archive";
    }

    bool text_like = !buf.empty();
    for (unsigned char c : buf)
    {
        if (!(c == 9 || c == 10 || c == 13 || (c >= 32 && c <= 126)))
        {
            text_like = false;
            break;
        }
    }

    if (text_like)
    {
        if (buf.size() >= 2 && buf[0] == '#' && buf[1] == '!')
        {
            std::string line;
            for (std::size_t i = 2; i < buf.size() && buf[i] != '\n' && buf[i] != '\r'; ++i)
                line += static_cast<char>(buf[i]);
            if (line.find("python") != std::string::npos)
                return "Python script";
            if (line.find("bash") != std::string::npos)
                return "Bash script";
            if (line.find("zsh") != std::string::npos)
                return "Zsh script";
            if (line.find("/sh") != std::string::npos)
                return "Shell script";
            if (line.find("perl") != std::string::npos)
                return "Perl script";
            if (line.find("ruby") != std::string::npos)
                return "Ruby script";
            if (line.find("node") != std::string::npos)
                return "Node.js script";
            if (line.find("php") != std::string::npos)
                return "PHP script";
            if (line.find("lua") != std::string::npos)
                return "Lua script";
            return "script";
        }

        const std::string head(buf.begin(), buf.begin() + std::min(buf.size(), std::size_t(64)));
        if (head.rfind("-----BEGIN CERTIFICATE", 0) == 0)
            return "PEM certificate";
        if (head.rfind("-----BEGIN PRIVATE KEY", 0) == 0)
            return "PEM private key (PKCS#8)";
        if (head.rfind("-----BEGIN RSA PRIVATE KEY", 0) == 0)
            return "PEM RSA private key";
        if (head.rfind("-----BEGIN PUBLIC KEY", 0) == 0)
            return "PEM public key";
        if (head.rfind("-----BEGIN", 0) == 0)
            return "PEM encoded data";

        if (head.rfind("<?xml", 0) == 0)
            return "XML document";
        if (head.rfind("<!DOCTYPE html", 0) == 0 ||
            head.rfind("<html", 0) == 0)
            return "HTML document";

        return "text data";
    }

    return "unknown binary data";
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

    if (argc < 2)
    {
        print_help(argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; ++i)
    {
        fs::path file = argv[i];
        std::ifstream in(file, std::ios::binary);
        if (!in)
        {
            std::cerr << "magic: cannot open: " << file.string() << "\n";
            continue;
        }

        std::vector<unsigned char> buf(32, 0);
        in.read(reinterpret_cast<char *>(buf.data()), static_cast<std::streamsize>(buf.size()));
        buf.resize(static_cast<std::size_t>(in.gcount()));

        std::cout << file.string() << ": " << identify(buf) << "\n";
    }

    return 0;
}
