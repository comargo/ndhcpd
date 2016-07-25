#include <ndhcpd/ndhcpd.hpp>

#include <getopt.h>
#include <iostream>
#include <vector>
#include <system_error>
#include <algorithm>

#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <grp.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>

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


void logger(int level, const std::string &msg) {
    std::cerr << level << ": " << msg;
}

void sig_handler_exit(int signo)
{
    sStop = true;
}


int main(int argc, char *argv[])
{
    // Setup signal handlers
    struct sigaction sa;
    sa.sa_handler = &sig_handler_exit;
    sa.sa_flags = SA_RESTART;
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


    std::vector<option> options = {
      {"pipe", required_argument, nullptr, 'p'},
    };

    std::string pipe_path = "/var/tmp/ndhcpd";

    for(;;) {
        int opt_index;
        int opt = getopt_long(argc, argv, "p:", options.data(), &opt_index);
        if(opt == -1) {
            break;
        }
        switch (opt) {
        case 'p':
            pipe_path = optarg;
            break;
        default:
            break;
        }
    }

    int ret = mkfifo(pipe_path.c_str(), S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP );
    if(ret < 0) {
        perror("mkfifo");
//        return EXIT_FAILURE;
    }

    struct group* gr = getgrnam("netdev");
    chown(pipe_path.c_str(), -1, gr->gr_gid);

    // Daemonize point

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
                    if((pos = cmdParam.find(' ')) != std::string::npos) {
                        std::string ipFrom(cmdParam, 0, pos);
                        std::string ipTo(cmdParam, pos+1);
                        srv.addRange(ipFrom, ipTo);
                    }
                    else {
                        srv.addIp(cmdParam);
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
                        return EXIT_SUCCESS;
                    }
                default:
                    break;
                }
            }
        }
    }
    catch(const std::system_error &err) {
        std::cerr << err.what() << "(" << err.code() << ")" << std::endl;
    }

    catch(const std::exception &ex) {
        std::cerr << ex.what() << std::endl;
    }


    logger(6, "Gracefull exit\n");
    return EXIT_SUCCESS;
}
