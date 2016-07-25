#ifndef __DHCP_ERROR_HPP__
#define __DHCP_ERROR_HPP__

#include <system_error>

enum class dhcp_error {
    ok = 0,
    invalid_packet,
    invalid_hwtype,
    unexpected_packet_type,
    unexpected_message_type,
    no_more_leases,
    no_ip_requested
};

class dhcp_category_impl
        : public std::error_category
{
public:
    const char *name() const noexcept override { return "dhcp"; }
    bool equivalent( const std::error_code& code, int condition) const noexcept override { return false; }

    std::string message(int ev) const noexcept override
    {
        switch (dhcp_error(ev)) {
        case dhcp_error::ok:
            return "no error";
        case dhcp_error::invalid_packet:
            return "Invalid DHCP packet";
        case dhcp_error::invalid_hwtype:
            return "Unsupported hardware type in DHCP packet";
        case dhcp_error::unexpected_packet_type:
            return "Unexpected DHCP packet type";
        case dhcp_error::unexpected_message_type:
            return "Unexpected DHCP packet message type";
        case dhcp_error::no_more_leases:
            return "No leases available for client";
        case dhcp_error::no_ip_requested:
            return "DHCP Request packet without ip address";
        default:
            return "Unknown DHCP error";
        }
    }
};

const std::error_category& dhcp_category();

std::error_condition make_error_condition(dhcp_error e);
std::error_code make_error_code(dhcp_error e);

namespace std
{
template <> struct is_error_condition_enum<dhcp_error>
        : public true_type {};
template <> struct is_error_code_enum<dhcp_error>
        : public true_type {};
}


#endif//__DHCP_ERROR_HPP__
