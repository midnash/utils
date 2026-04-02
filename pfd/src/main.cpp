#include <algorithm>
#include <arpa/inet.h>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <unistd.h>

namespace fs = std::filesystem;

struct FdEntry
{
    int fd = -1;
    std::string type;
    std::string target;
};

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " [PID]\n"
        << "\n"
        << "Show open file descriptors for a process in a readable format.\n"
        << "If PID is omitted, current process PID is used.\n";
}

static std::vector<std::string> split_ws(const std::string &s)
{
    std::istringstream iss(s);
    std::vector<std::string> out;
    std::string tok;
    while (iss >> tok)
    {
        out.push_back(tok);
    }
    return out;
}

static std::string decode_ipv4_hex(const std::string &hex)
{
    if (hex.size() != 8)
    {
        return hex;
    }
    unsigned long raw = std::stoul(hex, nullptr, 16);
    std::ostringstream oss;
    oss << (raw & 0xFF) << '.' << ((raw >> 8) & 0xFF) << '.' << ((raw >> 16) & 0xFF) << '.' << ((raw >> 24) & 0xFF);
    return oss.str();
}

static std::string decode_ipv6_hex(const std::string &hex)
{
    if (hex.size() != 32)
    {
        return hex;
    }

    unsigned char bytes[16] = {0};
    for (int i = 0; i < 16; ++i)
    {
        std::string b = hex.substr(i * 2, 2);
        bytes[i] = static_cast<unsigned char>(std::stoul(b, nullptr, 16));
    }

    // Linux procfs stores each 32-bit chunk in little-endian order.
    for (int i = 0; i < 16; i += 4)
    {
        std::swap(bytes[i], bytes[i + 3]);
        std::swap(bytes[i + 1], bytes[i + 2]);
    }

    char out[INET6_ADDRSTRLEN] = {0};
    if (!inet_ntop(AF_INET6, bytes, out, sizeof(out)))
    {
        return hex;
    }
    return out;
}

static std::string parse_proc_addr(const std::string &token, bool v6)
{
    auto pos = token.find(':');
    if (pos == std::string::npos)
    {
        return token;
    }
    std::string ip_hex = token.substr(0, pos);
    std::string port_hex = token.substr(pos + 1);

    unsigned long port = 0;
    try
    {
        port = std::stoul(port_hex, nullptr, 16);
    }
    catch (...)
    {
        return token;
    }

    std::string ip = v6 ? decode_ipv6_hex(ip_hex) : decode_ipv4_hex(ip_hex);
    std::ostringstream oss;
    oss << ip << ':' << port;
    return oss.str();
}

static std::unordered_map<std::string, std::string> tcp_states()
{
    return {
        {"01", "ESTABLISHED"}, {"02", "SYN_SENT"}, {"03", "SYN_RECV"}, {"04", "FIN_WAIT1"}, {"05", "FIN_WAIT2"}, {"06", "TIME_WAIT"}, {"07", "CLOSE"}, {"08", "CLOSE_WAIT"}, {"09", "LAST_ACK"}, {"0A", "LISTEN"}, {"0B", "CLOSING"}};
}

static void parse_inet_table(const fs::path &table, const std::string &proto, bool v6,
                             std::unordered_map<std::string, std::string> &out)
{
    std::ifstream f(table);
    if (!f)
    {
        return;
    }

    std::string line;
    std::getline(f, line); // header

    const auto states = tcp_states();

    while (std::getline(f, line))
    {
        auto cols = split_ws(line);
        if (cols.size() < 10)
        {
            continue;
        }

        std::string local = parse_proc_addr(cols[1], v6);
        std::string remote = parse_proc_addr(cols[2], v6);
        std::string state_code = cols[3];
        std::string inode = cols[9];

        std::string state_text = state_code;
        auto it = states.find(state_code);
        if (proto.rfind("tcp", 0) == 0 && it != states.end())
        {
            state_text = it->second;
        }

        std::ostringstream desc;
        desc << proto << " " << local << " -> " << remote;
        if (proto.rfind("tcp", 0) == 0)
        {
            desc << " (" << state_text << ")";
        }
        out[inode] = desc.str();
    }
}

