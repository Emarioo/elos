/*
    Crude text editor.

    Slate as in a flat rock you can write on.
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "elos/syscalls.h"
#include "elos/common/string.h"


void _start();

#define ASSERT(expression) ((expression) ? true : (printf("[Assert] %s (%s:%u)\n",#expression,__FILE__,__LINE__), *((char*)0) = 0))

