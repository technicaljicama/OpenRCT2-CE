#pragma region Copyright (c) 2014-2017 OpenRCT2 Developers
/*****************************************************************************
 * OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
 *
 * OpenRCT2 is the work of many authors, a full list can be found in contributors.md
 * For more information, visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * A full copy of the GNU General Public License can be found in licence.txt
 *****************************************************************************/
#pragma endregion

#ifndef DISABLE_NETWORK

#include "NetworkTypes.h"
#include "NetworkPacket.h"


uint8 * NetworkPacket::GetData()
{
    return Data.data();
}

uint32 NetworkPacket::GetCommand()
{
    if (Data.size() >= sizeof(uint32))
    {
        return ByteSwapBE(*(uint32 *)(Data.data()));
    }
    else
    {
        return NETWORK_COMMAND_INVALID;
    }
}

void NetworkPacket::Clear()
{
    BytesTransferred = 0;
    BytesRead = 0;
    Data.clear();
}

bool NetworkPacket::CommandRequiresAuth()
{
    switch (GetCommand()) {
    case NETWORK_COMMAND_PING:
    case NETWORK_COMMAND_AUTH:
    case NETWORK_COMMAND_TOKEN:
    case NETWORK_COMMAND_GAMEINFO:
    case NETWORK_COMMAND_OBJECTS:
        return false;
    default:
        return true;
    }
}

void NetworkPacket::Write(const uint8 * bytes, size_t size)
{
    Data.insert(Data.end(), bytes, bytes + size);
}

void NetworkPacket::WriteString(const utf8 * string)
{
    Write((uint8 *)string, strlen(string) + 1);
}

const uint8 * NetworkPacket::Read(size_t size)
{
    if (BytesRead + size > NetworkPacket::Size)
    {
        return nullptr;
    }
    else
    {
        uint8 * data = &GetData()[BytesRead];
        BytesRead += size;
        return data;
    }
}

const utf8 * NetworkPacket::ReadString()
{
    char * str = (char *)&GetData()[BytesRead];
    char * strend = str;
    while (BytesRead < Size && *strend != 0)
    {
        BytesRead++;
        strend++;
    }
    if (*strend != 0)
    {
        return nullptr;
    }
    BytesRead++;
    return str;
}

#endif
