#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "stdbool.h"

typedef int i32;
typedef unsigned int u32;

typedef struct Rect Rect;
struct Rect {
	i32 x, y, w, h;
    i32 flags;
};

typedef struct Rects Rects;
struct Rects {
    Rect* data;
    u32 used;
    u32 max;
};
#define RECT_VALID 0x1
#define RECT_SKIP_ONCE 0x2

typedef struct Surface Surface;
struct Surface {
	Rects rects;
};

Surface surface_create(){
    Surface surface;
    memset(&surface, 0, sizeof(Surface));
    return surface;
}
void surface_cleanup(Surface* surface){
    if(surface->rects.data)
        free(surface->rects.data);

    memset(surface, 0, sizeof(Surface));
}

Rect create_rect(i32 x, i32 y, i32 w, i32 h, i32 flags) {
    Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    rect.flags = flags;
    return rect;
}
Rects rects_create(){
    Rects rects;
    memset(&rects, 0, sizeof(Rects));
    return rects;
}
void rect_print(Rect* r){
    printf("{%d, %d, %d, %d}",r->x, r->y, r->w, r->h);
}
// returns index where rect was placed
// returns -1 on failure
i32 rects_add(Rects* rects, Rect rect) {
    i32 index = -1;
    if (rects->used == rects->max) {
        for (int i = 0; i < rects->used; i++) {
            Rect* r = rects->data + i;
            if(0 == (r->flags & RECT_VALID))
                continue;
            index = i;
        }
        if(index == -1) {
            i32 max = (rects->max * 2 + 10);
            Rect* ptr = 0;
            if(!rects->data) {
                ptr = malloc(max * sizeof(Rect));
            } else {
                ptr = realloc(rects->data, max * sizeof(Rect));
            }
            if(!ptr) {
                printf("realloc failed\n");
                return -1;
            }
            
            rects->data = ptr;
            rects->max = max;
        }
    }
    if(index == -1)
        index = rects->used++;
        
    rects->data[index] = rect;
    rects->data[index].flags = RECT_VALID;
    return index;
}
// rect should not exist in surface. Possibly odd behaviour.
bool surface_subtract_rect(Surface* surface, Rect* rect) {
    for (int i = 0; i < surface->rects.used; i++) {
        Rect* r = surface->rects.data + i;
        if(r->flags & RECT_SKIP_ONCE) {
            r->flags &= ~RECT_SKIP_ONCE;
            continue;
        }
        if(0 == (r->flags & RECT_VALID))
            continue;

        printf("cmp: ");
        rect_print(r);
        printf(" - ");
        rect_print(rect);
        printf("\n");


        // TODO: Check off by one errors!

        if(r->x + r->w < rect->x
            || r->x > rect->x + rect->w
            || r->y + r->h < rect->y
            || r->y > rect->y + rect->h
        ) {
            printf("%d outside of rect\n", i);
            // r is outside of rect
            continue;
        }

        if (rect->x < r->x
            && r->x + r->w < rect->x + rect->w
            && rect->y < r->y
            && r->y + r->h < rect->y + rect->h
        ) {
            // r is fully inside rect
            printf("%d fully in rect\n", i);
            r->flags &= ~RECT_VALID;
            continue;
        }

        // TODO: You may be able to optimise branch prediction here.
        // TODO: Reuse r instead of adding a new edge.
        
        bool overlap_t = (rect->y < r->y && r->y < rect->y + rect->h);
        bool overlap_l = (rect->x < r->x && r->x < rect->x + rect->w);
        bool overlap_b = (rect->y < r->y + r->h && r->y + r->h < rect->y + rect->h);
        bool overlap_r = (rect->x < r->x + r->w && r->x + r->w < rect->x + rect->w);

        printf("overlap t:%d l:%d b:%d r:%d\n", overlap_t, overlap_l, overlap_b, overlap_r);

        if(overlap_t && overlap_l && overlap_r) {
            printf("top cut\n");
            // top edge is fully covered
            i32 index = rects_add(&surface->rects, create_rect(r->x, rect->y + rect->h, r->w, (r->y + r->h) - (rect->y + rect->h), 0));
            if(index == -1)
                return false;
            if (index > i) {
                (surface->rects.data + index)->flags |= RECT_SKIP_ONCE;
            }
        } else if(overlap_l && overlap_t && overlap_b) {
            printf("left cut\n");
            // left edge is fully covered
            i32 index = rects_add(&surface->rects, create_rect(rect->x + rect->w, r->y, (r->x + r->w) - (rect->x + rect->w), r->h, 0));
            if(index == -1)
                return false;
            if (index > i) {
                (surface->rects.data + index)->flags |= RECT_SKIP_ONCE;
            }
        } else if(overlap_b && overlap_l && overlap_r) {
            printf("bottom cut\n");
            // bottom edge is fully covered
            i32 index = rects_add(&surface->rects, create_rect(r->x, r->y, r->w, (rect->y + rect->h) - (r->y + r->h), 0));
            if(index == -1)
                return false;
            if (index > i) {
                (surface->rects.data + index)->flags |= RECT_SKIP_ONCE;
            }
        } else if(overlap_r && overlap_t && overlap_b) {
            printf("right cut\n");
            // right edge is fully covered
            i32 index = rects_add(&surface->rects, create_rect(r->x, r->y, (rect->x + rect->w) - (r->x + r->w), r->h, 0));
            if(index == -1)
                return false;
            if (index > i) {
                (surface->rects.data + index)->flags |= RECT_SKIP_ONCE;
            }
        } else if(overlap_l && overlap_r) {
            printf("horizontal cut\n");
            // horizontal split =
            i32 index = rects_add(&surface->rects, create_rect(r->x, r->y, r->w, rect->y - r->y, 0));
            if(index == -1)
                return false;
            if (index > i) {
                (surface->rects.data + index)->flags |= RECT_SKIP_ONCE;
            }
            index = rects_add(&surface->rects, create_rect(r->x, rect->y + rect->h, r->w, (r->y + r->h) - (rect->y + rect->h), 0));
            if(index == -1)
                return false;
            if (index > i) {
                (surface->rects.data + index)->flags |= RECT_SKIP_ONCE;
            }
        } else if(overlap_t && overlap_b) {
            printf("vertical cut\n");
            // vertical split ||
            i32 index = rects_add(&surface->rects, create_rect(r->x, r->y, rect->x - r->x, r->h, 0));
            if(index == -1)
                return false;
            if (index > i) {
                (surface->rects.data + index)->flags |= RECT_SKIP_ONCE;
            }
            index = rects_add(&surface->rects, create_rect(rect->x + rect->w, r->y, (r->x + r->w) - (rect->x + rect->w), r->h, 0));
            if(index == -1)
                return false;
            if (index > i) {
                (surface->rects.data + index)->flags |= RECT_SKIP_ONCE;
            }
        } else {
            // TODO: corners
            continue;
        }

        r->flags &= ~RECT_VALID;
    }
    return true;
}

