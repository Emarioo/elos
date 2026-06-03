#pragma once

#include "elos/boot_api.h"

#include "elos/syscalls.h"

void EVE_init(BootAPI* boot_api);


bool EVE_request_user_event_buffer(u64 size, ELOS_UserEventBuffer** buffer);

void EVE_push_event(ELOS_UserEvent* userEvent);

