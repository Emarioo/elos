
#include "elos/kernel/device/device.h"

#include "elos/kernel/driver/pata.h"


// stub for now
static inline bool valid_user_address(void* ptr, int size) {
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



static int g_devices_len;
static elos__Device g_devices[100];



static elos__Device* find_device_by_id(elos__DeviceID id) {
    for (int i=0;i<g_devices_len;i++) {
        elos__Device* dev = &g_devices[i];
        if (dev->id == id && dev->type != elos__DEVICE_TYPE_NONE) {
            return dev;
        }
    }
    return NULL;
}



bool elos__list_devices(elos__DeviceType types, elos__Device* out_devices, u32* in_out_devices_len, elos__DeviceError* out_error) {
    // NOTE: types can be unsupported value. It just won't match with devices if some bit we don't handle is set.
    
    int res;

    if (!in_out_devices_len) {
        if(out_error) {
            out_error->type = elos__DEVICE_CODE_BAD_PARAMETER;
            out_error->message = temp_user_string("length parameter was NULL");
        }
        return false;
    }

    res = valid_user_pointer(in_out_devices_len, 4);
    if(!res) {
        if(out_error) {
            out_error->type = elos__DEVICE_CODE_BAD_PARAMETER;
            out_error->message = temp_user_string("access violation on length parameter");
        }
        return false;
    }

    res = valid_user_pointer(out_devices, sizeof(*out_devices) * *in_out_devices_len);
    if(!res) {
        if(out_error) {
            out_error->type = elos__DEVICE_CODE_BAD_PARAMETER;
            out_error->message = temp_user_string("access violation on array parameter");
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



bool elos__get_storage_information(elos__DeviceID id, u64* max_bytes) {
    int res;
    
    if (!max_bytes)
        return false;

    res = valid_user_address(max_bytes, 8);
    if (!res)
        return false;

    elos__Device* dev = find_device_by_id(id);
    if(!dev)
        return false;

    if (dev->type != elos__DEVICE_TYPE_STORAGE)
        return false;

    *max_bytes = dev->storage.max_bytes;
    
    return true;
}



bool elos__read_bytes(elos__DeviceID id, u64 offset, u64 size, u8* buffer) {
    int res;

    if (!buffer || size == 0)
        return false;
    
    res = valid_user_address(buffer, size);
    if (!res)
        return false;

    elos__Device* dev = find_device_by_id(id);
    if(!dev)
        return false;

    if (dev->type != elos__DEVICE_TYPE_STORAGE)
        return false;

    // TODO: Check integer overflow
    if (offset + size > dev->storage.max_bytes)
        return false;
    
    u8 temp_sector[512];
    u64 next_size = size;
    u64 next_offset = offset;
    while (next_size > 0) {

        if (next_offset % 512 == 0 && next_size >= 512) {
            ata_read_sectors(buffer, next_offset / 512, next_size / 512);
            next_offset += 512;
            next_size -= 512;
            continue;
        }
        int partSize = 512 - next_offset % 512;
        if (next_size < partSize)
            partSize = next_size;
        
        ata_read_sectors(temp_sector, next_offset / 512, 1);
        memcpy(buffer, temp_sector + next_offset % 512, partSize);
        next_offset += partSize;
        next_size -= partSize;
    }

    return true;
}



bool elos__write_sector(elos__DeviceID id, u64 offset, int size, u8* buffer) {
    elos__Device* dev = find_device_by_id(id);

    return false;
}



bool elos__scan_system() {

    return false;
}
