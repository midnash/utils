#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

static const std::vector<std::string> CAP_NAMES = {
    "cap_chown",
    "cap_dac_override",
    "cap_dac_read_search",
    "cap_fowner",
    "cap_fsetid",
    "cap_kill",
    "cap_setgid",
    "cap_setuid",
    "cap_setpcap",
    "cap_linux_immutable",
    "cap_net_bind_service",
    "cap_net_broadcast",
    "cap_net_admin",
    "cap_net_raw",
    "cap_ipc_lock",
    "cap_ipc_owner",
    "cap_sys_module",
    "cap_sys_rawio",
    "cap_sys_chroot",
    "cap_sys_ptrace",
    "cap_sys_pacct",
    "cap_sys_admin",
    "cap_sys_boot",
    "cap_sys_nice",
    "cap_sys_resource",
    "cap_sys_time",
    "cap_sys_tty_config",
    "cap_mknod",
    "cap_lease",
    "cap_audit_write",
    "cap_audit_control",
    "cap_setfcap",
    "cap_mac_override",
    "cap_mac_admin",
    "cap_syslog",
    "cap_wake_alarm",
    "cap_block_suspend",
    "cap_audit_read",
    "cap_perfmon",
    "cap_bpf",
    "cap_checkpoint_restore"};

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " [PID|FILE]\n"
        << "\n"
        << "Print Linux capabilities in a human-readable form for a process or file.\n"
        << "If no argument is provided, current process capabilities are shown.\n";
}

static bool is_all_digits(const std::string &s)
{
    if (s.empty())
        return false;
    for (unsigned char c : s)
    {
        if (std::isdigit(c) == 0)
            return false;
    }
    return true;
}

static std::vector<std::string> decode_caps_hex(const std::string &hex)
{
    unsigned long long mask = 0;
    try
    {
        mask = std::stoull(hex, nullptr, 16);
    }
    catch (...)
    {
        return {};
    }

    std::vector<std::string> out;
    for (std::size_t i = 0; i < CAP_NAMES.size(); ++i)
    {
        if (mask & (1ULL << i))
            out.push_back(CAP_NAMES[i]);
    }
    return out;
}

static std::string pretty_set_name(const std::string &raw)
{
    if (raw == "CapInh")
        return "Inheritable";
    if (raw == "CapPrm")
        return "Permitted";
    if (raw == "CapEff")
        return "Effective";
    if (raw == "CapBnd")
        return "Bounding";
    if (raw == "CapAmb")
        return "Ambient";
    return raw;
}

static void print_caps_wrapped(const std::vector<std::string> &caps, std::size_t indent = 13, std::size_t width = 100)
{
    std::string pad(indent, ' ');
    if (caps.empty())
    {
        std::cout << pad << "(none)\n";
        return;
    }

    std::cout << pad;
    std::size_t col = indent;

    for (std::size_t i = 0; i < caps.size(); ++i)
    {
        std::string item = caps[i];
        if (i + 1 < caps.size())
            item += ", ";

        if (col + item.size() > width)
        {
            std::cout << "\n"
                      << pad;
            col = indent;
        }

        std::cout << item;
        col += item.size();
    }

    std::cout << "\n";
}

static void print_cap_set(const std::string &key, const std::string &raw_hex)
{
    auto decoded = decode_caps_hex(raw_hex);
    std::cout << std::left << std::setw(12) << pretty_set_name(key)
              << " raw=" << raw_hex
              << "  count=" << decoded.size() << "\n";
    print_caps_wrapped(decoded);
}

static int show_process_caps(const std::string &pid_token)
{
    std::ifstream in("/proc/" + pid_token + "/status");
    if (!in)
    {
        std::cerr << "caps: cannot read /proc/" << pid_token << "/status\n";
        return 1;
    }

    std::map<std::string, std::string> raw;
    std::string line;
    while (std::getline(in, line))
    {
        auto p = line.find(':');
        if (p == std::string::npos)
            continue;
        std::string k = line.substr(0, p);
        std::string v = line.substr(p + 1);
        while (!v.empty() && std::isspace(static_cast<unsigned char>(v.front())))
            v.erase(v.begin());
        raw[k] = v;
    }

    const std::vector<std::string> keys = {"CapInh", "CapPrm", "CapEff", "CapBnd", "CapAmb"};
    std::cout << "Process capabilities\n";
    std::cout << "PID: " << raw["Pid"] << "\n\n";
    for (const auto &k : keys)
    {
        print_cap_set(k, raw[k]);
    }
    return 0;
}

static int show_file_caps(const std::string &path)
{
    std::string cmd = "getcap -n '" + path + "' 2>/dev/null";
    FILE *fp = popen(cmd.c_str(), "r");
    if (!fp)
    {
        std::cerr << "caps: failed to run getcap\n";
        return 1;
    }

    std::string out;
    char buf[1024];
    while (fgets(buf, sizeof(buf), fp))
        out += buf;
    int rc = pclose(fp);

    if (rc != 0 || out.empty())
    {
        std::cout << path << ": (no file capabilities)\n";
        return 0;
    }

    std::cout << out;
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 2)
    {
        std::string a = argv[1];
        if (a == "-h" || a == "--help")
        {
            print_help(argv[0]);
            return 0;
        }
    }

    if (argc == 1)
        return show_process_caps("self");

    if (argc != 2)
    {
        print_help(argv[0]);
        return 1;
    }

    std::string target = argv[1];
    if (is_all_digits(target))
        return show_process_caps(target);

    return show_file_caps(target);
}
