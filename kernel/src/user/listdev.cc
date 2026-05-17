/*
    Program to list devices
*/


#include "elos/kernel/device/device.h"
#include "elos/kernel/log/print.h"

// static void print() {

// }

void main() {
    kernel__DeviceError error;
    bool result;

    u32 devices_len = 20;
    kernel__Device devices[20];

    result = kernel__list_devices(KERNEL__DEVICE_TYPE_ALL, &devices, &devices_len, &error);

    if(!result) {
        // print(error.message);
    }

}