#pragma once


#include "elos/elos.h"

ELOS_Error net_open(const ELOS_Net_Address* address, ELOS_Net_Handle* handle);

ELOS_Error net_close(ELOS_Net_Handle handle);

ELOS_Error net_write(ELOS_Net_Handle handle, const ELOS_Net_Address* address, const void* data, u32 size);

ELOS_Error net_read(ELOS_Net_Handle handle, ELOS_Net_Address* address, void* buffer, u32* bufferSize, u64 timeout_ns);

u32 net_ipv4_from_str(const char* address);
const char* net_ipv4_str(u32 address);
