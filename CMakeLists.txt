cmake_minimum_required(VERSION 2.8)
project(ndhcpd)

# Set modules path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

#Load modules
include(GNUInstallDirs)
include(PVS-Studio)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(log4cpp REQUIRED log4cpp)

#Library version variables
set(ndhcpd_VERSION_MAJOR 0)
set(ndhcpd_VERSION_MINOR 2)
set(ndhcpd_VERSION_PATCH 0)
set(ndhcpd_VERSION_STRING ${ndhcpd_VERSION_MAJOR}.${ndhcpd_VERSION_MINOR}.${ndhcpd_VERSION_PATCH})

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
add_library(ndhcpd SHARED ${libndhcpd_src} ${libndhcpd_inc})
target_include_directories(ndhcpd
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${log4cpp_INCLUDE_DIRS})
target_link_libraries(ndhcpd ${CMAKE_THREAD_LIBS_INIT} ${log4cpp_LIBRARY_DIRS} ${log4cpp_LIBRARIES})
set_target_properties(ndhcpd PROPERTIES
    FRAMEWORK TRUE
    VERSION ${ndhcpd_VERSION_STRING}
    SOVERSION ${ndhcpd_VERSION_MAJOR}
    PUBLIC_HEADER "${libndhcpd_inc}")

# Daemon
add_executable(ndhcpd-app ndhcpd-app.cc)
set_target_properties(ndhcpd-app PROPERTIES OUTPUT_NAME ndhcpd)
target_include_directories(ndhcpd-app
    PRIVATE
        ${log4cpp_INCLUDE_DIRS})
target_link_libraries(ndhcpd-app ndhcpd ${log4cpp_LIBRARY_DIRS} ${log4cpp_LIBRARIES})

# Install library
install(TARGETS ndhcpd EXPORT ndhcpd
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT bin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT lib
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT dev
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT dev
 )
install(EXPORT ndhcpd DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ndhcpd)
install(FILES ndhcpd-config.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ndhcpd)

# Install daemon
install(TARGETS ndhcpd-app
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT bin
 )
install(FILES ndhcpd-app.log.properties DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/ndhcpd)

install(CODE "MESSAGE(\"To run service as non-root user the capabilities should be set using setcap(8) command.\")")
install(CODE "MESSAGE(\"# /sbin/setcap cap_net_bind_service,cap_net_raw,cap_net_admin+epi ${CMAKE_INSTALL_PREFIX}/bin/ndhcpd\")")

# PVS-Studio analyze target
pvs_studio_add_target(TARGET analyze
		FORMAT tasklist
                OUTPUT FORMAT errorfile
		PREPROCESSOR gcc
                LOG "${PROJECT_BINARY_DIR}/target.plog"
		ANALYZE ndhcpd-app ndhcpd
                CXX_FLAGS "-I${PROJECT_SOURCE_DIR}/include"
                C_FLAGS "-I${PROJECT_SOURCE_DIR}/include"
		)
