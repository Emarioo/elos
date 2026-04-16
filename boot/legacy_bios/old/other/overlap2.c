#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#include <Windows.h>

typedef int i32;
typedef unsigned int u32;
typedef unsigned long long u64;

// #define PRINT
#define STATS
#ifdef PRINT
#define PRINTF(...) printf(__VA_ARGS__);
#else
#define PRINTF(...) ;
#endif

static u64 perf_frequency = 0;
u64 time_start(){
    if(perf_frequency == 0) {
        // TODO: Handle return value
        QueryPerformanceFrequency((LARGE_INTEGER*)&perf_frequency);
    }
    u64 counter = 0;
    // TODO: Handle return value
    QueryPerformanceCounter((LARGE_INTEGER*)&counter);
    return counter;
}
double time_end(u64 previous){
    u64 counter = 0;
    // TODO: Handle return value
    QueryPerformanceCounter((LARGE_INTEGER*)&counter);
    if(perf_frequency == 0)
        return (double)(counter-previous);
    return (double)(counter-previous) / (double)perf_frequency;
}

typedef struct Rect Rect;
struct Rect
{
    union
    {
        i32 xywh[4];
        struct
        {
            i32 x, y, w, h;
        };
    };
    i32 flags;
};

typedef struct Surface Surface;
struct Surface
{
    Rect *data;
    u32 used;
    u32 max;
    u32 iteration;

#ifdef STATS
    u32 inside;
    u32 outside;
    u32 comparisons;
    u32 additions;
    u32 skips;
    u32 invalid_skips;
    u32 overlap_checks;
    u32 reallocations;
#endif
};

#define RECT_SKIP_ONCE 0x2
#define RECT_VALID 0x1

Surface surface_create(u32 max)
{
    Surface surface;
    memset(&surface, 0, sizeof(Surface));
    if(max!=0) {
        surface.data = malloc(sizeof(Rect) * max);
        surface.max = max;
    }
    return surface;
}
void surface_cleanup(Surface *surface)
{
    if (surface->data)
        free(surface->data);

    memset(surface, 0, sizeof(Surface));
}
void surface_clear(Surface *surface)
{
    surface->used = 0;
}
int surface_area(Surface *surface)
{
    int area = 0;
    for (int i = 0; i < surface->used; i++)
    {
        Rect *r = surface->data + i;
        if (0 == (r->flags & RECT_VALID))
            continue;

        area += r->w * r->h;
    }
    return area;
}
bool surface_empty(Surface* surface) {
    return surface->used == 0;
}
void surface_stats(Surface* surface){
    #ifdef STATS
    printf("Surface stats:\n");
    printf(" Inside: %d\n",surface->inside);
    printf(" Outside: %d\n",surface->outside);
    printf(" Comparisions: %d\n",surface->comparisons);
    printf(" Overlap checks: %d\n",surface->overlap_checks);
    printf(" Additions: %d\n",surface->additions);
    printf(" Skips: %d\n",surface->skips);
    printf(" Invalid skips: %d\n",surface->invalid_skips);
    printf(" Reallocations: %d\n", surface->reallocations);
    printf(" Area: %d\n",surface_area(surface));
    #endif
}

Rect create_rect(i32 x, i32 y, i32 w, i32 h, i32 flags)
{
    Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    rect.flags = flags;
    return rect;
}
void rect_print(Rect *r)
{
    PRINTF("{%d, %d, %d, %d}", r->x, r->y, r->w, r->h);
}