bool surface_subtract(Surface* surface, Rects* rects) {
    for(int i=0;i<rects->used;i++){
        Rect* r = rects->data + i;
        if (0 == (r->flags & RECT_VALID)) {
            continue;
        }
        bool yes = surface_subtract_rect(surface, r);
        if(!yes)
            return false;
    }
    return true;
}
int surface_area(Surface* surface) {
    int area = 0;
    for (int i = 0; i < surface->rects.used; i++) {
        Rect* r = surface->rects.data + i;
        if(0 == (r->flags & RECT_VALID))
            continue;

        area += r->w * r->h;
    }
    return area;
}
void surface_print(Surface* surface){
    printf("Surface: used: %d, max: %d\n",surface->rects.used, surface->rects.max);
    for(int i=0;i<surface->rects.used;i++){
        Rect* r = surface->rects.data + i;
        if((r->flags & RECT_VALID) == 0)
            continue;
        printf(" %d: %d %d %d %d\n",i,r->x, r->y, r->w, r->h);
    }
}

int main(int argc, char** argv) {
    printf("Start\n");
    Surface surface = surface_create();
#define RECT(...) rects_add(&rects, create_rect(__VA_ARGS__, 0));
    
    rects_add(&surface.rects, create_rect(0,0,10,10,0));

    surface_print(&surface);

    Rects rects = rects_create();
    // RECT(0,0,5,5);
    RECT(0,-1,40,4);
    // RECT(0,0,5,40);
    // RECT(0,0,100,100);

    surface_subtract(&surface, &rects);
    surface_print(&surface);
    
    return 1;
}