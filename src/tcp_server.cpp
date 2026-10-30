#include "tcp_server.h"
#include "command_parser.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

TcpServer::TcpServer()
    : server_fd_(-1),
      client_fd_(-1),
      usb_("/dev/serial/by-id/usb-STMicroelectronics_STM32_Virtual_ComPort_3069347A3332-if00")
{
}

TcpServer::~TcpServer()
{
    /*
     * 關閉 TCP Client Socket。
     * 如果程式正常結束，確保不留下已建立的連線。
     */
    if (client_fd_ >= 0)
    {
        ::close(client_fd_);
        client_fd_ = -1;
    }

    /*
     * 關閉 TCP Server Socket。
     */
    if (server_fd_ >= 0)
    {
        ::close(server_fd_);
        server_fd_ = -1;
    }

    /*
     * 關閉 USB CDC。
     */
    usb_.close();
}

bool TcpServer::start()
{
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd_ < 0)
    {
        std::cerr << "[ERROR] socket() failed." << std::endl;
        return false;
    }

    std::cout << "[OK] socket() created. fd="
              << server_fd_ << std::endl;

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(DEFAULT_PORT);

    if (bind(server_fd_,
             reinterpret_cast<struct sockaddr*>(&server_addr),
             sizeof(server_addr)) < 0)
    {
        std::cerr << "[ERROR] bind() failed." << std::endl;
        ::close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    std::cout << "[OK] bind() successful. port="
              << DEFAULT_PORT << std::endl;

    if (listen(server_fd_, 1) < 0)
    {
        std::cerr << "[ERROR] listen() failed." << std::endl;
        ::close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    std::cout << "[OK] listen() successful." << std::endl;

    /*
     * TCP Server 初始化完成。
     *
     * 注意：
     * 這裡不再呼叫 accept()。
     *
     * accept() 將移至 run() 的外層 while(true)，
     * 讓 Qt Client 離線後仍然可以等待下一次連線。
     */

    std::cout << "[WAIT] Waiting for client connection..."
              << std::endl;

    return true;
}

void TcpServer::run()
{
    std::cout << "[RUN] TCP server waiting for clients."
              << std::endl;

    /*
     * 外層迴圈：
     *
     * 持續等待 Qt Client 連線。
     * 當目前 Client 離線後，回到這裡重新 accept()。
     *
     * 只有程式被外部終止，或發生需要停止服務的情況，
     * 才會離開這個迴圈。
     */
    while (true)
    {
        /*
         * 等待 Qt Client 建立 TCP 連線。
         */
        client_fd_ = accept(server_fd_, nullptr, nullptr);

        if (client_fd_ < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            std::cerr << "[ERROR] accept() failed: "
                      << std::strerror(errno)
                      << std::endl;

            usleep(100000);
            continue;
        }

        std::cout << "[OK] Client connected. fd="
                  << client_fd_ << std::endl;

        /*
         * 每次 Qt Client 連線時，確認 USB CDC 已開啟。
         *
         * 如果 USB 開啟失敗，關閉目前 Client，
         * 然後繼續等待下一次 Qt 連線。
         */
        if (!usb_.open())
        {
            std::cerr << "[ERROR] Failed to open USB device."
                      << std::endl;

            ::close(client_fd_);
            client_fd_ = -1;

            continue;
        }

        std::cout << "[OK] USB device opened."
                  << std::endl;

        std::cout << "[RUN] Client communication loop started."
                  << std::endl;

        /*
         * client_connected：
         * 記錄目前 Qt Client 是否仍然保持連線。
         */
        bool client_connected = true;

        /*
         * 內層迴圈：
         *
         * 處理目前 Qt Client 的命令及 STM32 回應。
         * 當 Client 離線時，離開內層迴圈。
         */
        while (client_connected)
        {
            char buffer[1024];

            const ssize_t received = recv(
                client_fd_,
                buffer,
                sizeof(buffer) - 1,
                0);

            if (received < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }

                std::cerr << "[ERROR] recv() failed: "
                          << std::strerror(errno)
                          << std::endl;

                client_connected = false;
                break;
            }

            if (received == 0)
            {
                std::cout << "[INFO] Client disconnected."
                          << std::endl;

                client_connected = false;
                break;
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
                    MSG_NOSIGNAL);

                if (sent < 0)
                {
                    std::cerr << "[ERROR] send() failed."
                              << std::endl;

                    client_connected = false;
                    break;
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
            std::string latestResponse;

            while (true)
            {
                const UsbReceiveResult rxResult =
                    usb_.receiveResponse(response);

                if (rxResult != UsbReceiveResult::RESPONSE_RECEIVED)
                {
                    if (rxResult == UsbReceiveResult::TIMEOUT)
                    {
                        std::cerr
                            << "[ERROR] USB receiveResponse() timeout."
                            << std::endl;
                    }
                    else
                    {
                        std::cerr
                            << "[ERROR] USB receiveResponse() failed."
                            << std::endl;
                    }

                    break;
                }

                std::cout << "[USB RX] Response received: "
                          << response;

                latestResponse = response;

                response += "\r\n";

                std::size_t totalSent = 0U;

                while (totalSent < response.size())
                {
                    const ssize_t sent = send(
                        client_fd_,
                        response.data() + totalSent,
                        response.size() - totalSent,
                        MSG_NOSIGNAL);

                    if (sent < 0)
                    {
                        if (errno == EINTR)
                        {
                            continue;
                        }

                        std::cerr << "[ERROR] send() failed: "
                                  << std::strerror(errno)
                                  << std::endl;

                        client_connected = false;
                        break;
                    }

                    if (sent == 0)
                    {
                        std::cerr << "[ERROR] send() returned 0."
                                  << std::endl;

                        client_connected = false;
                        break;
                    }

                    totalSent += static_cast<std::size_t>(sent);
                }

                /*
                 * TCP 傳送失敗代表目前 Client 已無法正常通訊。
                 * 離開 USB 回應迴圈，稍後清理目前連線。
                 */
                if (!client_connected)
                {
                    break;
                }

                std::cout << "[TCP TX] Response sent to client"
                          << std::endl;

                /*
                 * MOV 等命令收到 >Done 時，
                 * 結束目前命令的 USB Response Session。
                 */
                if ((result.command == "MOV" ||
                     result.command == "MRV" ||
                     result.command == "MSV" ||
                     result.command == "MSR" ||
                     result.command == "SSI" ||
                     result.command == "SSP" ||
                     result.command == "SLI") &&
                    (latestResponse == ">Done" ||
                     latestResponse == ">Done\r\n"))
                {
                    break;
                }

                /*
                 * 其他命令收到 >Done 時，
                 * 同樣結束目前命令的 Response Session。
                 */
                if (latestResponse == ">Done" ||
                    latestResponse == ">Done\r\n")
                {
                    break;
                }
            }

            /*
             * 如果 TCP Client 已經離線，
             * 不再執行後續的命令存檔流程。
             */
            if (!client_connected)
            {
                break;
            }

            /*
             * Command-specific trajectory save conditions.
             *
             * 保留原有條件：
             * 指定命令收到 >Done 後才要求 SPI 軌跡存檔。
             */
            if ((result.command == "MOV" ||
                 result.command == "MRV" ||
                 result.command == "MSV" ||
                 result.command == "MSR" ||
                 result.command == "SSI" ||
                 result.command == "SSP" ||
                 result.command == "SLI") &&
                (latestResponse == ">Done" ||
                 latestResponse == ">Done\r\n"))
            {
                std::cout << "[UDS TX] SAVE_TRAJECTORY"
                          << std::endl;

                const UdsSaveResult uds_result =
                    uds_.saveTrajectory();

                switch (uds_result)
                {
                case UdsSaveResult::SAVE_OK:
                    std::cout << "[UDS RX] SAVE_OK"
                              << std::endl;
                    break;

                case UdsSaveResult::SAVE_ERROR:
                    std::cerr << "[UDS RX] SAVE_ERROR"
                              << std::endl;
                    break;

                case UdsSaveResult::CONNECT_ERROR:
                    std::cerr << "[UDS ERROR] Connection failed"
                              << std::endl;
                    break;

                case UdsSaveResult::SEND_ERROR:
                    std::cerr << "[UDS ERROR] Send failed"
                              << std::endl;
                    break;

                case UdsSaveResult::RECEIVE_ERROR:
                    std::cerr << "[UDS ERROR] Receive failed"
                              << std::endl;
                    break;

                case UdsSaveResult::TIMEOUT:
                    std::cerr << "[UDS ERROR] Timeout"
                              << std::endl;
                    break;

                case UdsSaveResult::INVALID_RESPONSE:
                    std::cerr << "[UDS ERROR] Invalid response"
                              << std::endl;
                    break;
                }
            }
        }

        /*
         * 目前 Qt Client 已離線或 TCP 通訊發生錯誤。
         *
         * 關閉目前 Client Socket，避免資源持續占用。
         */
        if (client_fd_ >= 0)
        {
            ::close(client_fd_);
            client_fd_ = -1;
        }

        /*
         * 關閉目前 Client 使用的 USB CDC。
         * 下次 Qt 重新連線時，會再次呼叫 usb_.open()。
         */
        usb_.close();

        std::cout << "[WAIT] Client disconnected. "
                  << "Waiting for next client connection..."
                  << std::endl;

        /*
         * 回到外層 while(true)，重新執行 accept()。
         */
    }
}