cmake_minimum_required(VERSION 2.8)
project(ndhcpd)

# Set modules path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

#Load modules
include(GNUInstallDirs)
include(PVS-Studio)
find_package(Threads REQUIRED)

#Library version variables
set(ndhcpd_VERSION_MAJOR 0)
set(ndhcpd_VERSION_MINOR 1)
set(ndhcpd_VERSION_PATCH 5)
set(ndhcpd_VERSION_STRING ${ndhcpd_VERSION_MAJOR}.${ndhcpd_VERSION_MINOR}.${ndhcpd_VERSION_PATCH})

set(ndhcpd_NAME ${CMAKE_PROJECT_NAME}${ndhcpd_VERSION_MAJOR})

#Common compilation files
if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set (CMAKE_CXX_FLAGS "--std=gnu++11 ${CMAKE_CXX_FLAGS}")
    endif ()
else ()
    set(CMAKE_CXX_STANDARD 11)
endif ()

#Common compile options
add_compile_options(-pedantic -Wall)

#Configuration file
configure_file(config.h.in config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Library
set(libndhcpd_src ndhcpd.cc
                ndhcpd_p.cc ndhcpd_p.hpp
                dhcp_packet.cc dhcp_packet.hpp
                dhcp_error.cc dhcp_error.hpp
                file.cc file.hpp
                socket.cc socket.hpp)
set(libndhcpd_inc include/ndhcpd.hpp include/ndhcpd.h)
add_library(${ndhcpd_NAME} SHARED ${libndhcpd_src} ${libndhcpd_inc})
target_include_directories(${ndhcpd_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)
target_link_libraries(${ndhcpd_NAME} ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(${ndhcpd_NAME} PROPERTIES
    FRAMEWORK TRUE
    VERSION ${ndhcpd_VERSION_STRING}
    SOVERSION ${ndhcpd_VERSION_MAJOR}
    PUBLIC_HEADER "${libndhcpd_inc}")

# Daemon
add_executable(ndhcpd-app ndhcpd-app.cc)
set_target_properties(ndhcpd-app PROPERTIES OUTPUT_NAME ndhcpd)
target_link_libraries(ndhcpd-app ${ndhcpd_NAME})

# Install library
install(TARGETS ${ndhcpd_NAME} EXPORT ${ndhcpd_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT bin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT lib
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT dev
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT dev
 )
install(EXPORT ${ndhcpd_NAME} DESTINATION ${CMAKE_INndhcpdSTALL_LIBDIR}/cmake/${ndhcpd_NAME})
install(FILES ${ndhcpd_NAME}-config.cmake DESTINATION ${CMAKE_INndhcpdSTALL_LIBDIR}/cmake/${ndhcpd_NAME})

# Install daemon
install(TARGETS ndhcpd-app
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT bin
 )

install(CODE "MESSAGE(\"To run service as non-root user the capabilities should be set using setcap(8) command.\")")
install(CODE "MESSAGE(\"# /sbin/setcap cap_net_bind_service,cap_net_raw,cap_net_admin+epi ${CMAKE_INSTALL_PREFIX}/bin/ndhcpd\")")

# PVS-Studio analyze target
pvs_studio_add_target(TARGET analyze
		FORMAT tasklist
                OUTPUT FORMAT errorfile
		PREPROCESSOR gcc
                LOG "${PROJECT_BINARY_DIR}/target.plog"
		ANALYZE ndhcpd-app ${ndhcpd_NAME}
                CXX_FLAGS "-I${PROJECT_SOURCE_DIR}/include"
                C_FLAGS "-I${PROJECT_SOURCE_DIR}/include"
		)
