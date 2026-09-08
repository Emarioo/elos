#pragma once


#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elos/syscalls.h"
#include "elos/common/intrinsics.h"
#include "elos/common/string.h"

#include "prism/prism.h"
#include "supper/HandmadeMath.h"



typedef struct {
    HMM_Vec3 points[3];
} Triangle3D;


typedef struct {
    HMM_Vec3 pos;
    HMM_Vec3 rot;

    HMM_Mat4 rotationMatrix;

    HMM_Mat4 perspectiveMatrix;
    HMM_Mat4 viewMatrix;
    HMM_Mat4 viewProjectionMatrix;
} Camera;

typedef struct {
    Triangle3D* triangles;
    int         triangles_len;
    int         triangles_max;
} Model;


typedef struct {
    HMM_Vec3 pos;
    HMM_Quat rot;
    Model*   model;
} Entity;

/*
    One session per process.
*/
typedef struct {
    int x;
} SupperSession;


void game_loop();

bool get_event(ELOS_UserEvent* event);



extern PrismInstance*        g_instance;
extern PrismSurface*         g_surface;
extern PrismSurfaceInfo      g_surfaceInfo;
extern u64                   g_ticks_per_second;
extern ELOS_UserEventBuffer* g_userEvents;
extern SupperSession         g_supperSession;
