#ifndef NDHCPD_DHCP_PACKET_HPP
#define NDHCPD_DHCP_PACKET_HPP

#include <stdint.h>

enum class dhcp_message_type : uint8_t {
    minval = 1,

    discover = 1,
    offer = 2,
    request = 3,
    decline = 4,
    ack = 5,
    nak = 6,
    release = 7,
    inform = 8,

    maxval = 8
};

struct dhcp_option {
    enum class _code : uint8_t {
        padding = 0,
        requested_ip = 50,
        lease_time = 51,
        message_type = 53,
        server_id = 54,
        end = 255
    } code;
    uint8_t len;
    char value[1];
};

struct dhcp_packet {
        enum class _op : uint8_t {
            BOOTREQUEST = 1,
            BOOTREPLY = 2
        } op;      /* BOOTREQUEST or BOOTREPLY */
        uint8_t htype;   /* hardware address type. 1 = 10mb ethernet */
        uint8_t hlen;    /* hardware address length */
        uint8_t hops;    /* used by relay agents only */
        uint32_t xid;    /* unique id */
        uint16_t secs;   /* elapsed since client began acquisition/renewal */
        uint16_t flags;  /* only one flag so far: */
#define BROADCAST_FLAG 0x8000 /* "I need broadcast replies" */
        uint32_t ciaddr; /* client IP (if client is in BOUND, RENEW or REBINDING state) */
        uint32_t yiaddr; /* 'your' (client) IP address */
        /* IP address of next server to use in bootstrap, returned in DHCPOFFER, DHCPACK by server */
        uint32_t siaddr_nip;
        uint32_t gateway_nip; /* relay agent IP address */
        uint8_t chaddr[16];   /* link-layer client hardware address (MAC) */
        uint8_t sname[64];    /* server host name (ASCIZ) */
        uint8_t file[128];    /* boot file name (ASCIZ) */
        enum _cookie : uint32_t {
            cookie_value_he = 0x63825363
        } cookie;
        uint8_t options[308];
};

const void *dhcp_get_option(const dhcp_packet &packet, dhcp_option::_code code);

void dhcp_add_option(dhcp_packet *packet, dhcp_option::_code code, uint8_t len, const void *value);

template<typename T>
inline void dhcp_add_option(dhcp_packet *packet, dhcp_option::_code code, const T& value) {
    static_assert(sizeof(value) < 256, "Value too big");
    return dhcp_add_option(packet, code, static_cast<uint8_t>(sizeof(value)), &value);
}

const char * dhcp_message_type_name(dhcp_message_type type);

#endif//NDHCPD_DHCP_PACKET_HPP
