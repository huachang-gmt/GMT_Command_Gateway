#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <cstdint>
#include "usb_transport.hpp"

class TcpServer
{
public:
    static constexpr std::uint16_t DEFAULT_PORT = 9999;

    TcpServer();
    ~TcpServer();

    bool start();
    void run();

private:
    int server_fd_;
    int client_fd_;
    UsbTransport usb_;
};

#endif // TCP_SERVER_H