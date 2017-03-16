#ifndef NDHCPD_H
#define NDHCPD_H

#include <stdlib.h>
#include <stdint.h>

extern "C" {

typedef struct {int unused;} *ndhcpd_t;

ndhcpd_t ndhcpd_create() __THROW;
void ndhcpd_delete(ndhcpd_t _ndhcpd) __THROW;
void ndhcpd_setInterfaceName(ndhcpd_t _ndhcpd, const char *ifaceName) __THROW;
void ndhcpd_addRange_s(ndhcpd_t _ndhcpd, const char *from, const char *to, const char *mask) __THROW;
void ndhcpd_addRange_i(ndhcpd_t _ndhcpd, uint32_t from, uint32_t to, uint32_t mask) __THROW;
void ndhcpd_addIp_s(ndhcpd_t _ndhcpd, const char *ip, const char *mask) __THROW;
void ndhcpd_addIp_i(ndhcpd_t _ndhcpd, uint32_t ip, uint32_t mask) __THROW;
int ndhcpd_ips(const ndhcpd_t _ndhcpd, uint32_t *ips, size_t ipsCount) __THROW;

int ndhcpd_start(ndhcpd_t _ndhcpd) __THROW;
int ndhcpd_stop(ndhcpd_t _ndhcpd) __THROW;
int ndhcpd_isStarted(const ndhcpd_t _ndhcpd) __THROW;

}

#endif//NDHCPD_H
