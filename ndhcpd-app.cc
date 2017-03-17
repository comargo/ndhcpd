// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ndhcpd.hpp>
#include "config.h"

#include <getopt.h>
#include <iostream>
#include <vector>
#include <system_error>
#include <algorithm>
#include <sstream>
#include <functional>

#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <grp.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <string.h>

#include "file.hpp"

#include <log4cpp/PropertyConfigurator.hh>
#include <log4cpp/Category.hh>

static bool volatile sStop = false;


template<typename T>
class scope_exit
{
public:
    explicit scope_exit(T&& exitfn) :
        exitFn(std::forward<T>(exitfn))
    {}

    ~scope_exit()
    {
        try {
            exitFn();
        }
        catch(...){/*eatup exceptions*/}
    }

    scope_exit(const scope_exit<T>&) = delete;
    scope_exit& operator =(const scope_exit<T>&) = delete;

    scope_exit(scope_exit<T>&&) = default;
    scope_exit& operator =(scope_exit<T>&&) = default;
private:
    T exitFn;
};

template <typename T>
scope_exit<T> make_scope_exit(T&& exitFn)
{
    return scope_exit<T>(std::forward<T>(exitFn));
}

void sig_handler_exit(int signo, siginfo_t *siginfo, void *ctx)
{
    log4cpp::Category::getInstance("ndhcpd.app").infoStream()
            << "Caught signal " << signo << " from " << siginfo->si_pid;
    sStop = true;
}


int main(int argc, char *argv[])
{
    std::vector<std::string> logFileNames;
    logFileNames.push_back(std::string(SYSCONFDIR) + "/ndhcpd-app.log.properties");
    logFileNames.push_back("./ndhcpd-app.log.properties");
    const char* envLogConfigFileName = getenv("NDHCPD_LOG");
    if(envLogConfigFileName) {
        logFileNames.push_back(envLogConfigFileName);
    }

    for(auto logConfigFileName : logFileNames) {
        try {
            log4cpp::PropertyConfigurator::configure(logConfigFileName);
            log4cpp::Category::getInstance("ndhcpd.app").debugStream() << "NDHCPD loaded log properties from " << logConfigFileName;
        }
        catch(const log4cpp::ConfigureFailure &fail) {
        }
    }

    log4cpp::Category &log = log4cpp::Category::getInstance("ndhcpd.app");


    std::vector<option> options = {
        {"pipe", required_argument, nullptr, 'p'},
        {"group", required_argument, nullptr, 'g'},
        {"foreground", no_argument, nullptr, 'f'},
	{0,0,0,0}
    };

    std::string pipe_path = "/var/tmp/ndhcpd";
    std::string pipe_group = "netdev";
    bool daemonize = true;

    for(;;) {
        int opt_index;
        int opt = getopt_long(argc, argv, "p:g:fvs:", options.data(), &opt_index);
        if(opt == -1) {
            break;
        }
        switch (opt) {
        case 'p':
            pipe_path = optarg;
            break;
        case 'g':
            pipe_group = optarg;
            break;
        case 'f':
            daemonize = false;
            break;
        default:
            break;
        }
    }

    mode_t oldMask = umask(002);
    int ret = mkfifo(pipe_path.c_str(), S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP );
    umask(oldMask);
    log.infoStream() << "Creating FIFO " << pipe_path;
    if(ret < 0) {
        log.warn(std::system_error(errno, std::system_category(), "mkfifo()").what());
    }

    struct group* gr = getgrnam(pipe_group.c_str());
    if(!gr) {
        log.warn(std::system_error(errno, std::system_category(), "getgrnam()").what());
    }
    if(chown(pipe_path.c_str(), -1, gr->gr_gid) != 0) {
        log.warn(std::system_error(errno, std::system_category(), "chown()").what());
    }

    // Daemonize point
    if(daemonize) {
        if(daemon(0,0) != 0) {
            log.error(std::system_error(errno, std::system_category(), "daemon()").what());
            return EXIT_FAILURE;
        }
    }

    log.notice("Starting...");

    // Setup signal handlers
    struct sigaction sa;
    sa.sa_sigaction = &sig_handler_exit;
    sa.sa_flags = SA_RESTART|SA_SIGINFO;
    sigfillset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    sa.sa_handler = SIG_IGN;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    try {
        const auto unlink_fifo(make_scope_exit(std::bind(&unlink, pipe_path.c_str())));

        mode_t oldUmask = umask(0002);
        File fifo(open(pipe_path.c_str(), O_RDWR|O_NONBLOCK));
        umask(oldUmask);
        ndhcpd srv;
        while(!sStop) {
            std::vector<char> buf(256);

            std::vector<struct pollfd> pollFds = {
                { fifo, POLLIN|POLLERR },
            };

            int ret = poll(pollFds.data(), pollFds.size(), -1);
            if(ret < 0) {
                if(errno == EINTR) {
                    // signal interrupt
                    continue;
                }
                throw std::system_error(errno, std::system_category(), "poll(fifo)");
            }
            if(ret == 0) {
                // Spurious wake-up. Probably signal
                continue;
            }

            ssize_t len = read(fifo, buf.data(), buf.size());
            if(len < 0) {
                if(errno == EAGAIN || errno == EWOULDBLOCK
                        || errno == EINTR) {
                    continue;
                }
                throw std::system_error(errno, std::system_category(), "read(fifo)");
            }
            if(len == 0) {
                // Spurious wake-up. Probably signal
                continue;
            }
            buf.resize(len);

            std::vector<std::string> cmds;
            for(auto bufIter = buf.begin(); bufIter != buf.end(); /*we will iterate manually*/) {
                auto nextIter = std::find(bufIter, buf.end(), '\n');
                cmds.emplace_back(bufIter, nextIter);
                bufIter = nextIter;
                if(bufIter != buf.end()) {
                    ++bufIter;
                }
            }

            for(std::string cmd : cmds) {
                log.debugStream() << "Command \"" << cmd << "\"";
                std::string cmdParam(cmd, 1);
                std::string::size_type pos;
                switch (cmd[0]) {
                case 'i': // set interface name
                    log.infoStream() << "Set interface " << cmdParam;
                    srv.setInterfaceName(cmdParam);
                    break;
                case 'a':
                {
                    std::string subnet="255.255.255.0";
                    if((pos = cmdParam.find('/')) != std::string::npos) {
                        subnet.assign(cmdParam, pos+1, std::string::npos);
                        cmdParam.erase(pos);
                    }
                    if((pos = cmdParam.find('-')) != std::string::npos) {
                        std::string ipFrom(cmdParam, 0, pos);
                        std::string ipTo(cmdParam, pos+1);
                        log.infoStream() << "Add IP range " << ipFrom << "-" << ipTo << "/" << subnet;
                        srv.addRange(ipFrom, ipTo, subnet);
                    }
                    else {
                        log.infoStream() << "Add IP address " << cmdParam << "/" << subnet;
                        srv.addIp(cmdParam,subnet);
                    }
                }
                    break;
                case 's':
                    if(cmd == "start") {
                        log.info("Start service");
                        srv.start();
                    }
                    else if(cmd == "stop"){
                        log.info("Stop service");
                        srv.stop();
                    }
                case 'q':
                    if(cmd == "quit") {
                        log.info("Caught 'quit' command. Exiting...");
                        return EXIT_SUCCESS;
                    }
                default:
                    break;
                }
            }
        }
    }
    catch(const std::exception &ex) {
        log.error(ex.what());
        return EXIT_FAILURE;
    }
    log.notice("Exiting...");
    return EXIT_SUCCESS;
}
