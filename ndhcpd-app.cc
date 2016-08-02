#include <ndhcpd.hpp>

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
#define SYSLOG_NAMES 1
#include <syslog.h>
#include <string.h>

#include "file.hpp"

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


static int max_loglevel;

void logger(int level, const std::string &msg) {
    if(max_loglevel < 0) { // Use -1 as syslog
        syslog(level, "%s", msg.c_str());
    } else if(level <= max_loglevel){
        const char *levelName = std::find_if(std::begin(prioritynames), std::prev(std::end(prioritynames)), [level](const CODE& code){
            return code.c_val == level;
        })->c_name;

        std::cerr << levelName << ": " << msg << std::endl;
    }
}

void sig_handler_exit(int signo, siginfo_t *siginfo, void *ctx)
{
    std::ostringstream str;
    str << "Caught signal " << signo << " from " << siginfo->si_pid;
    logger(LOG_INFO, str.str());
    sStop = true;
}


int main(int argc, char *argv[])
{
    std::vector<option> options = {
        {"pipe", required_argument, nullptr, 'p'},
        {"group", required_argument, nullptr, 'g'},
        {"foreground", no_argument, nullptr, 'f'},
        {"syslog", required_argument, nullptr, 's'},
    };

    std::string pipe_path = "/var/tmp/ndhcpd";
    std::string pipe_group = "netdev";
    bool daemonize = true;
    int debuglevel = LOG_ERR;
    bool useDebugLevel = false;

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
        case 's':
            debuglevel = std::find_if(std::begin(prioritynames), std::prev(std::end(prioritynames)), [](const CODE& code){
                return strcmp(code.c_name, optarg) == 0;
            })->c_val;
            if(debuglevel == -1) {
                char *endptr;
                debuglevel = strtoul(optarg, &endptr, 0);
            }
            useDebugLevel = true;
            break;
        case 'v':
            debuglevel++;
            useDebugLevel = true;
            break;
        default:
            break;
        }
    }

    // Setup logging
    if(daemonize) {
        if(useDebugLevel) {
            setlogmask(LOG_UPTO(debuglevel));
        }
        max_loglevel = -1;
    }
    else {
        max_loglevel = debuglevel;
    }

    int ret = mkfifo(pipe_path.c_str(), S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP );
    if(ret < 0) {
        logger(LOG_WARNING, std::system_error(errno, std::system_category(), "mkfifo()").what());
    }

    struct group* gr = getgrnam(pipe_group.c_str());
    if(!gr) {
        logger(LOG_WARNING, std::system_error(errno, std::system_category(), "getgrnam()").what());
    }
    if(chown(pipe_path.c_str(), -1, gr->gr_gid) != 0) {
        logger(LOG_WARNING, std::system_error(errno, std::system_category(), "chown()").what());
    }

    // Daemonize point
    if(daemonize) {
	daemon(0,0);
    }

    logger(LOG_INFO, "Starting...");

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

        File fifo(open(pipe_path.c_str(), O_RDONLY|O_NONBLOCK));
        ndhcpd srv;
        srv.setLog(logger);

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
            for(auto bufIter = buf.begin();
                bufIter != buf.end();
                /*we will iterate manually*/) {
                auto nextIter = std::find(bufIter, buf.end(), '\n');
                cmds.emplace_back(bufIter, nextIter);
                bufIter = nextIter;
                if(bufIter != buf.end()) {
                    ++bufIter;
                }
            }

            for(std::string cmd : cmds) {
                std::string cmdParam(cmd, 1);
                std::string::size_type pos;
                switch (cmd[0]) {
                case 'i': // set interface name
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
                        srv.addRange(ipFrom, ipTo, subnet);
                    }
                    else {
                        srv.addIp(cmdParam,subnet);
                    }
                }
                    break;
                case 's':
                    if(cmd == "start") {
                        srv.start();
                    }
                    else if(cmd == "stop"){
                        srv.stop();
                    }
                case 'q':
                    if(cmd == "quit") {
                        logger(LOG_INFO, "Caught 'quit' command. Exiting...");
                        return EXIT_SUCCESS;
                    }
                default:
                    break;
                }
            }
        }
    }
    catch(const std::exception &ex) {
        logger(LOG_ERR, ex.what());
        return EXIT_FAILURE;
    }
    logger(LOG_INFO, "Exiting...");
    return EXIT_SUCCESS;
}
