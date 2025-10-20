/*
    Functions return true on success, false on failure.
    Functions have an error parameter which:
    - Has more information on failure.
    - Can be null.
    - Will be untouched on success.
*/

#include "elos/kernel/common/types.h"


// ############################
//           TYPES
// ############################


typedef enum kernel__DeviceType {
    KERNEL__DEVICE_TYPE_NONE     = 0x0,
    KERNEL__DEVICE_TYPE_STORAGE  = 0x1,
    KERNEL__DEVICE_TYPE_ALL      = 0xFFFFFFFF,
} kernel__DeviceType;

typedef struct kernel__Device {
    kernel__DeviceType type;
    int id;
} kernel__Device;

typedef enum kernel__DeviceErrorCode {
    KERNEL__DEVICE_CODE_SUCCESS,
    KERNEL__DEVICE_CODE_BAD_PARAMETER,
} kernel__DeviceErrorCode;

typedef struct {
    kernel__DeviceErrorCode type;
    cstring message;
} kernel__DeviceError;


// ############################
//     FUNCTION INTERFACE
// ############################


/*
    @param types               Device types can be ored to include multiple device types, use KERNEL__DEVICE_TYPE_ALL for all devices.
    
    @param out_devices         Pointer to array of devices. If null then in_out_devices_len will be set to the number of devices known by the kernel.

    @param in_out_devices_len  in:  Max number of devices that can be written to the array.
                               out: Number of devices written. Or the number of devices known by the kernel if out_devices is null.
*/
bool kernel__list_devices(kernel__DeviceType types, kernel__Device* out_devices, u32* in_out_devices_len, kernel__DeviceError* out_error);


/*
    Thoughts on interface (good and dumb)

    We want to return a list of structs with device information such as type, id, and name.
    
    There are a couple of ways to do it:
    - Return an array of devices and a length. The array memory is owned by the kernel which won't work because it's kernel memory so i guess we have to allocate on heap.
      Each call to list_devices will invalidate
      the previous returned array (same array may be reused if kernel has a static global array with some max number of devices it can return, maybe 100).
      The problem is that any pointer the user provides, the kernel will write to that location. So I suppose we must check that the process who
      called is allowed to access this memory (maybe OS does this check?).
    - User provides an array with some max num devices and the kernel fills the array with information.
    - User provides an iterator struct and you call kernel__iterate_devices(). The iterator struct controls how far you've iterated.
      The problem here is that we perform many kernel calls which will be expensive. Each time we have to validate the user's wanted array address.

    We also need to handle sudden device disconnects. That's where the device info has a status flag. It says what it's state was when you called it.
    It may have changed if you try to interact with it though so you must consistently check.

    For the max number issue, we could provide an index offset so you can iterate a chunk of devices at a time. Interesting API
    but not necessary for devices since we'll probably have less than 100.

*/