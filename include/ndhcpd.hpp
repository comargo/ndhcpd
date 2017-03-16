#ifndef NDHCPD_HPP
#define NDHCPD_HPP

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
    void addRange(const std::string &from, const std::string &to, const std::string &mask);
    void addRange(uint32_t from, uint32_t to, uint32_t mask); // in host endiannes
    void addIp(const std::string &ip, const std::string &mask);
    void addIp(uint32_t ip, uint32_t mask);
    std::vector<uint32_t> ips() const;

public:
    void start();
    void stop();
    bool isStarted() const;

private:
    std::unique_ptr<ndhcpd_private> d;
};


#endif//NDHCPD_HPP
