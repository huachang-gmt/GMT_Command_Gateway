#ifndef USB_TRANSPORT_HPP
#define USB_TRANSPORT_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <cstddef>

class UsbTransport {
public:
    explicit UsbTransport(const std::string& device);
    ~UsbTransport();

    bool open();
    void close();

    bool isOpen() const;

    bool sendCommand(const std::string& command);

    bool sendCommandPartial(
        const std::string& command,
        std::size_t bytesToSend
    );

    bool receiveResponse(std::string& response);

private:
    std::string device_;
    int fd_;

    uint16_t nextSequence_;

    std::vector<uint8_t> lastSentPacket_;
    uint16_t lastSentSequence_;
};

#endif // USB_TRANSPORT_HPP