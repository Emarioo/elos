#include "elos/kernel/frame/font.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main() {
    int res;
    Font* font;
    const char* path = "res/PixelOperator.ttf";
    FILE * file = fopen(path, "rb");
    assert(file);

    fseek(file, 0, SEEK_END);
    int fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    void* data = malloc(fileSize);
    assert(data);

    res = fread(data, 1, fileSize, file);
    assert(res == fileSize);
    fclose(file);

    bool yes = font__load_from_bytes(data, fileSize, &font);

    if(yes) {
        printf("SUCCESS\n");
    } else {
        printf("Couldn't load font\n");
    }

    return 0;
}
