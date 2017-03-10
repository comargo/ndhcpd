// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ndhcpd.hpp>
#include "ndhcpd_p.hpp"

#include <arpa/inet.h>
#include <algorithm>

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

void ndhcpd::addRange(const std::string &from, const std::string &to, const std::string &mask)
{
    in_addr_t n_addr_from = inet_addr(from.c_str());
    in_addr_t n_addr_to = inet_addr(to.c_str());
    in_addr_t n_addr_mask = inet_addr(mask.c_str());

    return addRange(ntohl(n_addr_from), ntohl(n_addr_to), ntohl(n_addr_mask));
}

void ndhcpd::addRange(uint32_t from, uint32_t to, uint32_t mask)
{
    for(uint32_t ip = min(from, to); ip <= max(from,to); ++ip) {
        addIp(ip, mask);
    }
}

void ndhcpd::addIp(const std::string &ip, const std::string &mask)
{
    in_addr_t n_addr = inet_addr(ip.c_str());
    in_addr_t n_addr_mask = inet_addr(mask.c_str());

    return addIp(ntohl(n_addr), ntohl(n_addr_mask));
}

void ndhcpd::addIp(uint32_t ip, uint32_t mask)
{
    if(mask <= 32) {
        // mask len provided instead of mask
        mask = ~((1<<(32-mask))-1);
    }
    d->leases.emplace(ndhcpd_private::ipinfo(ip, mask), nullptr);
}

std::vector<uint32_t> ndhcpd::ips() const
{
    std::vector<uint32_t> out;
    out.reserve(d->leases.size());
    for(auto &value : d->leases) {
        out.push_back(value.first.ip);
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

// C interface implementation
#include <ndhcpd.h>
ndhcpd_t ndhcpd_create() __THROW
{
    try {
        return reinterpret_cast<ndhcpd_t>(new ndhcpd());
    }
    catch(...) {
        return nullptr;
    }
}

void ndhcpd_delete(ndhcpd_t _ndhcpd) __THROW
{
    ndhcpd* p = reinterpret_cast<ndhcpd*>(_ndhcpd);
    delete p;
}

void ndhcpd_setInterfaceName(ndhcpd_t _ndhcpd, const char *ifaceName) __THROW
{
    ndhcpd* p = reinterpret_cast<ndhcpd*>(_ndhcpd);
    p->setInterfaceName(ifaceName);
}

void ndhcpd_addRange_s(ndhcpd_t _ndhcpd, const char *from, const char *to, const char *mask) __THROW
{
    ndhcpd* p = reinterpret_cast<ndhcpd*>(_ndhcpd);
    p->addRange(from, to, mask);
}

void ndhcpd_addRange_i(ndhcpd_t _ndhcpd, uint32_t from, uint32_t to, uint32_t mask) __THROW
{
    ndhcpd* p = reinterpret_cast<ndhcpd*>(_ndhcpd);
    p->addRange(from, to, mask);
}

void ndhcpd_addIp_s(ndhcpd_t _ndhcpd, const char *ip, const char *mask) __THROW
{
    ndhcpd* p = reinterpret_cast<ndhcpd*>(_ndhcpd);
    p->addIp(ip, mask);
}

void ndhcpd_addIp_i(ndhcpd_t _ndhcpd, uint32_t ip, uint32_t mask) __THROW
{
    ndhcpd* p = reinterpret_cast<ndhcpd*>(_ndhcpd);
    p->addIp(ip, mask);
}

int ndhcpd_ips(const ndhcpd_t _ndhcpd, uint32_t *ips, size_t ipsCount) __THROW
{
    const ndhcpd* p = reinterpret_cast<const ndhcpd*>(_ndhcpd);
    if(!ips) {
        return p->ips().size();
    }

    auto vIps = p->ips();
    std::copy_n(vIps.begin(), std::min(ipsCount, vIps.size()), ips);
    return std::min(ipsCount, vIps.size());
}

int ndhcpd_start(ndhcpd_t _ndhcpd) __THROW
{
    try {
        ndhcpd* p = reinterpret_cast<ndhcpd*>(_ndhcpd);
        p->start();
        return 0;
    }
    catch(const std::system_error &err) {
        return err.code().value();
    }
    catch(...) {
        return -1;
    }
}

int ndhcpd_stop(ndhcpd_t _ndhcpd) __THROW
{
    try {
        ndhcpd* p = reinterpret_cast<ndhcpd*>(_ndhcpd);
        p->stop();
        return 0;
    }
    catch(const std::system_error &err) {
        return err.code().value();
    }
    catch(...) {
        return -1;
    }
}

int ndhcpd_isStarted(const ndhcpd_t _ndhcpd) __THROW
{
    ndhcpd* p = reinterpret_cast<ndhcpd*>(_ndhcpd);
    return p->isStarted()?1:0;
}

void ndhcpd_setLog(ndhcpd_t _ndhcpd, ndhcpd_logfn_t logfn) __THROW
{
    ndhcpd* p = reinterpret_cast<ndhcpd*>(_ndhcpd);
    p->setLog([logfn](int level, std::string msg){return logfn(level, msg.c_str());});
}
