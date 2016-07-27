#ifndef NDHCPD_FILE_HPP
#define NDHCPD_FILE_HPP

class File
{
public:
    File();
    File(int fd);
    virtual ~File();

    File(const File&) = delete;
    File& operator=(const File&) = delete;

    File(File &&other);
    File &operator =(File &&other);

public:
    operator int() const {return get_fd();}
    virtual int get_fd() const { return fd; }

    operator bool() const { return isValid(); }
    virtual bool isValid() const { return fd != -1; }

public:
    virtual int close();

private:
    int fd;

};

#endif//NDHCPD_FILE_HPP
