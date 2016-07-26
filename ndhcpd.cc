#include <ndhcpd.hpp>
#include "ndhcpd_p.hpp"

#include <arpa/inet.h>

using std::min;
using std::max;

ndhcpd::ndhcpd()
    : d(new ndhcpd_private)
{
}

ndhcpd::~ndhcpd()
{
}

void ndhcpd::setInterfaceName(const std::string &ifaceName)
{
    d->ifaceName = ifaceName;
}

void ndhcpd::addRange(const std::string &from, const std::string &to)
{
    in_addr_t n_addr_from = inet_addr(from.c_str());
    in_addr_t n_addr_to = inet_addr(to.c_str());

    return addRange(ntohl(n_addr_from), ntohl(n_addr_to));
}

void ndhcpd::addRange(uint32_t from, uint32_t to)
{
    for(uint32_t ip = min(from, to); ip <= max(from,to); ++ip) {
        addIp(ip);
    }
}

void ndhcpd::addIp(const std::string &ip)
{
    in_addr_t n_addr = inet_addr(ip.c_str());

    return addIp(ntohl(n_addr));
}

void ndhcpd::addIp(uint32_t ip)
{
    d->leases.emplace(ip, nullptr);
}

std::vector<uint32_t> ndhcpd::ips() const
{
    std::vector<uint32_t> out;
    out.reserve(d->leases.size());
    for(auto &value : d->leases) {
        out.push_back(value.first);
    }
    return out;
}

void ndhcpd::start()
{
    d->start();
}

void ndhcpd::stop()
{
    d->stop();
}

bool ndhcpd::isStarted() const
{
    return d->serverThread.joinable();
}

void ndhcpd::setLog(ndhcpd::logfn_t logfn)
{
    d->logfn = logfn;
}
