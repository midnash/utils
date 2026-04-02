#include <algorithm>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <iomanip>
#include <iostream>
#include <net/if.h>
#include <set>
#include <string>
#include <tuple>
#include <vector>

struct Row
{
    std::string iface;
    std::string family;
    std::string address;
};

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " [--all]\n"
        << "\n"
        << "Print local IP addresses in a clean interface-oriented format.\n"
        << "\n"
        << "Options:\n"
        << "  -a, --all   Include loopback and down interfaces\n"
        << "  -h, --help  Show this help\n";
}

static std::string format_address(const sockaddr *sa)
{
    char buf[INET6_ADDRSTRLEN] = {0};
    if (sa->sa_family == AF_INET)
    {
        const auto *in = reinterpret_cast<const sockaddr_in *>(sa);
        if (inet_ntop(AF_INET, &in->sin_addr, buf, sizeof(buf)))
        {
            return buf;
        }
    }
    else if (sa->sa_family == AF_INET6)
    {
        const auto *in6 = reinterpret_cast<const sockaddr_in6 *>(sa);
        if (inet_ntop(AF_INET6, &in6->sin6_addr, buf, sizeof(buf)))
        {
            return buf;
        }
    }
    return {};
}

int main(int argc, char **argv)
{
    bool include_all = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            print_help(argv[0]);
            return 0;
        }
        if (arg == "-a" || arg == "--all")
        {
            include_all = true;
            continue;
        }
        std::cerr << "lip: unknown option '" << arg << "'\n";
        return 1;
    }

    ifaddrs *ifaddr = nullptr;
    if (getifaddrs(&ifaddr) != 0 || !ifaddr)
    {
        std::perror("lip: getifaddrs");
        return 1;
    }

    std::vector<Row> rows;
    std::set<std::tuple<std::string, std::string, std::string>> seen;

    for (ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_name || !ifa->ifa_addr)
        {
            continue;
        }

        int fam = ifa->ifa_addr->sa_family;
        if (fam != AF_INET && fam != AF_INET6)
        {
            continue;
        }

        bool is_up = (ifa->ifa_flags & IFF_UP) != 0;
        bool is_loopback = (ifa->ifa_flags & IFF_LOOPBACK) != 0;
        if (!include_all && (!is_up || is_loopback))
        {
            continue;
        }

        std::string addr = format_address(ifa->ifa_addr);
        if (addr.empty())
        {
            continue;
        }

        std::string family = fam == AF_INET ? "ipv4" : "ipv6";
        auto key = std::make_tuple(std::string(ifa->ifa_name), family, addr);
        if (seen.insert(key).second)
        {
            rows.push_back({ifa->ifa_name, family, addr});
        }
    }

    freeifaddrs(ifaddr);

    std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b)
              {
        if (a.iface != b.iface) {
            return a.iface < b.iface;
        }
        if (a.family != b.family) {
            return a.family < b.family;
        }
        return a.address < b.address; });

    if (rows.empty())
    {
        std::cout << "No local interface addresses found.\n";
        return 1;
    }

    std::size_t iface_w = std::string("INTERFACE").size();
    std::size_t family_w = std::string("FAMILY").size();
    for (const auto &row : rows)
    {
        iface_w = std::max(iface_w, row.iface.size());
        family_w = std::max(family_w, row.family.size());
    }

    std::cout << std::left << std::setw(static_cast<int>(iface_w + 2)) << "INTERFACE"
              << std::setw(static_cast<int>(family_w + 2)) << "FAMILY"
              << "ADDRESS\n";
    for (const auto &row : rows)
    {
        std::cout << std::left << std::setw(static_cast<int>(iface_w + 2)) << row.iface
                  << std::setw(static_cast<int>(family_w + 2)) << row.family
                  << row.address << '\n';
    }

    return 0;
}