void visualize(Surface *surface, Surface *sub, const char *path)
{
    FILE *file = fopen(path, "w");

    int bsize = 100 + surface->used * 30;
    if(sub)
        bsize += sub->used * 30;
    char* buffer = malloc(bsize);
    int bytes = 0;
    char varName[100];
    strcpy(varName, path);
    int nameBegin = 0;
    int length = strlen(path);
    for (int i = length - 1; i >= 0; i--)
    {
        char chr = path[i];
        if (chr == '/')
        {
            nameBegin = i + 1;
            // PRINTF("%s\n",varName+nameBegin);
            break;
        }
        if (chr == '.')
        {
            varName[i] = 0;
        }
    }
    printf("%s\n",varName+nameBegin);

    bytes += sprintf(buffer + bytes, "var %s=[[", varName + nameBegin);
    int surc = 0;
    for (int i = 0; i < surface->used; i++)
    {
        Rect *r = surface->data + i;
        if (0 == (r->flags & RECT_VALID))
            continue;
        surc++;
        bytes += sprintf(buffer + bytes, "%d,%d,%d,%d,\n", r->x, r->y, r->w, r->h);
    }
    bytes += sprintf(buffer + bytes, "],[");
    int subc = 0;
    if(sub) {
        for (int i = 0; i < sub->used; i++)
        {
            Rect *r = sub->data + i;
            // if(0 == (r->flags & RECT_VALID))
            //     continue;
            subc++;
            bytes += sprintf(buffer + bytes, "%d,%d,%d,%d,", r->x, r->y, r->w, r->h);
        }
    }
    bytes += sprintf(buffer + bytes, "]]");

    fwrite(buffer, bytes, 1, file);
    fclose(file);

    free(buffer);
    printf("Surf: %d Sub: %d\n",surc, subc);
}
// returns index where rect was placed
// returns -1 on failure
i32 surface_add(Surface *surface, Rect rect)
{
    i32 index = -1;
    if (surface->used == surface->max)
    {
        for (int i = 0; i < surface->used; i++)
        {
            Rect *r = surface->data + i;
            if ((r->flags & RECT_VALID))
                continue;
            index = i;
            PRINTF("reuse %d\n",i);
            break;
        }
        if (index == -1)
        {
            i32 max = (surface->max * 2 + 10);
            Rect *ptr = 0;
            if (!surface->data)
            {
                ptr = malloc(max * sizeof(Rect));
            } else {
                ptr = realloc(surface->data, max * sizeof(Rect));
                #ifdef STATS
                surface->reallocations++;
                #endif
            }
            if (!ptr)
            {
                PRINTF("realloc failed\n");
                return -1;
            }
            PRINTF("resize %d %d\n",surface->used,surface->max);

            surface->data = ptr;
            surface->max = max;
        }
    }
    if (index == -1) {
        PRINTF("use %d\n",surface->used);
        index = surface->used++;
    }

    #ifdef STATS
    surface->additions++;
    #endif
    surface->data[index] = rect;
    surface->data[index].flags = RECT_VALID;
    return index;
}
// rect should not exist in surface. Possibly odd behaviour.
bool surface_subtract_rect(Surface *surface, Rect *rect)
{
    PRINTF("%s\n", __func__);
    if(rect->w ==0 || rect->h == 0)
        return true;
    for (int ri = 0; ri < surface->used; ri++)
    {
        // TODO: r->flags is hit quite often so it could be worth having a seperate allocation
        //  for flags and one for rects. Fewer cache misses?
        Rect *r = surface->data + ri;
        if (0 == (r->flags & RECT_VALID)) {
            #ifdef STATS
            surface->invalid_skips++;
            #endif
            continue;
        }
        if (r->flags & RECT_SKIP_ONCE)
        {
            #ifdef STATS
            surface->skips++;
            #endif
            PRINTF("skip[%d]\n",ri);
            r->flags &= ~RECT_SKIP_ONCE;
            continue;
        }

        #ifdef STATS
        surface->comparisons++;
        #endif

        #ifdef PRINT
        PRINTF("cmp[%d]: ",ri);
        rect_print(r);
        PRINTF(" - ");
        rect_print(rect);
        PRINTF("\n");
        #endif
        // NOTE: Be careful with <, >, <=, >=. It's easy to use the wrong one.

        if (r->x + r->w <= rect->x || r->x >= rect->x + rect->w || r->y + r->h <= rect->y || r->y >= rect->y + rect->h)
        {   
            
            #ifdef STATS
            surface->outside++;
            #endif
            PRINTF("outside of rect\n");
            // r is outside of rect
            continue;
        }

        if (rect->x <= r->x && r->x + r->w <= rect->x + rect->w && 
            rect->y <= r->y && r->y + r->h <= rect->y + rect->h)
        {
            
            #ifdef STATS
            surface->inside++;
            #endif
            // r is fully inside rect
            PRINTF("fully in rect\n");
            r->flags &= ~RECT_VALID;
            continue;
        }

        #ifdef STATS
        surface->overlap_checks++;
        #endif

        // TODO: You may be able to optimise branch prediction here.
        // TODO: Reuse r instead of adding a new edge.  
        bool general_x = rect->x + rect->w >= r->x && r->x + r->w >= rect->x;
        bool general_y = rect->y + rect->h >= r->y && r->y + r->h >= rect->y;
        bool overlap_t = (rect->y <= r->y && r->y < rect->y + rect->h && general_x);
        bool overlap_l = (rect->x <= r->x && r->x < rect->x + rect->w && general_y);
        bool overlap_b = (rect->y < r->y + r->h && r->y + r->h <= rect->y + rect->h && general_x);
        bool overlap_r = (rect->x < r->x + r->w && r->x + r->w <= rect->x + rect->w && general_y);

        bool overlaps[4] = {overlap_t, overlap_l, overlap_b, overlap_r};
        #ifdef PRINT
        const char *overlap_strs[4] = {"top", "left", "bottom", "right"};
        #endif
        int overlap_count = 0;
        int non_overlap = -1;
        for (int j = 0; j < 4; j++)
        {
            if (overlaps[j])
                overlap_count++;
            else
                non_overlap = j;
        }

        PRINTF("overlap t:%d l:%d b:%d r:%d\n", overlap_t, overlap_l, overlap_b, overlap_r);
        
        r->flags &= ~RECT_VALID;

        // #region
        switch (overlap_count) {
            case 3: 
        {
            int overlap_index = (non_overlap + 2) % 4;
            PRINTF("%s cut\n", overlap_strs[overlap_index]);

            Rect new_rect = create_rect(r->x, r->y, r->w, r->h, 0);
            if (overlap_index == 1)
            {
                new_rect.x = rect->x + rect->w;
                new_rect.w = (r->x + r->w) - (rect->x + rect->w);
            }
            else if (overlap_index == 0)
            {
                new_rect.y = rect->y + rect->h;
                new_rect.h = (r->y + r->h) - (rect->y + rect->h);
            }
            else if (overlap_index == 3)
            {
                new_rect.w = (rect->x) - (r->x);
            }
            else /* if(overlap_index == 2) */
            {
                new_rect.h = (rect->y) - (r->y);
            }

            i32 index = surface_add(surface, new_rect);
            if (index == -1)
                return false;
            if (index > ri)
            {
                (surface->data + index)->flags |= RECT_SKIP_ONCE;
            }
            break;
        }
            case 2:
        {
            if (overlaps[0] == overlaps[2] && overlaps[1] == overlaps[3])
            {
                if (overlaps[1]) {
                    PRINTF("horizontal cut\n");
                }else{
                    PRINTF("vertical cut\n");
                }

                Rect r0 = create_rect(r->x, r->y, r->w, r->h, 0);
                Rect r1 = create_rect(r->x, r->y, r->w, r->h, 0);
                if (overlaps[1])
                {
                    r0.h = rect->y - r->y;
                    r1.y = rect->y + rect->h;
                    r1.h = (r->y + r->h) - (rect->y + rect->h);
                }
                else /*if(overlaps[0])*/
                {
                    r0.w = rect->x - r->x;
                    r1.x = rect->x + rect->w;
                    r1.w = (r->x + r->w) - (rect->x + rect->w);
                    // PRINTF("r0: ");rect_print(&r0);PRINTF("\n");
                    // PRINTF("r1: ");rect_print(&r1);PRINTF("\n");
                }

                i32 index = surface_add(surface, r0);
                if (index == -1)
                    return false;
                if (index > ri)
                {
                    (surface->data + index)->flags |= RECT_SKIP_ONCE;
                }
                index = surface_add(surface, r1);
                if (index == -1)
                    return false;
                if (index > ri)
                {
                    (surface->data + index)->flags |= RECT_SKIP_ONCE;
                }
            }
            else
            {
                PRINTF("corner ");
                for (int j = 0; j < 4; j++)
                {
                    if (overlaps[j])
                        PRINTF("%s ", overlap_strs[j]);
                }
                PRINTF("\n");

                Rect r0 = create_rect(r->x, r->y, r->w, r->h, 0);
                Rect r1 = create_rect(r->x, r->y, r->w, r->h, 0);
                
                if(overlaps[0]){
                    r1.y = (rect->y + rect->h);
                }
                if(overlaps[1]){
                    r0.x = rect->x + rect->w;
                    r0.w = (r->x + r->w) - (rect->x + rect->w);
                    r1.w = (rect->x + rect->w) - r->x;
                } else {
                    r0.w = (rect->x) - (r->x);
                    r1.w = (r->x + r->w) - (rect->x);
                }
                if(overlaps[2]){
                    r1.h = (rect->y) - (r->y);
                }else{
                    r1.h = (r->y + r->h) - (rect->y + rect->h);
                }
                if(overlaps[3]){
                    r1.x = (rect->x);
                }

                i32 index = surface_add(surface, r0);
                if (index == -1)
                    return false;
                if (index > ri)
                {
                    (surface->data + index)->flags |= RECT_SKIP_ONCE;
                }
                index = surface_add(surface, r1);
                if (index == -1)
                    return false;
                if (index > ri)
                {
                    (surface->data + index)->flags |= RECT_SKIP_ONCE;
                }
            }
            break;
        }
            case 1:
        {
            PRINTF("3-split on ");
            for (int j = 0; j < 4; j++)
            {
                if (overlaps[j])
                    PRINTF("%s ", overlap_strs[j]);
            }
            PRINTF("\n");
            Rect r0 = create_rect(r->x, r->y, r->w, r->h, 0);
            Rect r1 = create_rect(r->x, r->y, r->w, r->h, 0);
            Rect r2 = create_rect(r->x, r->y, r->w, r->h, 0);
            
            // NOTE: You could extract simularities when setting r0, r1, r2 like I have previosusly
            //  But I have a suspicion that it's not going to improve much.

            if (overlaps[0])
            {
                r0.x = rect->x;
                r0.w = rect->w;
                r0.y = (rect->y + rect->h);
                r0.h = (r->y + r->h) - (rect->y + rect->h);

                r1.w = rect->x - r->x;

                r2.x = rect->x + rect->w;
                r2.w = (r->x + r->w) - (rect->x + rect->w);
            }
            else if (overlaps[1])
            {
                r0.x = rect->x + rect->w;
                r0.w = (r->x + r->w) - (rect->x + rect->w);
                r0.y = (rect->y);
                r0.h = rect->h;

                r1.h = rect->y - r->y;

                r2.y = rect->y + rect->h;
                r2.h = (r->y + r->h) - (rect->y + rect->h);
            }
            else if (overlaps[2])
            {
                r0.x = rect->x;
                r0.w = rect->w;
                r0.h = rect->y - r->y;

                r1.w = rect->x - r->x;

                r2.x = rect->x + rect->w;
                r2.w = (r->x + r->w) - (rect->x + rect->w);
            }
            else if (overlaps[3])
            {
                r0.w = rect->x - r->x;
                r0.y = rect->y;
                r0.h = rect->h;

                r1.h = rect->y - r->y;

                r2.y = rect->y + rect->h;
                r2.h = (r->y + r->h) - (rect->y + rect->h);
            }

            i32 index = surface_add(surface, r0);
            if (index == -1)
                return false;
            if (index > ri)
            {
                (surface->data + index)->flags |= RECT_SKIP_ONCE;
            }
            index = surface_add(surface, r1);
            if (index == -1)
                return false;
            if (index > ri)
            {
                (surface->data + index)->flags |= RECT_SKIP_ONCE;
            }
            index = surface_add(surface, r2);
            if (index == -1)
                return false;
            if (index > ri)
            {
                (surface->data + index)->flags |= RECT_SKIP_ONCE;
            }
            break;
        }
            case 0:
        {
            PRINTF("\n");
            Rect r0 = create_rect(r->x, r->y, r->w, r->h, 0);
            Rect r1 = create_rect(r->x, r->y, r->w, r->h, 0);
            Rect r2 = create_rect(r->x, r->y, r->w, r->h, 0);
            Rect r3 = create_rect(r->x, r->y, r->w, r->h, 0);

            r0.x = rect->x;
            r0.w = rect->w;
            r0.h = rect->y - r->y;

            r1.w = rect->x - r->x;

            r2.x = rect->x + rect->w;
            r2.w = (r->x + r->w) - (rect->x + rect->w);

            r3.x = rect->x;
            r3.w = rect->w;
            r3.y = (rect->y + rect->h);
            r3.h = (r->y + r->h) - (rect->y + rect->h);

            i32 index = surface_add(surface, r0);
            if (index == -1)
                return false;
            if (index > ri)
            {
                (surface->data + index)->flags |= RECT_SKIP_ONCE;
            }
            index = surface_add(surface, r1);
            if (index == -1)
                return false;
            if (index > ri)
            {
                (surface->data + index)->flags |= RECT_SKIP_ONCE;
            }
            index = surface_add(surface, r2);
            if (index == -1)
                return false;
            if (index > ri)
            {
                (surface->data + index)->flags |= RECT_SKIP_ONCE;
            }
            index = surface_add(surface, r3);
            if (index == -1)
                return false;
            if (index > ri)
            {
                (surface->data + index)->flags |= RECT_SKIP_ONCE;
            }
            break;
        }
        }
        // #endregion
    }
    return true;
}

