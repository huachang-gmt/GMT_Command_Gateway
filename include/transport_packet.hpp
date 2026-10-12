#ifndef TRANSPORT_PACKET_HPP
#define TRANSPORT_PACKET_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace UsbTransportPacket
{

/*
 * ============================================================
 * USB Command Transport Packet
 * ============================================================
 * ┌───────────┬─────────┬────────┬──────────┬──────────┬────────────┬───────┐
 * │ Magic 2B  │ Version │ Type   │ Sequence │ Length   │ Payload    │ CRC16 │
 * │           │  1B     │  1B    │  2B      │  2B      │ N bytes    │ 2B    │
 * └───────────┴─────────┴────────┴──────────┴──────────┴────────────┴───────┘
 *
 * Packet format:
 *
 *   Byte 0~1   : Magic
 *   Byte 2     : Version
 *   Byte 3     : Type
 *   Byte 4~5   : Sequence Number
 *   Byte 6~7   : Payload Length
 *   Byte 8~N   : Payload
 *   Last 2     : CRC16
 *
 *   Packet Size = HEADER_SIZE + Payload Length + CRC_SIZE
 *   Packet Size = 8 + Payload Length + 2   
 *
 * All multi-byte integer fields use little-endian byte order.
 *
 * Payload is opaque to the USB transport layer.
 * The transport layer does NOT parse command contents.
 *
 * Example payload:
 *
 *   "MOV R 1000 1000 500 0 0 0\r\n"
 *
 * ============================================================
 */

/*
 * Fixed packet identification.
 *
 * 0x47 0x4D = ASCII "GM"
 */
constexpr uint8_t MAGIC_BYTE_0 = 0x47; // 'G'
constexpr uint8_t MAGIC_BYTE_1 = 0x4D; // 'M'

/*
 * Transport protocol version.
 */
constexpr uint8_t PROTOCOL_VERSION = 0x01;

/*
 * Packet types.
 */
enum class PacketType : uint8_t
{
    COMMAND             = 0x01,
    RESPONSE            = 0x02,
    ACK                 = 0x03,
    RETRANSMIT_REQUEST  = 0x04
};

/*
 * Packet header size, excluding payload and CRC.
 *
 *   Magic       2 bytes
 *   Version     1 byte
 *   Type        1 byte
 *   Sequence    2 bytes
 *   Length      2 bytes
 *
 * Total = 8 bytes
 */
constexpr std::size_t HEADER_SIZE = 8;

/*
 * CRC size.
 */
constexpr std::size_t CRC_SIZE = 2;

/*
 * Minimum complete packet size:
 *
 *   Header + CRC
 */
constexpr std::size_t MIN_PACKET_SIZE = HEADER_SIZE + CRC_SIZE;

/*
 * Maximum payload size.
 *
 * This is a transport-layer safety limit.
 * It is NOT related to the number of command parameters.
 */
constexpr std::size_t MAX_PAYLOAD_SIZE = 4096;

/*
 * ------------------------------------------------------------
 * Logical Packet Header
 * ------------------------------------------------------------
 *
 * This structure describes the logical fields only.
 *
 * It is NOT written directly to USB.
 * Serialization is performed explicitly so compiler padding
 * can never affect the wire protocol.
 */
struct PacketHeader
{
    uint8_t  magic0 = MAGIC_BYTE_0;
    uint8_t  magic1 = MAGIC_BYTE_1;
    uint8_t  version = PROTOCOL_VERSION;
    uint8_t  type = 0;
    uint16_t sequence = 0;
    uint16_t payloadLength = 0;
};

/*
 * ------------------------------------------------------------
 * Logical Packet
 * ------------------------------------------------------------
 *
 * payload is completely opaque to USB Transport.
 */
struct Packet
{
    PacketHeader header{};
    std::vector<uint8_t> payload;
    uint16_t crc = 0;
};

/*
 * ------------------------------------------------------------
 * CRC16
 * ------------------------------------------------------------
 *
 * CRC is calculated over:
 *
 *   Header + Payload
 *
 * CRC bytes themselves are NOT included.
 */
uint16_t calculateCrc16(
    const uint8_t* data,
    std::size_t length
);

/*
 * ------------------------------------------------------------
 * Encode
 * ------------------------------------------------------------
 *
 * Convert a logical Packet into its wire-format byte stream.
 *
 * Returns false if the packet is invalid.
 */
bool encodePacket(
    const Packet& packet,
    std::vector<uint8_t>& encoded
);

/*
 * ------------------------------------------------------------
 * Decode
 * ------------------------------------------------------------
 *
 * Convert a complete wire-format byte stream into a logical Packet.
 *
 * Returns false if:
 *
 *   - packet is too short
 *   - magic is invalid
 *   - version is invalid
 *   - packet type is invalid
 *   - payload length is invalid
 *   - packet size is inconsistent
 *   - CRC is invalid
 */
bool decodePacket(
    const uint8_t* data,
    std::size_t length,
    Packet& packet
);

/*
 * Create Command Packet
 *
 * Convert an ASCII command into a COMMAND packet.
 *
 * CRLF handling:
 *
 * The command payload is kept exactly as supplied.
 * This function does NOT modify the command.
 *
 * Therefore the caller decides whether the command contains
 * "\r\n".
 */
bool createCommandPacket(
    const std::string& command,
    uint16_t sequence,
    Packet& packet
);

/*
 * Extract payload from a decoded packet.
 *
 * The payload is returned without interpretation.
 */
std::string payloadToString(
    const Packet& packet
);

/*
 * Convert PacketType to its wire value.
 */
constexpr uint8_t packetTypeToByte(PacketType type)
{
    return static_cast<uint8_t>(type);
}

} // namespace UsbTransportPacket

#endif // TRANSPORT_PACKET_HPP