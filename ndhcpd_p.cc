#include "ndhcpd_p.hpp"

#include <netdb.h>
#include <net/if.h>
#include <sys/eventfd.h>
#include <poll.h>
#include <unistd.h>
#include <string.h>

#include <algorithm>
#include <sstream>
#include <iomanip>

#include <stddef.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>

#include "dhcp_error.hpp"

#include <syslog.h>
#include <arpa/inet.h>

namespace std {
// std::size from C++17
template <class C>
constexpr auto size(const C& c) -> decltype(c.size())
{
    return c.size();
}

template <class T, std::size_t N>
constexpr std::size_t size(const T (&array)[N]) noexcept
{
    return N;
}
}

static std::string mac_to_string(const uint8_t *mac)
{
    std::ostringstream str;
    for(int i=0; i<6; ++i) {
        if(i != 0) {
            str << ':';
        }
        str << std::hex << std::setfill('0') << std::setw(2) << (uint)mac[i];
    }
    return str.str();

}

ndhcpd_private::ndhcpd_private()
    : stop_server(false)
{
    struct servent *port;
    port = getservbyname("bootps", "udp");
    srcAddr.sin_family = AF_INET;
    srcAddr.sin_port = port->s_port;
    srcAddr.sin_addr.s_addr = INADDR_ANY;

    port = getservbyname("bootpc", "udp");
    dstAddr.sin_family = AF_INET;
    dstAddr.sin_port = port->s_port;
    endservent();


}

ndhcpd_private::~ndhcpd_private()
{
    stop(true);
}


bool ndhcpd_private::lease_is_mac_equal::operator ()(const leases_t::value_type &lease)
{
    if(!lease.second)
        return false;
    return memcmp(lease.second->mac.data(), mac, lease.second->mac.size()) == 0;
}

bool ndhcpd_private::lease_is_overdue::operator ()(const leases_t::value_type &lease)
{
    if(!lease.second) {
        // not allocated lease is ok
        return true;
    }
    return (lease.second->lease_start + lease.second->lease_time < now);
}

void ndhcpd_private::log(int severity, const std::string &logStr) const
{
    if(logfn) {
        logfn(severity, logStr);
    }
}

ndhcpd_private::Logger ndhcpd_private::log(int severity) const
{
    return Logger(this, severity);
}

void ndhcpd_private::get_server_id(const Socket &_server)
{
    struct ifreq ifr;
    ifaceName.copy(ifr.ifr_name, std::size(ifr.ifr_name));
    if(ioctl(_server, SIOCGIFADDR, &ifr) == 0) {
        server_id = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
        log(LOG_INFO) << "Bind DHCP server to " << inet_ntoa(server_id);
    }
    else {
        server_id.s_addr = INADDR_NONE;
        log(LOG_INFO) << "Starting DHCP without server IP";
    }
}

