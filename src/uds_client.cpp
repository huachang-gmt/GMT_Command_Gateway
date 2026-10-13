#include "uds_client.hpp"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <poll.h>
#include <string>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace
{
constexpr const char* UDS_SOCKET_PATH =
    "/tmp/gmt_trajectory.sock";

constexpr int UDS_TIMEOUT_MS = 1000;

constexpr char UDS_SAVE_COMMAND[] =
    "SAVE_TRAJECTORY\n";

constexpr std::size_t UDS_SAVE_COMMAND_SIZE =
    sizeof(UDS_SAVE_COMMAND) - 1;
}

UdsSaveResult UdsClient::saveTrajectory()
{
    const int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd < 0)
    {
        return UdsSaveResult::CONNECT_ERROR;
    }

    sockaddr_un address{};
    address.sun_family = AF_UNIX;

    std::strncpy(
        address.sun_path,
        UDS_SOCKET_PATH,
        sizeof(address.sun_path) - 1
    );

    if (connect(
            fd,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)) < 0)
    {
        close(fd);
        return UdsSaveResult::CONNECT_ERROR;
    }

    const auto start_time = std::chrono::steady_clock::now();

    std::size_t sent_bytes = 0;

    while (sent_bytes < UDS_SAVE_COMMAND_SIZE)
    {
        const ssize_t sent = send(
            fd,
            UDS_SAVE_COMMAND + sent_bytes,
            UDS_SAVE_COMMAND_SIZE - sent_bytes,
            MSG_NOSIGNAL
        );

        if (sent > 0)
        {
            sent_bytes += static_cast<std::size_t>(sent);
            continue;
        }

        if (sent < 0 && errno == EINTR)
        {
            continue;
        }

        close(fd);
        return UdsSaveResult::SEND_ERROR;
    }

    std::string response;
    char buffer[64];

    while (true)
    {
        const auto now = std::chrono::steady_clock::now();

        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - start_time
            ).count();

        const int remaining_timeout =
            UDS_TIMEOUT_MS - static_cast<int>(elapsed);

        if (remaining_timeout <= 0)
        {
            close(fd);
            return UdsSaveResult::TIMEOUT;
        }

        pollfd poll_fd{};
        poll_fd.fd = fd;
        poll_fd.events = POLLIN;

        const int poll_result = poll(
            &poll_fd,
            1,
            remaining_timeout
        );

        if (poll_result == 0)
        {
            close(fd);
            return UdsSaveResult::TIMEOUT;
        }

        if (poll_result < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            close(fd);
            return UdsSaveResult::RECEIVE_ERROR;
        }

        if (poll_fd.revents & POLLNVAL)
        {
            close(fd);
            return UdsSaveResult::RECEIVE_ERROR;
        }

        const ssize_t received = recv(
            fd,
            buffer,
            sizeof(buffer),
            0
        );

        if (received == 0)
        {
            close(fd);
            return UdsSaveResult::RECEIVE_ERROR;
        }

        if (received < 0)
        {
            if (errno == EINTR || errno == EAGAIN ||
                errno == EWOULDBLOCK)
            {
                continue;
            }

            close(fd);
            return UdsSaveResult::RECEIVE_ERROR;
        }

        response.append(
            buffer,
            static_cast<std::size_t>(received)
        );

        if (response.size() > 64)
        {
            close(fd);
            return UdsSaveResult::INVALID_RESPONSE;
        }

        const std::size_t newline_pos =
            response.find('\n');

        if (newline_pos != std::string::npos)
        {
            if (newline_pos != response.size() - 1)
            {
                close(fd);
                return UdsSaveResult::INVALID_RESPONSE;
            }

            if (response == "SAVE_OK\n")
            {
                close(fd);
                return UdsSaveResult::SAVE_OK;
            }

            if (response == "SAVE_ERROR\n")
            {
                close(fd);
                return UdsSaveResult::SAVE_ERROR;
            }

            close(fd);
            return UdsSaveResult::INVALID_RESPONSE;
        }

        if (response.size() >= 64)
        {
            close(fd);
            return UdsSaveResult::INVALID_RESPONSE;
        }
    }
}