#ifndef __SOCKET_HPP__
#define __SOCKET_HPP__

#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "file.hpp"

class Socket;

template<typename T>
struct sockopt {
    static void set(Socket &sock, int level, int optname, const T &optval);
    static void get(Socket &sock, int level, int optname, T* optval);
};


template <>
struct sockopt<bool> {
    static void set(Socket &sock, int level, int optname, bool optval);
    static void get(Socket &sock, int level, int optname, bool* optval);
};


class Socket : public File
{
public:
    Socket();
    Socket(int _domain, int _type, int _protocol);
    Socket(int _domain, int _type, int _protocol, int _fd);

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket &&other);
    Socket &operator =(Socket &&other);
public:
    template<typename T>
    void setsockopt(int level, int optname, const T &optval) {
        sockopt<T>::set(*this, level, optname, optval);
    }

    template<typename T>
    void getsockopt(int level, int optname, T* optval) {
        sockopt<T>::get(*this,level, optname, optval);
    }

    template<typename T>
    void bind(const T &addr) {
        bind(reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    }



private:
    int domain;
    int type;
    int protocol;

    void setsockopt(int level, int optname, const void* optval, size_t optlen);
    void getsockopt(int level, int optname, void* optval, socklen_t *optlen);
    template<typename T>
    friend struct sockopt;

    void bind(const struct sockaddr* addr, socklen_t addrLen);
};


template<typename T>
inline void sockopt<T>::set(Socket &sock, int level, int optname, const T &optval) {
    sock.setsockopt(level, optname, &optval, sizeof(optval));
}
template<typename T>
inline void sockopt<T>::get(Socket &sock, int level, int optname, T *optval) {
    socklen_t optlen = sizeof(optval);
    sock.getsockopt(level, optname, optval, &optlen);
}

inline void sockopt<bool>::set(Socket &sock, int level, int optname, bool optval) {
    sock.setsockopt(level, optname, optval?UINT32_C(1):UINT32_C(0));

}
inline void sockopt<bool>::get(Socket &sock, int level, int optname, bool *optval) {
    uint32_t u32val;
    sock.getsockopt(level, optname, &u32val);
    *optval = (u32val != 0);
}

#endif//__SOCKET_HPP__
