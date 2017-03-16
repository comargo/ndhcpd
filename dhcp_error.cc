// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "dhcp_error.hpp"

const std::error_category &dhcp_category()
{
    static dhcp_category_impl instance;
    return instance;
}

std::error_condition make_error_condition(dhcp_error e)
{
    return std::error_condition(
                static_cast<int>(e),
                dhcp_category());
}

std::error_code make_error_code(dhcp_error e)
{
    return std::error_code(
                static_cast<int>(e),
                dhcp_category());
}
