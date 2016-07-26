# ndhcpd
Very simple dhcp server implementation.

Consists of library, that is easy to integrate with your software and application.

Application controls via pipe.

### Pipe interface commands:
* `i<interface>` - Set interface to bind to
* `a<ip>` - add IP address to lease
* `a<ip> <ip>` - add IP address range to lease
* `start` - start server
* `stop` - stop server
* `quit` - quit application

## TODO List
- [ ] Daemonize application
- [ ] Add option to select pipe-file group
