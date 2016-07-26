#ifndef __NDHCPD_P_HPP__
#define __NDHCPD_P_HPP__

#include <ndhcpd.hpp>
#include <array>
#include <chrono>
#include <map>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <string>
#include <sstream>

#include <netinet/in.h>

#include "file.hpp"
#include "socket.hpp"

#include "dhcp_packet.hpp"


class ndhcpd_private
{
public:
    ndhcpd_private();
    ~ndhcpd_private();
public:
    struct lease_data {
        lease_data(const uint8_t *_mac, std::chrono::seconds _lease_time)
            : lease_time(_lease_time)
            , lease_start(std::chrono::steady_clock::now())
        {
            std::copy(_mac, _mac+6, mac.begin());
        }

        std::array<uint8_t,6> mac;
        std::chrono::seconds lease_time;
        std::chrono::steady_clock::time_point lease_start;
    };

    ndhcpd::logfn_t logfn;
    typedef std::map<uint32_t, std::unique_ptr<lease_data>> leases_t;
    leases_t leases;

    struct lease_is_mac_equal {
        lease_is_mac_equal(const uint8_t *mac)
            : mac(mac)
        {}

        bool operator ()(const leases_t::value_type &lease);
        const uint8_t *mac;
    };

    struct lease_is_overdue {
        lease_is_overdue(const std::chrono::steady_clock::time_point &now)
            : now(now)
        {}

        bool operator ()(const leases_t::value_type &lease);
        std::chrono::steady_clock::time_point now;
    };

    void log(int severity, const std::string &logStr) const;
    class Logger
    {
    public:
        Logger(const ndhcpd_private *that, int severity)
            : stream(new std::ostringstream)
            , that(that)
            , severity(severity)
        { }
        Logger(Logger &&) = default;

        ~Logger()
        {
            that->log(severity, stream->str());
        }
        template <class T>
        std::ostream& operator<<(const T &msg) {
            return (*stream) << msg;
        }

    private:
        std::unique_ptr<std::ostringstream> stream;
        const ndhcpd_private *that;
        int severity;
    };
    Logger log(int severity) const;

    void start();
    void stop(bool silent = false);

    void process_dhcp();

    // packet workflow
    struct dhcp_packet recieve_packet(int fd);
    struct dhcp_packet process_packet(const struct dhcp_packet &packet);
    void send_packet(int fd, const struct dhcp_packet &packet);

    //packet processors
    struct dhcp_packet make_offer(const struct dhcp_packet &packet);
    struct dhcp_packet process_ip_request(const struct dhcp_packet &packet);

    // output packet generator
    struct dhcp_packet ack_packet(const struct dhcp_packet &packet, leases_t::value_type *lease);
    struct dhcp_packet nak_packet(const struct dhcp_packet &packet);



    std::thread serverThread;
    Socket server;
    in_addr server_id;
    File event;
    bool stop_server;

    struct sockaddr_in srcAddr;
    struct sockaddr_in dstAddr;
    std::string ifaceName;


};

#endif//__NDHCPD_P_HPP__
