/*

    Utility functions to interract with ELOS Async operations.

*/

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "elos/syscalls.h"

typedef uint64_t Async_RequestID;

Async_RequestID async_submit(ELOS_AsyncRequest* req);
bool async_wait(Async_RequestID id, ELOS_AsyncCompletion* com, size_t timeout_ns);
