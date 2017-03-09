cmake_minimum_required(VERSION 2.8)
project(ndhcpd)

include(GNUInstallDirs)

set(ndhcpd_VERSION_MAJOR 0)
set(ndhcpd_VERSION_MINOR 1)
set(ndhcpd_VERSION_PATCH 5)
set(ndhcpd_VERSION_STRING ${ndhcpd_VERSION_MAJOR}.${ndhcpd_VERSION_MINOR}.${ndhcpd_VERSION_PATCH})

if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set (CMAKE_CXX_FLAGS "--std=gnu++11 ${CMAKE_CXX_FLAGS}")
    endif ()
else ()
    set(CMAKE_CXX_STANDARD 11)
endif ()

find_package (Threads REQUIRED)

option(USER_INSTALLATION
	"Install in user home directory" OFF)
if(USER_INSTALLATION)
	set(CMAKE_INSTALL_PREFIX $ENV{HOME} CACHE PATH "Install path prefix, prepended onto install directories." FORCE)
	set(SERVICE_INSTALL_PATH .config/systemd/user CACHE STRING "Systemd unit installation path" FORCE)
	set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
	set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
	list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
	if("${isSystemDir}" STREQUAL "-1")
		set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
	endif("${isSystemDir}" STREQUAL "-1")

else(USER_INSTALLATION)
	set(SERVICE_INSTALL_PATH lib/systemd/system CACHE STRING "Systemd unit installation path")
endif(USER_INSTALLATION)



set(libndhcpd_src ndhcpd.cc
                ndhcpd_p.cc ndhcpd_p.hpp
                dhcp_packet.cc dhcp_packet.hpp
                dhcp_error.cc dhcp_error.hpp
                file.cc file.hpp
                socket.cc socket.hpp)
set(libndhcpd_inc include/ndhcpd.hpp include/ndhcpd.h)
add_library(ndhcpd${ndhcpd_VERSION_MAJOR} SHARED ${libndhcpd_src} ${libndhcpd_inc})
target_include_directories(ndhcpd${ndhcpd_VERSION_MAJOR}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)
target_link_libraries(ndhcpd${ndhcpd_VERSION_MAJOR} ${CMAKE_THREAD_LIBS_INIT})
target_compile_options(ndhcpd${ndhcpd_VERSION_MAJOR} PRIVATE -pedantic -Wall)
set_target_properties(ndhcpd${ndhcpd_VERSION_MAJOR} PROPERTIES
    FRAMEWORK TRUE
    VERSION ${ndhcpd_VERSION_STRING}
    PUBLIC_HEADER "${libndhcpd_inc}")

add_executable(ndhcpd-app ndhcpd-app.cc)
set_target_properties(ndhcpd-app PROPERTIES OUTPUT_NAME ndhcpd)
target_link_libraries(ndhcpd-app ndhcpd${ndhcpd_VERSION_MAJOR})
target_compile_options(ndhcpd-app PRIVATE -pedantic -Wall)
#set_target_properties(ndhcpd-app PROPERTIES VERSION ${ndhcpd_VERSION_STRING})

install(TARGETS ndhcpd${ndhcpd_VERSION_MAJOR} ndhcpd-app
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT bin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT lib
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT dev
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT dev
 )

configure_file(ndhcpd.service.in ${SERVICE_INSTALL_PATH}/ndhcpd.service)
install(FILES ${CMAKE_BINARY_DIR}/${SERVICE_INSTALL_PATH}/ndhcpd.service 
		DESTINATION ${SERVICE_INSTALL_PATH}
		PERMISSIONS OWNER_WRITE OWNER_READ GROUP_WRITE GROUP_READ WORLD_READ
		COMPONENT bin)
install(CODE "MESSAGE(\"To run service as non-root user the capabilities should be set using setcap(8) command.\")")
install(CODE "MESSAGE(\"# /sbin/setcap cap_net_bind_service,cap_net_raw,cap_net_admin+epi ${CMAKE_INSTALL_PREFIX}/bin/ndhcpd\")")
