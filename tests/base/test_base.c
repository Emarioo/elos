/*
    TEMPLATE = base.cfg
*/

#include "trial.h"

#include "string.h"

#include "elos/elos.h"


void _start() {
    trial_start();

    trial_assert("sample", 1 == 1);

    const char* msg = "Hello\n";
    SYS_debug_log(msg, strlen(msg));

    trial_assert("sample2", 1 == 1);

    trial_end();
}
