/*

    Download blobs from the internet

    Primarily HTTP URL

*/

#pragma once

#include <stdint.h>

int download_blob(const char* url, const char* path);

int download_blob_memory(const char* url, void* buffer, size_t* maxSize);


