
#include "elos/kernel/device/device.h"


// stub for now
static inline bool valid_user_address(void* ptr) {
    if (ptr == NULL)
        return true;
    // TODO: Validate address
    return true;
}

static inline cstring temp_user_string(const char* text) {
    // TODO: strcpy from valid range
    cstring str = { text, strlen(text) };
    return str;
}


bool kernel__list_devices(kernel__DeviceType types, kernel__Device* out_devices, u32* in_out_devices_len, kernel__DeviceError* out_error) {
    // NOTE: types can be unsupported value. It just won't match with devices if some bit we don't handle is set.
    
    int res;

    res = valid_user_pointer(out_devices);
    if(!res) {
        if(out_error) {
            out_error->type = KERNEL__DEVICE_CODE_BAD_PARAMETER;
            out_error->message = temp_user_string("access violation on array parameter");
        }
        return false;
    }

    res = valid_user_pointer(in_out_devices_len);
    if(!res) {
        if(out_error) {
            out_error->type = KERNEL__DEVICE_CODE_BAD_PARAMETER;
            out_error->message = temp_user_string("access violation on length parameter");
        }
        return false;
    }

    if (!in_out_devices_len) {
        if(out_error) {
            out_error->type = KERNEL__DEVICE_CODE_BAD_PARAMETER;
            out_error->message = temp_user_string("length parameter was NULL");
        }
        return false;
    }


    if (!out_devices) {
        // TODO: Return number of devices
        *in_out_devices_len = 0;
    } else {
        if(*in_out_devices_len == 0)
            return true;

        // TODO: Fill array with devices
        *in_out_devices_len = 0;
    }

    return true;
}
