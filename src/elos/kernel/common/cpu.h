

#include <immintrin.h>

#include "elos/kernel/common/types.h"

#include <efi.h>
#include <efilib.h>




static inline void sleep_ns(u64 ns) {
    u64 start = _rdtsc(); // used in case EFI doesn't work
    u64 freq = 4000000000;

    // EFI_TIME start_time;
    // EFI_TIME_CAPABILITIES cap;
    // EFI_STATUS status = ST->RuntimeServices->GetTime(&start_time, &cap);
    // if (EFI_ERROR(status)) {
    //     goto fallback;
    // }
    
    // while (1) {
    //     __pause();
        
    //     EFI_TIME now_time;
    //     EFI_STATUS status = ST->RuntimeServices->GetTime(&now_time, NULL);
    //     if (EFI_ERROR(status)) {
    //         goto fallback;
    //     }
        
    //     // @NOTE: We diff nanosecond all the way up to year to avoid wrap around issue and negative numbers which
    //     //    could cause you to wait indefinitely.
    //     //    In terms of precision an unsigned 64-bit integer should hold for 18.8 years.
    //     //   
    //     //   (18.69 = 2**64/(1e9 * 60 * 60 * 24 * 31 * 365))
        
    //     u64 delta_ns = 
    //     + ((u64)now_time.Year - (u64)start_time.Year) * 12LLU * 24LLU * 60LLU * 60LLU * 1000000000LLU
    //     + ((u64)now_time.Month - (u64)start_time.Month) * 31LLU * 24LLU * 60LLU * 60LLU * 1000000000LLU
    //     + ((u64)now_time.Day - (u64)start_time.Day) * 24LLU * 60LLU * 60LLU * 1000000000LLU
    //     + ((u64)now_time.Hour - (u64)start_time.Hour) * 60LLU * 60LLU * 1000000000LLU
    //     + ((u64)now_time.Minute - (u64)start_time.Minute) * 60LLU * 1000000000LLU
    //     + ((u64)now_time.Second - (u64)start_time.Second) * 1000000000LLU
    //     + (u64)now_time.Nanosecond - (u64)start_time.Nanosecond;
        
    //     if (delta_ns > ns)
    //         break;
    // }
    
    // return;
    
    // fallback:
    // printf("Fallback\n");

    while (1) {
        __pause();

        // Fallback, not great.
        u64 delta = _rdtsc() - start;
        if ((delta * 1000000000) / freq > ns)
            break;
    }
}

static inline void sleep_ms(u64 ms) {
    sleep_ns(ms * 1000000);
}

