/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#ifndef MANGOS_PROTO_PACKETCODEC_H
#define MANGOS_PROTO_PACKETCODEC_H

#include "Platform/Define.h"
#include "Utilities/WorldPacket.h"

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace proto
{
constexpr std::size_t CLIENT_HEADER_SIZE = 6;
constexpr std::size_t SERVER_HEADER_SIZE = 4;
constexpr uint32 MAX_CLIENT_PACKET_SIZE = 10240;

enum class DecodeStatus
{
    NeedMore,
    Ready,
    Malformed
};

class PacketCodec
{
public:
    using HeaderDecryptor = std::function<void(uint8*, std::size_t)>;
    using HeaderEncryptor = std::function<void(uint8*, std::size_t)>;

    explicit PacketCodec(HeaderDecryptor decryptor = {});
    DecodeStatus Feed(uint8 const* data, std::size_t len,
        std::vector<WorldPacket>& out);
    DecodeStatus FeedOne(uint8 const* data, std::size_t len,
        std::size_t& consumed, std::vector<WorldPacket>& out);
    static std::vector<uint8> Encode(WorldPacket const& packet,
        HeaderEncryptor const& encryptor = {});

    void SetHeaderDecryptor(HeaderDecryptor decryptor)
    {
        m_decryptor = std::move(decryptor);
    }

private:
    HeaderDecryptor m_decryptor;
    uint8 m_header[CLIENT_HEADER_SIZE]{};
    std::size_t m_headerFill = 0;
    bool m_haveHeader = false;
    uint16 m_opcode = 0;
    uint32 m_payloadNeeded = 0;
    std::vector<uint8> m_payload;
};
}

#endif
