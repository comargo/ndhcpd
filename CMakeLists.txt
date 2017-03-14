cmake_minimum_required(VERSION 2.8)
project(ndhcpd)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

include(GNUInstallDirs)
include(PVS-Studio)

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

install(CODE "MESSAGE(\"To run service as non-root user the capabilities should be set using setcap(8) command.\")")
install(CODE "MESSAGE(\"# /sbin/setcap cap_net_bind_service,cap_net_raw,cap_net_admin+epi ${CMAKE_INSTALL_PREFIX}/bin/ndhcpd\")")

pvs_studio_add_target(TARGET analyze
		FORMAT tasklist
                OUTPUT FORMAT errorfile
		PREPROCESSOR gcc
                LOG "${PROJECT_BINARY_DIR}/target.plog"
		ANALYZE ndhcpd-app ndhcpd${ndhcpd_VERSION_MAJOR}
                CXX_FLAGS "-I${PROJECT_SOURCE_DIR}/include"
                C_FLAGS "-I${PROJECT_SOURCE_DIR}/include"
		)
