#include "tcp_server.h"
#include "command_parser.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>


TcpServer::TcpServer()
    : server_fd_(-1),
      client_fd_(-1),
      usb_("/dev/ttyACM0")
{
}

TcpServer::~TcpServer()
{
}

bool TcpServer::start()
{
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd_ < 0)
    {
        std::cerr << "[ERROR] socket() failed." << std::endl;
        return false;
    }

    std::cout << "[OK] socket() created. fd=" << server_fd_ << std::endl;

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(DEFAULT_PORT);

    if (bind(server_fd_,
             reinterpret_cast<struct sockaddr*>(&server_addr),
             sizeof(server_addr)) < 0)
    {
        std::cerr << "[ERROR] bind() failed." << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    std::cout << "[OK] bind() successful. port="
              << DEFAULT_PORT << std::endl;

    if (listen(server_fd_, 1) < 0)
    {
        std::cerr << "[ERROR] listen() failed." << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    std::cout << "[OK] listen() successful." << std::endl;

    std::cout << "[WAIT] Waiting for client connection..."
              << std::endl;

    client_fd_ = accept(server_fd_, nullptr, nullptr);

    if (client_fd_ < 0)
    {
        std::cerr << "[ERROR] accept() failed." << std::endl;
        return false;
    }

    std::cout << "[OK] Client connected. fd="
              << client_fd_ << std::endl;

    if (!usb_.open())
    {
        std::cerr << "[ERROR] Failed to open USB device: /dev/ttyACM0"
                << std::endl;
        close(client_fd_);
        client_fd_ = -1;
        return false;
    }

    std::cout << "[OK] USB device opened: /dev/ttyACM0"
            << std::endl;

    return true;
}

void TcpServer::run()
{
    std::cout << "[RUN] Client communication loop started."
              << std::endl;

    while (true)
    {
        char buffer[1024];

        const ssize_t received = recv(
            client_fd_,
            buffer,
            sizeof(buffer) - 1,
            0);

        if (received < 0)
        {
            std::cerr << "[ERROR] recv() failed." << std::endl;
            return;
        }

        if (received == 0)
        {
            std::cout << "[INFO] Client disconnected." << std::endl;
            return;
        }

        buffer[received] = '\0';

        std::cout << "[RX] " << buffer;

        CommandParser parser;
        const ParseResult result = parser.parse(buffer);

        if (result.result == ParserResult::INVALID)
        {
            const std::string error_response =
                "[ERROR] " + result.error;

            const ssize_t sent = send(
                client_fd_,
                error_response.c_str(),
                error_response.size(),
                0);

            if (sent < 0)
            {
                std::cerr << "[ERROR] send() failed." << std::endl;
                return;
            }

            std::cout << "[TX] " << error_response;
            continue;
        }
        
        std::cout << "[PARSE] VALID" << std::endl;

        if (!usb_.sendCommand(result.payload))
        {
            std::cerr << "[ERROR] USB sendCommand() failed."
                      << std::endl;
            continue;
        }

        std::cout << "[USB TX] Command sent to STM32"
                  << std::endl;

        std::string response;

        if (!usb_.receiveResponse(response))
        {
            std::cerr << "[ERROR] USB receiveResponse() failed."
                      << std::endl;
            continue;
        }

        std::cout << "[USB RX] Response received: "
                  << response;

        response += "\r\n";
        
        const ssize_t sent = send(
            client_fd_,
            response.c_str(),
            response.size(),
            0);

        if (sent < 0)
        {
            std::cerr << "[ERROR] send() failed."
                      << std::endl;
            return;
        }

        std::cout << "[TCP TX] Response sent to client"
                  << std::endl;



    }

}
