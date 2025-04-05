
#include "string.h"

void memcpy(void* dst, const void* src, int size) {
    while(--size)
        *((char*)dst + size) = *((char*)src + size);
}
int strlen(const char* str) {
    int n = 0;
    while(str[n++] != 0);
    return n - 1;

}
void memset(void* dst, int val, int size) {
    for(int i=0;i<size;i++) {
        *((char*)dst + i) = val;
    }
}