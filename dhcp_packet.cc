// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "dhcp_packet.hpp"
#include <string.h>

struct dhcp_option* dhcp_find_option(const dhcp_packet &packet, dhcp_option::_code code)
{
    struct dhcp_option *option;
    option = (struct dhcp_option *)packet.options;
    while(option->code != dhcp_option::_code::end) {
        if(option->code == code) {
            return option;
        }
        switch (option->code) {
        case dhcp_option::_code::padding:
            option = (struct dhcp_option *)(((uint8_t*)option)+1);
            break;
        default:
            option = (struct dhcp_option *)(((uint8_t*)option)+option->len+2);
            break;
        }
    }
    if(code == dhcp_option::_code::end) {
        return option;
    }
    return nullptr;
}

const void *dhcp_get_option(const dhcp_packet &packet, dhcp_option::_code code)
{
    const struct dhcp_option *option = dhcp_find_option(packet, code);
    if(option) {
        return option->value;
    }
    return nullptr;
}


void dhcp_add_option(dhcp_packet *packet, dhcp_option::_code code, uint8_t len, const void *value)
{
    struct dhcp_option *option = dhcp_find_option(*packet, dhcp_option::_code::end);

    option->code = code;
    option->len = len;
    memcpy(option->value, value, option->len);

    // append END tag;
    option = (struct dhcp_option *)(((char*)option)+option->len+2);
    option->code = dhcp_option::_code::end;

}

const char *dhcp_message_type_name(dhcp_message_type type)
{
    switch (type) {
    case dhcp_message_type::discover:
        return "DISCOVER";
    case dhcp_message_type::offer:
    return "OFFER";
    case dhcp_message_type::request:
        return "REQUEST";
    case dhcp_message_type::decline:
        return "DECLINE";
    case dhcp_message_type::ack:
        return "ACK";
    case dhcp_message_type::nak:
        return "NAK";
    case dhcp_message_type::release:
        return "RELEASE";
    case dhcp_message_type::inform:
        return "INFORM";
    default:
        return "<<Unknown message type>>";
    }
}
