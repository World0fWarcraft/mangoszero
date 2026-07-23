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

#include "TestSupport.hpp"

#include "SessionMailbox.h"

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace
{
std::unique_ptr<WorldPacket> Packet(uint16 opcode, uint8 value)
{
    auto packet = std::make_unique<WorldPacket>(opcode, 1);
    *packet << value;
    return packet;
}

void mailboxTransfersPacketsInFifoOrder()
{
    SessionMailbox mailbox;
    CHECK(mailbox.Enqueue(Packet(1, 0x11)));
    CHECK(mailbox.Enqueue(Packet(2, 0x22)));

    WorldPacket* raw = nullptr;
    CHECK(mailbox.Next(raw));
    std::unique_ptr<WorldPacket> first(raw);
    CHECK(first->GetOpcode() == 1);
    CHECK((*first)[0] == 0x11);

    CHECK(mailbox.Next(raw));
    std::unique_ptr<WorldPacket> second(raw);
    CHECK(second->GetOpcode() == 2);
    CHECK((*second)[0] == 0x22);
    CHECK(!mailbox.Next(raw));
}

void closedMailboxRejectsNewOwnership()
{
    SessionMailbox mailbox;
    mailbox.Close();

    CHECK(mailbox.IsClosed());
    CHECK(!mailbox.Enqueue(Packet(3, 0x33)));
    WorldPacket* raw = nullptr;
    CHECK(!mailbox.Next(raw));
}

void closeRacingProducersLeavesNoPostClosePackets()
{
    SessionMailbox mailbox;
    std::atomic<bool> start{false};
    std::vector<std::thread> producers;
    for (unsigned producer = 0; producer < 4; ++producer)
    {
        producers.emplace_back([&mailbox, &start, producer]()
        {
            while (!start.load())
                std::this_thread::yield();
            for (unsigned packet = 0; packet < 200; ++packet)
                mailbox.Enqueue(Packet(uint16(producer + 1), uint8(packet)));
        });
    }

    start.store(true);
    mailbox.Close();
    for (std::thread& producer : producers)
        producer.join();

    WorldPacket* raw = nullptr;
    CHECK(!mailbox.Next(raw));
    CHECK(!mailbox.Enqueue(Packet(9, 0x99)));
}

void detachedRegistryRouteCannotReachItsReplacement()
{
    auto oldMailbox = std::make_shared<SessionMailbox>();
    std::shared_ptr<SessionMailbox> retainedDelivery = oldMailbox;
    oldMailbox->Close();

    auto replacement = std::make_shared<SessionMailbox>();
    CHECK(!retainedDelivery->Enqueue(Packet(4, 0x44)));
    CHECK(replacement->Enqueue(Packet(5, 0x55)));

    WorldPacket* raw = nullptr;
    CHECK(replacement->Next(raw));
    std::unique_ptr<WorldPacket> packet(raw);
    CHECK(packet->GetOpcode() == 5);
    CHECK(!replacement->Next(raw));
}

void closeDrainsResidualPackets()
{
    SessionMailbox mailbox;
    CHECK(mailbox.Enqueue(Packet(7, 0x77)));
    CHECK(mailbox.Enqueue(Packet(8, 0x88)));
    mailbox.Close();

    WorldPacket* raw = nullptr;
    CHECK(mailbox.IsClosed());
    CHECK(!mailbox.Next(raw));
    CHECK(!mailbox.Enqueue(Packet(9, 0x99)));
}
}

int main()
{
    mailboxTransfersPacketsInFifoOrder();
    closedMailboxRejectsNewOwnership();
    closeRacingProducersLeavesNoPostClosePackets();
    detachedRegistryRouteCannotReachItsReplacement();
    closeDrainsResidualPackets();
    return mangos::test::failures == 0 ? 0 : 1;
}