void ndhcpd_private::start()
{
    try {
        if(serverThread.joinable()) {
            log(LOG_INFO, "Server thread already started");
            return;
        }
        stop_server = false;
        Socket _server(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

        _server.setsockopt(SOL_SOCKET, SO_REUSEADDR, true);
        _server.setsockopt(SOL_SOCKET, SO_BROADCAST, true);

        if(!ifaceName.empty()) {
            struct ifreq ifr;
            ifaceName.copy(ifr.ifr_name, std::size(ifr.ifr_name));
            //  The resulting character string is not null-terminated.
            ifr.ifr_name[std::size(ifr.ifr_name)-1] = 0;
            _server.setsockopt(SOL_SOCKET, SO_BINDTODEVICE, ifr);
            get_server_id(_server);
        }

        sockaddr_in addr = srcAddr;
        _server.bind(addr);

        File _event(eventfd(0,0));

        std::swap(server, _server);
        std::swap(event, _event);

        serverThread = std::thread(std::mem_fn(&ndhcpd_private::process_dhcp), this);
        log(LOG_INFO) << "Service started";
    }
    catch(const std::system_error &err) {
        log(LOG_ERR) << err.what();
        throw;
    }
}

void ndhcpd_private::stop(bool silent)
{
    if(!silent) {
        log(LOG_INFO) << "Stoping service";
    }
    stop_server = true;
    eventfd_write(event, 1);
    if(serverThread.joinable()) {
        serverThread.join();
        if(!silent) {
            log(LOG_INFO) << "Service stoped";
        }
    }
    else {
        if(!silent) {
            log(LOG_INFO) << "Service already stoped";
        }
    }
    leases.clear();
    server.close();
    event.close();
}

void ndhcpd_private::process_dhcp()
{
    try {
        std::vector<struct pollfd> pollFds = {
            { event, POLLIN|POLLERR },
            { server, POLLIN|POLLERR }
        };

        while(!stop_server) {
            auto ret = poll(pollFds.data(), pollFds.size(), -1);
            if(ret < 0) {
                throw std::system_error(errno, std::system_category(), "poll()");
            }
            if(pollFds[0].revents != 0) {
                // event handle has been raised
                eventfd_t val;
                eventfd_read(event, &val);
                // check stop_server variable
                continue;
            }
            std::for_each(pollFds.cbegin()+1, pollFds.cend(), [this](const pollfd& fd){
               if(fd.revents & (POLLERR|POLLHUP|POLLNVAL)) {
                   log(LOG_ERR) << "Socket " << fd.fd << " in error state";
               }
               else if(fd.revents & POLLIN) {
                   try {
                       if(server_id.s_addr == INADDR_NONE) {
                           get_server_id(server);
                       }
                       struct dhcp_packet in_packet;
                       in_packet = recieve_packet(fd.fd);
                       struct dhcp_packet out_packet = process_packet(in_packet);
                       send_packet(fd.fd, out_packet);
                   }
                   catch(const std::system_error &err) {
                       log(LOG_ERR) << err.what();
                   }
               }
            });
        }
    }
    catch(const std::exception &err) {
        log(LOG_ERR) << err.what();
    }
}

dhcp_packet ndhcpd_private::recieve_packet(int fd)
{
    dhcp_packet packet;
    ssize_t len = read(fd, &packet, sizeof(packet));
    if(len < 0) {
        throw std::system_error(errno, std::system_category(), "read()");
    }
    if(len < (ssize_t)offsetof(dhcp_packet, options) ||
            packet.cookie != htonl(dhcp_packet::cookie_value_he)) {
        throw std::system_error(make_error_code(dhcp_error::invalid_packet), "recieve_packet()");
    }
    if(packet.htype != ARPHRD_ETHER ||
            packet.hlen != 6) {
        throw std::system_error(make_error_code(dhcp_error::invalid_hwtype), "recieve_packet()");
    }
    if(packet.op != dhcp_packet::_op::BOOTREQUEST) {
        throw std::system_error(make_error_code(dhcp_error::unexpected_packet_type), "recieve_packet()");
    }

    return packet;
}

dhcp_packet ndhcpd_private::process_packet(const dhcp_packet &packet)
{
    const dhcp_message_type *msgType = static_cast<const dhcp_message_type *>(dhcp_get_option(packet, dhcp_option::_code::message_type));
    if(!msgType) {
        throw std::system_error(make_error_code(dhcp_error::invalid_packet), "process_packet()");
    }
    if(*msgType < dhcp_message_type::minval
            || *msgType > dhcp_message_type::maxval) {
        throw std::system_error(make_error_code(dhcp_error::invalid_packet), "process_packet()");
    }

    log(LOG_INFO) << "Recieved " << dhcp_message_type_name(*msgType) << " from " << mac_to_string(packet.chaddr);

    switch(*msgType) {
    case dhcp_message_type::discover:
        return make_offer(packet);
    case dhcp_message_type::request:
        return process_ip_request(packet);
    default:
        throw std::system_error(make_error_code(dhcp_error::unexpected_packet_type), "process_packet()");
    }

}

void ndhcpd_private::send_packet(int fd, const dhcp_packet &packet)
{
    uint32_t ciaddr;
    if((packet.flags & htons(BROADCAST_FLAG))
            || packet.ciaddr == 0
            ) {
        ciaddr = INADDR_BROADCAST;
    }
    else {
        ciaddr = packet.ciaddr;
    }

    struct sockaddr_in addr = dstAddr;
    addr.sin_addr.s_addr = ciaddr;
    ssize_t ret = sendto(fd, &packet, sizeof(packet), 0, (const sockaddr*)&addr, sizeof(addr));
    if(ret < 0)
        throw std::system_error(errno, std::system_category(), "sendto()");

    log(LOG_INFO) << "Sent " << dhcp_message_type_name(*static_cast<const dhcp_message_type *>(dhcp_get_option(packet, dhcp_option::_code::message_type))) << " to " << mac_to_string(packet.chaddr);

}

dhcp_packet ndhcpd_private::make_offer(const dhcp_packet &packet)
{
    dhcp_packet out_packet;
    memset(&out_packet, 0, sizeof(out_packet));
    out_packet.op = dhcp_packet::_op::BOOTREPLY;
    out_packet.htype = ARPHRD_ETHER;
    out_packet.hlen = 6;
    out_packet.xid = packet.xid;
    memcpy(out_packet.chaddr, packet.chaddr, sizeof(out_packet.chaddr));
    out_packet.flags = packet.flags;
    out_packet.ciaddr = packet.ciaddr;
    out_packet.cookie = (dhcp_packet::_cookie)htonl(dhcp_packet::cookie_value_he);
    out_packet.options[0] = (uint8_t)dhcp_option::_code::end;
    dhcp_add_option(&out_packet, dhcp_option::_code::message_type, dhcp_message_type::offer);
    dhcp_add_option(&out_packet, dhcp_option::_code::server_id, server_id);

    // Find lease with same MAC-address
    leases_t::iterator leaseIter = std::find_if(leases.begin(), leases.end(), lease_is_mac_equal(packet.chaddr));

    if(leaseIter == leases.end()) {
        leaseIter = std::find_if(leases.begin(), leases.end(), lease_is_overdue(std::chrono::steady_clock::now()));
    }

    if(leaseIter == leases.end()) {
        throw std::system_error(make_error_code(dhcp_error::no_more_leases), "make_offer()");
    }

    leaseIter->second.reset(new lease_data(packet.chaddr, std::chrono::seconds(60))); // Set lease for offer time (60 sec)

    out_packet.yiaddr = htonl(leaseIter->first.ip);
    dhcp_add_option(&out_packet, dhcp_option::_code::lease_time, htonl(leaseIter->second->lease_time.count()));
    dhcp_add_option(&out_packet, dhcp_option::_code::subnet_mask, htonl(leaseIter->first.subnet));
    in_addr addr = {out_packet.yiaddr};
    log(LOG_INFO) << "Make offer for " << inet_ntoa(addr) << " to " << mac_to_string(out_packet.chaddr);
    return out_packet;
}

dhcp_packet ndhcpd_private::process_ip_request(const dhcp_packet &packet)
{
    const uint32_t *server_id_opt = (uint32_t *)dhcp_get_option(packet, dhcp_option::_code::server_id);
    const uint32_t *requested_ip_opt = (uint32_t *)dhcp_get_option(packet, dhcp_option::_code::requested_ip);

    uint32_t requested_ip;
    if(requested_ip_opt) {
        requested_ip = ntohl(*requested_ip_opt);
    }
    else {
        requested_ip = ntohl(packet.ciaddr);
        if(requested_ip == 0) {
            throw std::system_error(make_error_code(dhcp_error::no_ip_requested), "process_ip_request()");
        }
    }

    leases_t::iterator leaseIter = std::find_if(leases.begin(), leases.end(), lease_is_mac_equal(packet.chaddr));
    if(leaseIter != leases.end() && leaseIter->first.ip == requested_ip) {
        // client requested or configured IP matches the lease.
        // ACK it, and bump lease expiration time.
        in_addr addr = {htonl(requested_ip)};
        log(LOG_INFO) << "Acknowledge request for " << inet_ntoa(addr) << " to " << mac_to_string(packet.chaddr);
        return ack_packet(packet, &(*leaseIter));
    }

    // No lease for this MAC, or lease IP != requested IP

    if(server_id_opt // client is in SELECTING state
            || requested_ip_opt // client is in INIT-REBOOT state
            ) {
        // "No, we don't have this IP for you"
        log(LOG_INFO) << "Not acknowledge request to " << mac_to_string(packet.chaddr);
        return nak_packet(packet);
    }

    // client is in RENEWING or REBINDING, with wrong IP address.
    // do not answer
    throw std::system_error(make_error_code(dhcp_error::invalid_packet), "process_ip_request()");
}

dhcp_packet ndhcpd_private::ack_packet(const dhcp_packet &packet, leases_t::value_type *lease)
{
    dhcp_packet out_packet;
    memset(&out_packet, 0, sizeof(out_packet));
    out_packet.op = dhcp_packet::_op::BOOTREPLY;
    out_packet.htype = ARPHRD_ETHER;
    out_packet.hlen = 6;
    out_packet.xid = packet.xid;
    memcpy(out_packet.chaddr, packet.chaddr, sizeof(out_packet.chaddr));
    out_packet.flags = packet.flags;
    out_packet.ciaddr = packet.ciaddr;
    out_packet.cookie = (dhcp_packet::_cookie)htonl(dhcp_packet::cookie_value_he);
    out_packet.options[0] = (uint8_t)dhcp_option::_code::end;
    dhcp_add_option(&out_packet, dhcp_option::_code::message_type, dhcp_message_type::ack);
    dhcp_add_option(&out_packet, dhcp_option::_code::server_id, server_id);

    lease->second.reset(new lease_data(packet.chaddr, std::chrono::hours(1))); // Set lease for lease time (1hour)

    out_packet.yiaddr = htonl(lease->first.ip);
    dhcp_add_option(&out_packet, dhcp_option::_code::lease_time, htonl(lease->second->lease_time.count()));
    dhcp_add_option(&out_packet, dhcp_option::_code::subnet_mask, htonl(lease->first.subnet));
    return out_packet;
}

dhcp_packet ndhcpd_private::nak_packet(const dhcp_packet &packet)
{
    dhcp_packet out_packet;
    memset(&out_packet, 0, sizeof(out_packet));
    out_packet.op = dhcp_packet::_op::BOOTREPLY;
    out_packet.htype = ARPHRD_ETHER;
    out_packet.hlen = 6;
    out_packet.xid = packet.xid;
    memcpy(out_packet.chaddr, packet.chaddr, sizeof(out_packet.chaddr));
    out_packet.flags = packet.flags;
    out_packet.ciaddr = packet.ciaddr;
    out_packet.cookie = (dhcp_packet::_cookie)htonl(dhcp_packet::cookie_value_he);
    out_packet.options[0] = (uint8_t)dhcp_option::_code::end;
    dhcp_add_option(&out_packet, dhcp_option::_code::message_type, dhcp_message_type::nak);
    dhcp_add_option(&out_packet, dhcp_option::_code::server_id, server_id);
    return out_packet;
}
