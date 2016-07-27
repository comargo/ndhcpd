#ifndef NDHCPD_H
#define NDHCPD_H

#include <stdlib.h>
#include <stdint.h>

extern "C" {

typedef struct {int unused;} *ndhcpd_t;

ndhcpd_t ndhcpd_create() __THROW;
void ndhcpd_delete(ndhcpd_t _ndhcpd) __THROW;
void ndhcpd_setInterfaceName(ndhcpd_t _ndhcpd, const char *ifaceName) __THROW;
void ndhcpd_addRange_s(ndhcpd_t _ndhcpd, const char *from, const char *to) __THROW;
void ndhcpd_addRange_i(ndhcpd_t _ndhcpd, uint32_t from, uint32_t to) __THROW;
void ndhcpd_addIp_s(ndhcpd_t _ndhcpd, const char *ip) __THROW;
void ndhcpd_addIp_i(ndhcpd_t _ndhcpd, uint32_t ip) __THROW;
int ndhcpd_ips(const ndhcpd_t _ndhcpd, uint32_t *ips, size_t ipsCount) __THROW;

int ndhcpd_start(ndhcpd_t _ndhcpd) __THROW;
int ndhcpd_stop(ndhcpd_t _ndhcpd) __THROW;
int ndhcpd_isStarted(const ndhcpd_t _ndhcpd) __THROW;

typedef void (*ndhcpd_logfn_t)(int level, const char *msg);
void ndhcpd_setLog(ndhcpd_t _ndhcpd, ndhcpd_logfn_t logfn) __THROW;

}

#endif//NDHCPD_H
