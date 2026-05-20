
// gcc scripts/fwrite.c -g -o int/fwrite && int/fwrite

#include "stdio.h"
#include "stdlib.h"
#include "string.h"


int main(int argc, const char** argv) {
    if (argc <= 1) {
        printf("Usage: fwrite <path>\n");
        return 1;
    }
    const char* path = argv[1];
    const char* text = "Stuff is here";

    FILE* file = fopen(path, "r+");
    if (!file) {
        printf("Cannot open %s\n", path);
        return 1;
    }
    int text_len = strlen(text);
    size_t res = fwrite(text, 1, text_len, file);
    if (res != text_len) {
        printf("Cannot write %d bytes to %s\n", text_len, path);
        return 1;
    }
    fclose(file);
    return 0;
}
