#include "usb_transport.hpp"
#include "transport_packet.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <sys/select.h>
#include <termios.h>
#include <vector>
#include <chrono>

#include <thread>

UsbTransport::UsbTransport(const std::string& device)
    : device_(device),
      fd_(-1),
      nextSequence_(1)
{
}

UsbTransport::~UsbTransport()
{
    close();
}

bool UsbTransport::open()
{
    if (fd_ >= 0) {
        return true;
    }

    fd_ = ::open(
        device_.c_str(),
        O_RDWR | O_NOCTTY
    );

    if (fd_ < 0) {
        return false;
    }

    struct termios tty;

    if (tcgetattr(fd_, &tty) != 0) {
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    cfmakeraw(&tty);

    /*
     * USB CDC ACM does not depend on a real UART baud rate,
     * but configure a conventional 115200 8N1 terminal state.
     */
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    return true;
}

void UsbTransport::close()
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool UsbTransport::isOpen() const
{
    return (fd_ >= 0);
}

bool UsbTransport::sendCommand(const std::string& command)
{
    if (fd_ < 0) {
        return false;
    }

    using namespace UsbTransportPacket;

    /*
     * Create logical COMMAND packet.
     */
    Packet packet;

    if (!createCommandPacket(
            command,
            nextSequence_,
            packet)) {

        return false;
    }

    /*
     * Encode packet into wire-format bytes.
     */
    std::vector<uint8_t> encoded;

    if (!encodePacket(packet, encoded)) {
        return false;
    }

    lastSentPacket_ = encoded;
    lastSentSequence_ = packet.header.sequence;

    std::cout
        << "[USB TX] COMMAND"
        << " seq=" << packet.header.sequence
        << " payload=" << packet.payload.size()
        << " total=" << encoded.size()
        << '\n';

    std::cout << "[USB TX] HEX:";

    for (const uint8_t byte : encoded)
    {
        std::printf(" %02X", byte);
    }

    std::printf("\n");

    /*
     * Send the complete encoded packet.
     *
     * write() is allowed to write fewer bytes than requested,
     * so keep writing until the entire packet has been sent.
     */
    std::size_t totalWritten = 0;

    while (totalWritten < encoded.size()) {

        const ssize_t written = ::write(
            fd_,
            encoded.data() + totalWritten,
            encoded.size() - totalWritten
        );

        if (written < 0) {

            if (errno == EINTR) {
                continue;
            }

            return false;
        }

        if (written == 0) {
            return false;
        }

        totalWritten += static_cast<std::size_t>(written);
    }

    /*
     * Packet successfully transmitted.
     *
     * Advance sequence only after the complete packet
     * has been written successfully.
     */
    ++nextSequence_;

    return true;

}


bool UsbTransport::sendCommandPartial(
    const std::string& command,
    std::size_t bytesToSend
)
{
    if (fd_ < 0) {
        return false;
    }

    using namespace UsbTransportPacket;

    Packet packet;

    if (!createCommandPacket(
            command,
            nextSequence_,
            packet)) {

        return false;
    }

    std::vector<uint8_t> encoded;

    if (!encodePacket(packet, encoded)) {
        return false;
    }

    if (bytesToSend == 0U ||
        bytesToSend > encoded.size()) {

        return false;
    }

    /*
     * Save the COMPLETE packet before sending.
     *
     * This is the retransmission cache.
     */
    lastSentPacket_ = encoded;
    lastSentSequence_ = packet.header.sequence;

    std::cout
        << "[USB TX TEST] PARTIAL COMMAND"
        << " seq=" << packet.header.sequence
        << " sending=" << bytesToSend
        << "/" << encoded.size()
        << '\n';

    std::cout << "[USB TX TEST] HEX:";

    for (std::size_t i = 0U; i < bytesToSend; ++i) {
        std::printf(
            " %02X",
            encoded[i]
        );
    }

    std::printf("\n");

    std::size_t totalWritten = 0U;

    while (totalWritten < bytesToSend) {

        const ssize_t written = ::write(
            fd_,
            encoded.data() + totalWritten,
            bytesToSend - totalWritten
        );

        if (written < 0) {

            if (errno == EINTR) {
                continue;
            }

            return false;
        }

        if (written == 0) {
            return false;
        }

        totalWritten += static_cast<std::size_t>(written);
    }

    /*
     * Do NOT advance nextSequence_ yet.
     *
     * The packet has intentionally not been completely
     * transmitted. It will be advanced after the cached
     * packet is successfully retransmitted.
     */

    return true;
}



bool UsbTransport::receiveResponse(std::string& response)
{
    if (fd_ < 0) {
        return false;
    }

    response.clear();

    using namespace UsbTransportPacket;

    static std::vector<uint8_t> rxBuffer;

    constexpr std::size_t READ_BUFFER_SIZE = 256;

    uint8_t readBuffer[READ_BUFFER_SIZE];

    constexpr int RX_TIMEOUT_MS = 100U; //100;  // 暫時測試數值，袁術直視 100

    auto lastRxTime = std::chrono::steady_clock::now();

    while (true) {

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd_, &readfds);

        const auto now =
            std::chrono::steady_clock::now();

        const auto elapsed =
            std::chrono::duration_cast<
                std::chrono::milliseconds
            >(now - lastRxTime).count();

        const int remainingMs =
            RX_TIMEOUT_MS -
            static_cast<int>(elapsed);

        if (!rxBuffer.empty() &&
            remainingMs <= 0) {

            std::cerr
                << "[USB RX] ERROR: RX TIMEOUT - incomplete packet, received="
                << rxBuffer.size()
                << " bytes"
                << '\n';

            rxBuffer.clear();

            return false;
        }

        struct timeval timeout;

        const int waitMs =
            (rxBuffer.empty())
                ? RX_TIMEOUT_MS
                : remainingMs;

        timeout.tv_sec = waitMs / 1000;
        timeout.tv_usec = (waitMs % 1000) * 1000;



        const int result = ::select(
            fd_ + 1,
            &readfds,
            nullptr,
            nullptr,
            &timeout
        );

        if (result < 0) {

            if (errno == EINTR) {
                continue;
            }

            return false;
        }

        if (result == 0) {

            if (!rxBuffer.empty()) {

                std::cerr
                    << "[USB RX] ERROR: RX TIMEOUT - incomplete packet, received="
                    << rxBuffer.size()
                    << " bytes"
                    << '\n';

                rxBuffer.clear();

                return false;
            }

            return false;
        }



        const ssize_t received = ::read(
            fd_,
            readBuffer,
            sizeof(readBuffer)
        );

        if (received < 0) {

            if (errno == EINTR) {
                continue;
            }

            return false;
        }

        if (received == 0) {
            return false;
        }

        lastRxTime = std::chrono::steady_clock::now(); // 每次真的收到 USB 資料，就把 100 ms timeout 重新開始。

        /*
         * USB CDC is a byte stream.
         *
         * One read() does not necessarily equal one
         * transport packet.
         */
        rxBuffer.insert(
            rxBuffer.end(),
            readBuffer,
            readBuffer + received
        );

        /*
         * Print exactly what CM5 received.
         */
        std::cout
            << "[USB RX] Len="
            << received
            << '\n';

        std::cout
            << "[USB RX] HEX:";

        for (ssize_t i = 0; i < received; ++i) {
            std::printf(
                " %02X",
                readBuffer[i]
            );
        }

        std::printf("\n");

        /*
         * Try to decode one or more complete packets
         * from the accumulated byte stream.
         */
        while (true) {

            /*
             * Need at least two bytes to search for MAGIC.
             */
            if (rxBuffer.size() < 2U) {
                break;
            }

            /*
             * Find MAGIC = 0x47 0x4D.
             */
            std::size_t magicIndex = 0U;

            while ((magicIndex + 1U) < rxBuffer.size()) {

                if (rxBuffer[magicIndex] == MAGIC_BYTE_0 &&
                    rxBuffer[magicIndex + 1U] == MAGIC_BYTE_1) {
                    break;
                }

                ++magicIndex;
            }

            /*
             * MAGIC not completely found.
             *
             * Preserve a possible trailing 0x47.
             */
            if ((magicIndex + 1U) >= rxBuffer.size()) {

                if (rxBuffer.back() == MAGIC_BYTE_0) {
                    rxBuffer.erase(
                        rxBuffer.begin(),
                        rxBuffer.end() - 1
                    );
                } else {
                    rxBuffer.clear();
                }

                break;
            }

            /*
             * Remove bytes before MAGIC.
             */
            if (magicIndex > 0U) {
                rxBuffer.erase(
                    rxBuffer.begin(),
                    rxBuffer.begin() + magicIndex
                );
            }

            /*
             * Need complete 8-byte header.
             */
            if (rxBuffer.size() < HEADER_SIZE) {
                break;
            }

            /*
             * Payload length is bytes 6~7.
             */
            const uint16_t payloadLength =
                static_cast<uint16_t>(
                    static_cast<uint16_t>(rxBuffer[6]) |
                    (static_cast<uint16_t>(rxBuffer[7]) << 8)
                );

            if (payloadLength > MAX_PAYLOAD_SIZE) {

                std::cerr
                    << "[USB RX] Invalid payload length: "
                    << payloadLength
                    << '\n';

                rxBuffer.erase(
                    rxBuffer.begin()
                );

                continue;
            }

            const std::size_t expectedLength =
                HEADER_SIZE +
                static_cast<std::size_t>(payloadLength) +
                CRC_SIZE;

            /*
             * IMPORTANT:
             *
             * Packet is incomplete.
             *
             * This is NOT an error.
             * More USB CDC data may arrive later.
             */
            if (rxBuffer.size() < expectedLength) {

                std::cout
                    << "[USB RX] Fragment: "
                    << rxBuffer.size()
                    << "/"
                    << expectedLength
                    << " bytes, waiting..."
                    << '\n';

                break;
            }

            /*
             * We have a complete packet.
             */
            Packet packet;

            if (!decodePacket(
                    rxBuffer.data(),
                    expectedLength,
                    packet)) {

                std::cerr
                    << "[USB RX] Packet decode/CRC ERROR"
                    << '\n';

                /*
                 * Discard one byte and attempt to
                 * resynchronize to the next MAGIC.
                 */
                rxBuffer.erase(
                    rxBuffer.begin()
                );

                continue;
            }

            std::cout
                << "[USB RX] Packet OK"
                << " type=0x"
                << std::hex
                << static_cast<unsigned>(
                    packet.header.type
                )
                << std::dec
                << " seq="
                << packet.header.sequence
                << " payload="
                << packet.header.payloadLength
                << '\n';

            std::cout
                << "[USB RX] Payload HEX:";

            for (uint8_t byte : packet.payload) {
                std::printf(
                    " %02X",
                    byte
                );
            }

            std::printf("\n");


            /*
            * Handle RETRANSMIT_REQUEST.
            */
            if (packet.header.type ==
                packetTypeToByte(PacketType::RETRANSMIT_REQUEST)) {

                if (packet.payload.size() != 2U) {

                    std::cerr
                        << "[USB RX] Invalid RETRANSMIT_REQUEST payload length: "
                        << packet.payload.size()
                        << '\n';

                    rxBuffer.erase(
                        rxBuffer.begin(),
                        rxBuffer.begin() + expectedLength
                    );

                    continue;
                }

                const uint16_t requestedSequence =
                    static_cast<uint16_t>(
                        static_cast<uint16_t>(packet.payload[0]) |
                        (static_cast<uint16_t>(packet.payload[1]) << 8U)
                    );

                std::cout
                    << "[USB RETRANSMIT] Request seq="
                    << requestedSequence
                    << '\n';

                if (requestedSequence != lastSentSequence_) {

                    std::cerr
                        << "[USB RETRANSMIT] Cache MISS"
                        << " requested="
                        << requestedSequence
                        << " cached="
                        << lastSentSequence_
                        << '\n';

                    rxBuffer.erase(
                        rxBuffer.begin(),
                        rxBuffer.begin() + expectedLength
                    );

                    continue;
                }

                if (lastSentPacket_.empty()) {

                    std::cerr
                        << "[USB RETRANSMIT] Cache EMPTY"
                        << '\n';

                    rxBuffer.erase(
                        rxBuffer.begin(),
                        rxBuffer.begin() + expectedLength
                    );

                    continue;
                }

                std::cout
                    << "[USB RETRANSMIT] Cache HIT"
                    << " seq="
                    << requestedSequence
                    << " bytes="
                    << lastSentPacket_.size()
                    << '\n';



                std::cout
                    << "[USB RETRANSMIT] Waiting 20 ms before resend..."
                    << '\n';

                std::this_thread::sleep_for(
                    std::chrono::milliseconds(20)
                );

                std::cout << "[USB RETRANSMIT] HEX:";

                for (const uint8_t byte : lastSentPacket_) {
                    std::printf(" %02X", byte);
                }

                std::printf("\n");

                std::size_t totalWritten = 0U;

                while (totalWritten < lastSentPacket_.size()) {

                    const ssize_t written = ::write(
                        fd_,
                        lastSentPacket_.data() + totalWritten,
                        lastSentPacket_.size() - totalWritten
                    );

                    if (written < 0) {

                        if (errno == EINTR) {
                            continue;
                        }

                        std::cerr
                            << "[USB RETRANSMIT] write() failed: "
                            << std::strerror(errno)
                            << '\n';

                        return false;
                    }

                    if (written == 0) {
                        return false;
                    }

                    totalWritten += static_cast<std::size_t>(written);
                }

                /*
                * The cached packet has now been retransmitted completely.
                */
                ++nextSequence_;

                std::cout
                    << "[USB RETRANSMIT] Complete packet retransmitted"
                    << " seq="
                    << requestedSequence
                    << " bytes="
                    << totalWritten
                    << '\n';

                rxBuffer.erase(
                    rxBuffer.begin(),
                    rxBuffer.begin() + expectedLength
                );

                continue;
            }



            /*
             * We expect RESPONSE for this test.
             */
            if (packet.header.type ==
                packetTypeToByte(PacketType::RESPONSE)) {

                response = payloadToString(packet);

                /*
                 * Remove the complete packet from
                 * the stream accumulator.
                 */
                rxBuffer.erase(
                    rxBuffer.begin(),
                    rxBuffer.begin() + expectedLength
                );

                return true;
            }

            /*
             * Valid packet, but not a RESPONSE.
             * Remove it and continue looking.
             */
            rxBuffer.erase(
                rxBuffer.begin(),
                rxBuffer.begin() + expectedLength
            );
        }
    }
}