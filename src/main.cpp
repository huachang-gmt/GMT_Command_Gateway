#include "tcp_server.h"

#include <iostream>

int main()
{
    TcpServer server;

    if (!server.start()) {
        std::cerr << "Failed to start TCP server\n";
        return 1;
    }

    std::cout << "TCP server started on port 9999\n";

    server.run();

    return 0;
}
