#pragma once

#include "elos/boot_api.h"

#include "elos/syscalls.h"

void EVE_init(BootAPI* boot_api);


bool EVE_request_user_event_buffer(u32 maxEvents, ELOS_UserEventBuffer** buffer, u32* wholeBufferSize);

void EVE_push_event(ELOS_UserEvent* userEvent);

