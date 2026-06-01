/*
    Client API for the Prism compositor.

    WARNING: API is very simple and flawed from a general, extendable, long term perspective.
      For example window, surface, buffer is combined into one: "PrismSurface".
*/

#pragma once


#include <stdint.h>
#include <stdbool.h>


typedef struct PrismInstance PrismInstance;
typedef struct PrismSurface PrismSurface;

typedef struct {
    int       width;
    int       height;
    int       stride;
    uint32_t* buffer;
} PrismSurfaceInfo;

/*
    Initialize Prism client and connect to server.
*/
PrismInstance* prism_init();

/*
    Ask Prism server to provide a surface.
*/
PrismSurface* prism_createSurface(PrismInstance* instance, int width, int height);
void prism_destroySurface(PrismSurface* surface);

/*
    Get information about the surface. Most importantly the pixel buffer.
*/
void prism_surfaceInfo(PrismSurface* surface, PrismSurfaceInfo* info);

/*
    Tell Prism server to render the updated surface.
*/
bool prism_presentSurface(PrismSurface* surface);
