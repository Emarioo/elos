
#include "elos/kernel/common/types.h"

int strlen(const char* ptr) {
    const char* base = ptr;
    while(*(ptr++)) ;
    return (u64)ptr - (u64)base;
}





void print(const cstring text) {
    
}

void prints(const char* text, int len) {
    
}


void print(const char* text, int len) {

}



// void snprintf(string buffer, const char* format, ...) {
    
// }