cmake_minimum_required(VERSION 2.8)
project(ndhcpd)

set(ndhcpd_VERSION_MAJOR 0)
set(ndhcpd_VERSION_MINOR 1)
set(ndhcpd_VERSION_PATCH 0)
set(ndhcpd_VERSION_STRING ${ndhcpd_VERSION_MAJOR}.${ndhcpd_VERSION_MINOR}.${ndhcpd_VERSION_PATCH})

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
set(libndhcpd_inc include/ndhcpd.hpp)
add_library(ndhcpd SHARED ${libndhcpd_src} ${libndhcpd_inc})
target_include_directories(ndhcpd
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)
target_link_libraries(ndhcpd ${CMAKE_THREAD_LIBS_INIT})
target_compile_options(ndhcpd PRIVATE -pedantic)
set_target_properties(ndhcpd PROPERTIES
    FRAMEWORK TRUE
    VERSION ${ndhcpd_VERSION_STRING}
    PUBLIC_HEADER ${libndhcpd_inc})

add_executable(ndhcpd-app ndhcpd-app.cc)
set_target_properties(ndhcpd-app PROPERTIES OUTPUT_NAME ndhcpd)
target_link_libraries(ndhcpd-app ndhcpd)
#set_target_properties(ndhcpd-app PROPERTIES VERSION ${ndhcpd_VERSION_STRING})

install(TARGETS ndhcpd ndhcpd-app
    RUNTIME DESTINATION bin COMPONENT bin
    LIBRARY DESTINATION lib COMPONENT lib
    ARCHIVE DESTINATION lib COMPONENT dev
    PUBLIC_HEADER DESTINATION include COMPONENT dev
 )
