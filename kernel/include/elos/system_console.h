/*

The system console is the escape hatch if all other programs break, especially the compositor.

The console is rendered ontop of everything and has schedule priority.
*/

#pragma once

#include "elos/common/types.h"



/*
    Enable or disable the system console.
*/
void SCON_enable(bool enabled);
bool SCON_is_enabled();

/*
    Entry point of the system console.
    A thread should be created since it will never return.
*/
void SCON_main();

