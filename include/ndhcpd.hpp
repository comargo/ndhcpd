#ifndef __NDHCPD_HPP__
#define __NDHCPD_HPP__

#include <stdint.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>

class ndhcpd_private;

class ndhcpd {
public:
    ndhcpd();
    ~ndhcpd();

    ndhcpd(const ndhcpd&) = delete;
    ndhcpd& operator=(const ndhcpd&) = delete;

public:
    void setInterfaceName(const std::string& ifaceName);
    void addRange(const std::string &from, const std::string &to);
    void addRange(uint32_t from, uint32_t to); // in host endiannes
    void addIp(const std::string &ip);
    void addIp(uint32_t ip);
    std::vector<uint32_t> ips() const;

public:
    void start();
    void stop();
    bool isStarted() const;

public:
    typedef std::function<void(int, const std::string&)> logfn_t;
    void setLog(logfn_t logfn);

private:
    std::unique_ptr<ndhcpd_private> d;
};


#endif//__NDHCPD_HPP__
