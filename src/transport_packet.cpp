#include "transport_packet.hpp"

#include <cstddef>
#include <cstdint>

namespace UsbTransportPacket
{

uint16_t calculateCrc16(
    const uint8_t* data,
    std::size_t length
)
{
    constexpr uint16_t CRC16_POLYNOMIAL = 0xA001;
    constexpr uint16_t CRC16_INITIAL    = 0xFFFF;

    uint16_t crc = CRC16_INITIAL;

    for (std::size_t i = 0; i < length; ++i) {
        crc ^= data[i];

        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x0001U) {
                crc = static_cast<uint16_t>(
                    (crc >> 1) ^ CRC16_POLYNOMIAL
                );
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

namespace
{

void appendUint16LE(
    std::vector<uint8_t>& packet,
    uint16_t value
)
{
    packet.push_back(
        static_cast<uint8_t>(value & 0x00FFU)
    );

    packet.push_back(
        static_cast<uint8_t>((value >> 8) & 0x00FFU)
    );
}

uint16_t readUint16LE(
    const uint8_t* data
)
{
    return static_cast<uint16_t>(
        static_cast<uint16_t>(data[0]) |
        (static_cast<uint16_t>(data[1]) << 8)
    );
}

bool isValidPacketType(uint8_t type)
{
    switch (static_cast<PacketType>(type)) {
    case PacketType::COMMAND:
    case PacketType::RESPONSE:
    case PacketType::ACK:
    case PacketType::RETRANSMIT_REQUEST:
        return true;

    default:
        return false;
    }
}

} // namespace

bool encodePacket(
    const Packet& packet,
    std::vector<uint8_t>& encoded
)
{
    encoded.clear();

    if (packet.payload.size() > MAX_PAYLOAD_SIZE) {
        return false;
    }

    if (packet.payload.size() > 0xFFFFU) {
        return false;
    }

    if (packet.header.version != PROTOCOL_VERSION) {
        return false;
    }

    if (packet.header.magic0 != MAGIC_BYTE_0 ||
        packet.header.magic1 != MAGIC_BYTE_1) {
        return false;
    }

    if (!isValidPacketType(packet.header.type)) {
        return false;
    }

    const uint16_t payloadLength =
        static_cast<uint16_t>(packet.payload.size());

    /*
     * Header
     */
    encoded.reserve(
        HEADER_SIZE +
        payloadLength +
        CRC_SIZE
    );

    encoded.push_back(MAGIC_BYTE_0);
    encoded.push_back(MAGIC_BYTE_1);
    encoded.push_back(packet.header.version);
    encoded.push_back(packet.header.type);

    appendUint16LE(
        encoded,
        packet.header.sequence
    );

    /*
     * IMPORTANT:
     *
     * Payload length is automatically calculated here.
     * The caller does not need to manually specify it.
     */
    appendUint16LE(
        encoded,
        payloadLength
    );

    /*
     * Payload
     */
    encoded.insert(
        encoded.end(),
        packet.payload.begin(),
        packet.payload.end()
    );

    /*
     * CRC covers Header + Payload.
     */
    const uint16_t crc =
        calculateCrc16(
            encoded.data(),
            encoded.size()
        );

    appendUint16LE(encoded, crc);

    return true;
}

bool decodePacket(
    const uint8_t* data,
    std::size_t length,
    Packet& packet
)
{
    if (data == nullptr) {
        return false;
    }

    if (length < MIN_PACKET_SIZE) {
        return false;
    }

    /*
     * Magic
     */
    if (data[0] != MAGIC_BYTE_0 ||
        data[1] != MAGIC_BYTE_1) {
        return false;
    }

    /*
     * Version
     */
    if (data[2] != PROTOCOL_VERSION) {
        return false;
    }

    /*
     * Packet type
     */
    if (!isValidPacketType(data[3])) {
        return false;
    }

    /*
     * Sequence and payload length.
     */
    const uint16_t sequence =
        readUint16LE(&data[4]);

    const uint16_t payloadLength =
        readUint16LE(&data[6]);

    if (payloadLength > MAX_PAYLOAD_SIZE) {
        return false;
    }

    /*
     * Complete packet must be:
     *
     * Header + Payload + CRC
     */
    const std::size_t expectedLength =
        HEADER_SIZE +
        static_cast<std::size_t>(payloadLength) +
        CRC_SIZE;

    if (length != expectedLength) {
        return false;
    }

    /*
     * CRC stored at the end of packet.
     */
    const uint16_t receivedCrc =
        readUint16LE(
            &data[HEADER_SIZE + payloadLength]
        );

    /*
     * CRC covers Header + Payload.
     */
    const uint16_t calculatedCrc =
        calculateCrc16(
            data,
            HEADER_SIZE + payloadLength
        );

    if (receivedCrc != calculatedCrc) {
        return false;
    }

    /*
     * Packet is valid.
     */
    packet.header.magic0 = data[0];
    packet.header.magic1 = data[1];
    packet.header.version = data[2];
    packet.header.type = data[3];
    packet.header.sequence = sequence;
    packet.header.payloadLength = payloadLength;

    packet.payload.assign(
        data + HEADER_SIZE,
        data + HEADER_SIZE + payloadLength
    );

    packet.crc = receivedCrc;

    return true;
}

bool createCommandPacket(
    const std::string& command,
    uint16_t sequence,
    Packet& packet
)
{
    if (command.size() > MAX_PAYLOAD_SIZE) {
        return false;
    }

    packet.header.magic0 = MAGIC_BYTE_0;
    packet.header.magic1 = MAGIC_BYTE_1;
    packet.header.version = PROTOCOL_VERSION;
    packet.header.type =
        packetTypeToByte(PacketType::COMMAND);
    packet.header.sequence = sequence;

    /*
     * Length is intentionally derived from the payload.
     */
    packet.header.payloadLength =
        static_cast<uint16_t>(command.size());

    packet.payload.assign(
        command.begin(),
        command.end()
    );

    packet.crc = 0;

    return true;
}

std::string payloadToString(
    const Packet& packet
)
{
    return std::string(
        packet.payload.begin(),
        packet.payload.end()
    );
}

} // namespace UsbTransportPacket