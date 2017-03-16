// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "file.hpp"
#include <system_error>
#include <unistd.h>

File::File()
    : fd(-1)
{

}

File::File(int fd)
    :fd(fd)
{
    if(!isValid()) {
        throw std::system_error(errno, std::system_category(), "File()");
    }
}

File::~File()
{
    close();
}

File::File(File &&other)
    : fd(-1)
{
    std::swap(fd, other.fd);
}

File &File::operator =(File &&other)
{
    std::swap(fd, other.fd);
    return *this;
}

int File::close()
{
    if(isValid()) {
        ::close(fd);
        fd = -1;
    }
    return 0;
}
