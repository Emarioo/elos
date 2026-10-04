#pragma once


#include "elos/elos.h"

ELOS_Error net_open(const ELOS_Net_Address* address, ELOS_Net_Handle* handle);

ELOS_Error net_close(ELOS_Net_Handle handle);

ELOS_Error net_write(ELOS_Net_Handle handle, const ELOS_Net_Address* address, const void* data, uint32_t size);

ELOS_Error net_read(ELOS_Net_Handle handle, ELOS_Net_Address* address, void* buffer, uint32_t* bufferSize, uint64_t timeout_ns);

uint32_t net_ipv4_from_str(const char* address);
const char* net_ipv4_str(uint32_t address);