bool surface_subtract(Surface *surface, Surface *rects)
{
    PRINTF("%s\n", __func__);
    for (int i = 0; i < rects->used; i++)
    {
        Rect *r = rects->data + i;
        if (0 == (r->flags & RECT_VALID))
        {
            continue;
        }
        bool yes = surface_subtract_rect(surface, r);
        if (!yes)
            return false;
    }
    // char path[100];
    // sprintf(path, "states/state%d.js", surface->iteration++);
    // visualize(surface, rects, path);
    return true;
}
void surface_print(Surface *surface)
{
    PRINTF("Surface: used: %d, max: %d\n", surface->used, surface->max);
    for (int i = 0; i < surface->used; i++)
    {
        Rect *r = surface->data + i;
        if ((r->flags & RECT_VALID) == 0)
            continue;
        PRINTF(" %d: %d %d %d %d\n", i, r->x, r->y, r->w, r->h);
    }
}
float random() {return (float)rand()/(float)RAND_MAX;}
int random_range(int a, int b) {return a + (b-a)*random();}

void test_random(int sw, int sh, int loops){
    srand(10);
    // srand((u32)time(0));

    Surface surface = surface_create(100);
    surface_add(&surface, create_rect(0, 0, sw, sh, 0));
    Surface sub = surface_create(100);
    // Rect* windows = malloc(loops * sizeof(Rect));
    for(int i=0;i<loops;i++){
        // int x = random_range(-2,sw+2);
        // int y = random_range(-2,sh+2);
        // int w = random_range(0,sw/2+2);
        // int h = random_range(0,sh/2+2);
        
        int x = random_range(0,sw);
        int y = random_range(0,sh);
        int w = random_range(1,sw-x);
        int h = random_range(1,sh-y);

        if(w==0||h==0) {
            i--; // redo
            continue;
        }
        surface_add(&sub, create_rect(x,y,w,h, 0));
        // *(windows + i) = create_rect(x,y,w,h, 0);
    }

    u64 start = time_start();
    surface_subtract(&surface, &sub);
    // for(int i=0;i<loops;i++){
    //     surface_subtract_rect(&surface, windows + i);
    // }
    printf("%s in %lf seconds\n",__func__,time_end(start));

    visualize(&surface, &sub, "states/state0.js");
    // visualize(&surface, 0, "states/state0.js");

    surface_stats(&surface);

    surface_cleanup(&sub);
    surface_cleanup(&surface);
}
void test_custom(){
    Surface surface = surface_create(0);
    // surface_add(&surface, create_rect(0, 0, 10, 1, 0));
    surface_add(&surface, create_rect(0, 0, 40, 40, 0));

#define RECT(...) surface_add(&sub, create_rect(__VA_ARGS__, 0));
#define SUB(...)                          \
    { \
        RECT(__VA_ARGS__);                \
    }
        // surface_subtract(&surface, &sub); 
        // surface_clear(&sub); 

    Surface sub = surface_create(20);

    // SUB(-1, -1, 5, 5)
    // SUB(-3 , -1, 3, 3)
    // surface_subtract(&surface, &sub);
    // SUB(4,4,10,4)

    // SUB(4,-2,3,15);
    // SUB(0,0,5,5);

    SUB(-1,-1,5,5);
    SUB(4,0,7,5);
    SUB(7,7,15,15);
    // SUB(9,9,15,15);
    // SUB(0,5,3,5);
    // SUB(4,4,10,4);
    // SUB(0,0,5,5);
    // SUB(1,1,5,5);

    // TODO: Performance test with QueryPerformanceCounter and QueryPerformanceFrequency (Windows)

    surface_subtract(&surface, &sub);

    visualize(&surface, &sub, "states/state0.js");
    // surface_print(&surface);

    surface_cleanup(&surface);
    surface_cleanup(&sub);
}
int main(int argc, char **argv)
{
    // test_custom();
    test_random(1920,1080,1000);
    // test_random(200,200,100);
    // test_random(200,200,10);

    return 1;
}