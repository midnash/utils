#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

struct SockRow
{
    std::string proto;
    std::string local;
    std::string inode;
};

struct ProcRow
{
    int pid = 0;
    std::string comm;
};

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " [PORT]\n"
        << "\n"
        << "Show listening sockets and owning processes.\n"
        << "If PORT is provided, filter to that local port.\n";
}

static std::vector<std::string> split_ws(const std::string &s)
{
    std::istringstream iss(s);
    std::vector<std::string> out;
    std::string tok;
    while (iss >> tok)
        out.push_back(tok);
    return out;
}

static std::string decode_ipv4_hex(const std::string &hex)
{
    if (hex.size() != 8)
        return hex;
    unsigned long raw = std::stoul(hex, nullptr, 16);
    std::ostringstream oss;
    oss << (raw & 0xFF) << '.' << ((raw >> 8) & 0xFF) << '.' << ((raw >> 16) & 0xFF) << '.' << ((raw >> 24) & 0xFF);
    return oss.str();
}

static std::string decode_ipv6_hex(const std::string &hex)
{
    if (hex.size() != 32)
        return hex;

    std::array<unsigned char, 16> b{};
    for (int i = 0; i < 16; ++i)
    {
        b[i] = static_cast<unsigned char>(std::stoul(hex.substr(i * 2, 2), nullptr, 16));
    }
    for (int i = 0; i < 16; i += 4)
    {
        std::swap(b[i], b[i + 3]);
        std::swap(b[i + 1], b[i + 2]);
    }

    std::ostringstream oss;
    for (int i = 0; i < 16; i += 2)
    {
        unsigned x = (static_cast<unsigned>(b[i]) << 8) | b[i + 1];
        oss << std::hex << x;
        if (i != 14)
            oss << ':';
    }
    return oss.str();
}

static bool parse_proc_addr(const std::string &token, bool v6, std::string &addr, int &port)
{
    auto p = token.find(':');
    if (p == std::string::npos)
        return false;
    std::string h = token.substr(0, p);
    std::string ph = token.substr(p + 1);
    unsigned long prt = 0;
    try
    {
        prt = std::stoul(ph, nullptr, 16);
    }
    catch (...)
    {
        return false;
    }
    addr = v6 ? decode_ipv6_hex(h) : decode_ipv4_hex(h);
    port = static_cast<int>(prt);
    return true;
}

static void parse_net_table(const fs::path &p, const std::string &proto, bool v6, std::vector<SockRow> &out)
{
    std::ifstream in(p);
    if (!in)
        return;

    std::string line;
    std::getline(in, line); // header
    while (std::getline(in, line))
    {
        auto c = split_ws(line);
        if (c.size() < 10)
            continue;

        std::string state = c[3];
        if (state != "0A")
            continue;

        std::string addr;
        int port = 0;
        if (!parse_proc_addr(c[1], v6, addr, port))
            continue;

        out.push_back({proto, addr + ":" + std::to_string(port), c[9]});
    }
}

static std::unordered_map<std::string, std::vector<ProcRow>> inode_to_processes()
{
    std::unordered_map<std::string, std::vector<ProcRow>> m;

    for (const auto &p : fs::directory_iterator("/proc", fs::directory_options::skip_permission_denied))
    {
        if (!p.is_directory())
            continue;
        std::string pid_s = p.path().filename().string();
        if (pid_s.empty() || !std::all_of(pid_s.begin(), pid_s.end(), ::isdigit))
            continue;

        int pid = 0;
        try
        {
            pid = std::stoi(pid_s);
        }
        catch (...)
        {
            continue;
        }

        std::string comm = "?";
        {
            std::ifstream commf(p.path() / "comm");
            if (commf)
                std::getline(commf, comm);
        }

        fs::path fd_dir = p.path() / "fd";
        std::error_code ec;
        for (const auto &fd : fs::directory_iterator(fd_dir, fs::directory_options::skip_permission_denied, ec))
        {
            if (ec)
                break;
            std::error_code sec;
            auto link = fs::read_symlink(fd.path(), sec);
            if (sec)
                continue;

            std::string t = link.string();
            if (t.rfind("socket:[", 0) != 0)
                continue;
            auto l = t.find('[');
            auto r = t.find(']');
            if (l == std::string::npos || r == std::string::npos || r <= l + 1)
                continue;

            std::string inode = t.substr(l + 1, r - l - 1);
            auto &vec = m[inode];
            bool exists = false;
            for (const auto &row : vec)
            {
                if (row.pid == pid)
                {
                    exists = true;
                    break;
                }
            }
            if (!exists)
                vec.push_back({pid, comm});
        }
    }

    return m;
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

    int filter_port = -1;
    if (argc == 2)
    {
        try
        {
            filter_port = std::stoi(argv[1]);
            if (filter_port < 0 || filter_port > 65535)
                throw std::invalid_argument("bad");
        }
        catch (...)
        {
            std::cerr << "port: port must be an integer in [0, 65535]\n";
            return 1;
        }
    }
    else if (argc > 2)
    {
        std::cerr << "port: too many arguments\n";
        return 1;
    }

    std::vector<SockRow> socks;
    parse_net_table("/proc/net/tcp", "tcp", false, socks);
    parse_net_table("/proc/net/tcp6", "tcp6", true, socks);

    auto procmap = inode_to_processes();

    std::cout << std::left << std::setw(7) << "PROTO"
              << std::setw(28) << "LOCAL"
              << std::setw(8) << "PID"
              << "PROCESS\n";

    int shown = 0;
    for (const auto &s : socks)
    {
        auto p = s.local.rfind(':');
        if (p == std::string::npos)
            continue;
        int local_port = 0;
        try
        {
            local_port = std::stoi(s.local.substr(p + 1));
        }
        catch (...)
        {
            continue;
        }

        if (filter_port >= 0 && local_port != filter_port)
            continue;

        auto it = procmap.find(s.inode);
        if (it == procmap.end())
        {
            std::cout << std::left << std::setw(7) << s.proto
                      << std::setw(28) << s.local
                      << std::setw(8) << "?"
                      << "?\n";
            ++shown;
            continue;
        }

        for (const auto &proc : it->second)
        {
            std::cout << std::left << std::setw(7) << s.proto
                      << std::setw(28) << s.local
                      << std::setw(8) << proc.pid
                      << proc.comm << "\n";
            ++shown;
        }
    }

    if (shown == 0)
    {
        std::cerr << "port: no listening sockets found";
        if (filter_port >= 0)
        {
            std::cerr << " for port " << filter_port;
        }
        std::cerr << "\n";
        return 1;
    }

    return 0;
}
