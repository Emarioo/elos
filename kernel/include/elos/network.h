#pragma once


void NET_init();


void NET_poll();

void NET_send_packet(void* buffer, int size);
