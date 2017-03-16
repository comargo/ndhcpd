// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "socket.hpp"

#include <sys/socket.h>
#include <system_error>
#include <netinet/ip.h>

Socket::Socket()
    : File()
    , domain(PF_UNSPEC)
    , type(0)
    , protocol(IPPROTO_IP)
{
}

Socket::Socket(int _domain, int _type, int _protocol)
    : File(socket(_domain, _type, _protocol))
    , domain(_domain)
    , type(_type)
    , protocol(_protocol)
{
}

Socket::Socket(int _domain, int _type, int _protocol, int _fd)
    : File(_fd)
    , domain(_domain)
    , type(_type)
    , protocol(_protocol)
{
}

Socket::Socket(Socket &&other)
    : File(std::move(other))
    , domain(PF_UNSPEC)
    , type(0)
    , protocol(IPPROTO_IP)
{
    using std::swap;
    swap(domain, other.domain);
    swap(type, other.type);
    swap(protocol, other.protocol);
}

Socket &Socket::operator =(Socket &&other)
{
    using std::swap;
    File::operator =(std::move(other));
    swap(domain, other.domain);
    swap(type, other.type);
    swap(protocol, other.protocol);
    return *this;
}

void Socket::setsockopt(int level, int optname, const void *optval, size_t optlen)
{
    if(!isValid()) {
        throw std::system_error(std::make_error_code(std::errc::bad_file_descriptor), __PRETTY_FUNCTION__);
    }

    if(::setsockopt(get_fd(), level, optname, optval, optlen) != 0) {
        throw std::system_error(errno, std::system_category(), __PRETTY_FUNCTION__);
    }
}

void Socket::getsockopt(int level, int optname, void *optval, socklen_t *optlen)
{
    if(!isValid()) {
        throw std::system_error(std::make_error_code(std::errc::bad_file_descriptor), __PRETTY_FUNCTION__);
    }

    if(::getsockopt(get_fd(), level, optname, optval, optlen) != 0) {
        throw std::system_error(errno, std::system_category(), __PRETTY_FUNCTION__);
    }
}

void Socket::bind(const sockaddr *addr, socklen_t addrLen)
{
    if(!isValid()) {
        throw std::system_error(std::make_error_code(std::errc::bad_file_descriptor), __PRETTY_FUNCTION__);
    }

    if(::bind(get_fd(), addr, addrLen) != 0) {
        throw std::system_error(errno, std::system_category(), __PRETTY_FUNCTION__);
    }

}
