cmake_minimum_required(VERSION 2.8)
project(ndhcpd)

if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set (CMAKE_CXX_FLAGS "--std=gnu++11 ${CMAKE_CXX_FLAGS}")
    endif ()
else ()
    set(CMAKE_CXX_STANDARD 11)
endif ()

find_package (Threads REQUIRED)


set(libndhcpd_src ndhcpd.cc
                ndhcpd_p.cc ndhcpd_p.hpp
                dhcp_packet.cc dhcp_packet.hpp
                dhcp_error.cc dhcp_error.hpp
                file.cc file.hpp
                socket.cc socket.hpp)
set(libndhcpd_inc include/ndhcpd/ndhcpd.hpp)
add_library(ndhcpd ${libndhcpd_src} ${libndhcpd_inc})
target_include_directories(ndhcpd
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)
target_link_libraries(ndhcpd ${CMAKE_THREAD_LIBS_INIT})

target_compile_options(ndhcpd PRIVATE -pedantic)

add_executable(ndhcpd-app ndhcpd-app.cc)
set_target_properties(ndhcpd-app PROPERTIES OUTPUT_NAME ndhcpd)
target_link_libraries(ndhcpd-app ndhcpd)