static void parse_unix_table(const fs::path &table, std::unordered_map<std::string, std::string> &out)
{
    std::ifstream f(table);
    if (!f)
    {
        return;
    }

    std::string line;
    std::getline(f, line); // header

    while (std::getline(f, line))
    {
        auto cols = split_ws(line);
        if (cols.size() < 7)
        {
            continue;
        }

        std::string inode = cols[6];
        std::string path;
        if (cols.size() > 7)
        {
            path = cols[7];
        }

        if (!path.empty())
        {
            out[inode] = "unix " + path;
        }
        else
        {
            out[inode] = "unix";
        }
    }
}

static std::unordered_map<std::string, std::string> build_socket_map(int pid)
{
    fs::path net_root = fs::path("/proc") / std::to_string(pid) / "net";
    std::unordered_map<std::string, std::string> m;

    parse_inet_table(net_root / "tcp", "tcp", false, m);
    parse_inet_table(net_root / "tcp6", "tcp6", true, m);
    parse_inet_table(net_root / "udp", "udp", false, m);
    parse_inet_table(net_root / "udp6", "udp6", true, m);
    parse_unix_table(net_root / "unix", m);

    return m;
}

static std::vector<FdEntry> collect_fds(int pid)
{
    std::vector<FdEntry> rows;
    fs::path fd_dir = fs::path("/proc") / std::to_string(pid) / "fd";
    auto socket_map = build_socket_map(pid);

    std::error_code ec;
    if (!fs::exists(fd_dir, ec) || ec)
    {
        return rows;
    }

    for (const auto &entry : fs::directory_iterator(fd_dir, fs::directory_options::skip_permission_denied, ec))
    {
        if (ec)
        {
            break;
        }

        int fd_num = -1;
        try
        {
            fd_num = std::stoi(entry.path().filename().string());
        }
        catch (...)
        {
            continue;
        }

        std::string target;
        std::error_code sec;
        auto link = fs::read_symlink(entry.path(), sec);
        target = sec ? "<unreadable>" : link.string();

        std::string type = "other";
        std::string detail = target;

        if (target.rfind("socket:[", 0) == 0)
        {
            type = "socket";
            auto start = target.find('[');
            auto end = target.find(']');
            if (start != std::string::npos && end != std::string::npos && end > start + 1)
            {
                std::string inode = target.substr(start + 1, end - start - 1);
                auto it = socket_map.find(inode);
                if (it != socket_map.end())
                {
                    detail = it->second;
                }
            }
        }
        else if (target.rfind("pipe:[", 0) == 0)
        {
            type = "pipe";
        }
        else if (target.rfind("anon_inode:[", 0) == 0)
        {
            type = "anon_inode";
        }
        else if (!target.empty() && target[0] == '/')
        {
            type = "file";
        }

        rows.push_back({fd_num, type, detail});
    }

    std::sort(rows.begin(), rows.end(), [](const FdEntry &a, const FdEntry &b)
              { return a.fd < b.fd; });

    return rows;
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

    int pid = static_cast<int>(::getpid());
    if (argc >= 2)
    {
        try
        {
            pid = std::stoi(argv[1]);
            if (pid <= 0)
            {
                throw std::invalid_argument("non-positive");
            }
        }
        catch (...)
        {
            std::cerr << "pfd: PID must be a positive integer\n";
            return 1;
        }
    }
    if (argc > 2)
    {
        std::cerr << "pfd: too many arguments\n";
        return 1;
    }

    auto rows = collect_fds(pid);
    if (rows.empty())
    {
        std::cerr << "pfd: no descriptors found or access denied for PID " << pid << "\n";
        return 1;
    }

    std::cout << "PID " << pid << '\n';
    std::cout << std::left << std::setw(6) << "FD" << std::setw(12) << "TYPE" << "TARGET\n";
    for (const auto &row : rows)
    {
        std::cout << std::left << std::setw(6) << row.fd << std::setw(12) << row.type << row.target << '\n';
    }

    return 0;
}
