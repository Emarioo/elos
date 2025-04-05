/*
    Algorithm for detecting if a set of rectangles fully cover a rectangle.


*/
//-- CONFIGURATION
// #define PRINT
// #define STATS

// ALONE_FLAGS is not faster.
// #define ALONE_FLAGS


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#include <Windows.h>

typedef int i32;
typedef short i16;
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;

#ifdef PRINT
#define PRINTF(...) printf(__VA_ARGS__);
#else
#define PRINTF(...) ;
#endif

#define FN_ENTER PRINTF("%s\n",__func__);

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


#ifdef ALONE_FLAGS
#define GET_FLAG(S,I) S->flags[I]
#else
#define GET_FLAG(S,I) S->data[I].flags
#endif

typedef struct Rect Rect;
struct Rect
{
    i32 x, y, w, h;
    #ifndef ALONE_FLAGS
    u8 flags;
    #endif
};

typedef struct Surface Surface;
struct Surface
{
    Rect *data;
    #ifdef ALONE_FLAGS
    u8 *flags;
    #endif
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
    u32 corners;
    u32 centers;
    u32 slices;
    u32 halves;
    u32 split3;
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
        #ifdef ALONE_FLAGS
        surface.flags = malloc(sizeof(u8) * max);
        memset(surface.flags,0,sizeof(u8)*max);
        #endif
        memset(surface.data,0,sizeof(Rect)*max);
        surface.max = max;
    }
    return surface;
}
void surface_cleanup(Surface *surface)
{
    if (surface->data)
        free(surface->data);
    #ifdef ALONE_FLAGS
    if (surface->flags)
        free(surface->flags);
    #endif

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
        if (0 == (GET_FLAG(surface,i) & RECT_VALID))
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
    printf(" corners: %d\n", surface->corners);
    printf(" centers: %d\n", surface->centers);
    printf(" slices: %d\n", surface->slices);
    printf(" halves: %d\n", surface->halves);
    printf(" split3: %d\n", surface->split3);
    #endif
}

Rect create_rect(i32 x, i32 y, i32 w, i32 h)
{
    Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    #ifndef ALONE_FLAGS
    rect.flags = 0;
    #endif
    return rect;
}
void rect_print(Rect *r)
{
    PRINTF("{%d, %d, %d, %d}", r->x, r->y, r->w, r->h);
}

void visualize(Surface *surface, Surface *sub, const char *path)
{

    int bsize = 100 + surface->used * 40;
    if(sub)
        bsize += sub->used * 40;
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
    // printf("%s\n",varName+nameBegin);

    bytes += sprintf(buffer + bytes, "var %s=[[", varName + nameBegin);
    int surc = 0;
    for (int i = 0; i < surface->used; i++)
    {
        Rect *r = surface->data + i;
        // printf("%d: %d\n",i, (int)GET_FLAG(surface,i));
        if (0 == (GET_FLAG(surface,i) & RECT_VALID))
            continue;
        if(GET_FLAG(surface,i) != RECT_VALID){
            // This should not happen and there is a bug if it does.
            fprintf(stderr,"ERROR, surface->data[%d] flag had SKIP!\n",i);
        }
        surc++;
        bytes += sprintf(buffer + bytes, "%d,%d,%d,%d,\n", r->x, r->y, r->w, r->h);
    }
    bytes += sprintf(buffer + bytes, "],[");
    int subc = 0;
    if(sub) {
        for (int i = 0; i < sub->used; i++)
        {
            Rect *r = sub->data + i;
            // printf("%d: %d\n",i,(int)GET_FLAG(surface,i));
            subc++;
            bytes += sprintf(buffer + bytes, "%d,%d,%d,%d,", r->x, r->y, r->w, r->h);
        }
    }
    bytes += sprintf(buffer + bytes, "]]");

    // int err = GetLastError();
    // printf("err %d\n",err);

    // printf("bytes %d %d\n",bytes, bsize);
    // fprintf(stderr,"eaeea");
    // fprintf(stderr,"%p\n",path);
    // fprintf(stderr,"%s\n",path);
    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    // GetLastError()
    WriteFile(file,buffer,bytes,0,0);
    CloseHandle(file);

    // FILE *file = fopen(path, "w");
    // printf("cras\n");
    // fwrite(buffer, bytes, 1, file);
    // fclose(file);

    free(buffer);
    printf("Surf: %d Sub: %d\n",surc, subc);
}
// returns index where rect was placed
// returns -1 on failure
i32 surface_add(Surface *surface, Rect rect)
{
    FN_ENTER

    i32 index = -1;
    if (surface->used == surface->max)
    {
        for (int i = 0; i < surface->used; i++)
        {
            // #ifndef ALONE_FLAGS
            // Rect *r = surface->data + i;
            // #endif
            if ((GET_FLAG(surface,i) & RECT_VALID))
                continue;
            index = i;
            PRINTF("  reuse %d\n",i);
            break;
        }
        if (index == -1)
        {
            i32 max = (surface->max * 2 + 10);
            Rect *ptr = 0;
            #ifdef ALONE_FLAGS
            u8 *fptr = 0;
            #endif
            if (!surface->data)
            {
                ptr = malloc(max * sizeof(Rect));
                #ifdef ALONE_FLAGS
                fptr = malloc(max * sizeof(u8));
                #endif
            } else {
                ptr = realloc(surface->data, max * sizeof(Rect));
                #ifdef ALONE_FLAGS
                fptr = realloc(surface->flags, max * sizeof(u8));
                #endif
                #ifdef STATS
                surface->reallocations++;
                #endif
            }
            if(ptr)
                surface->data = ptr;
            #ifdef ALONE_FLAGS
            if(fptr)
                surface->flags = fptr;
            #endif

            if (!ptr
            #ifdef ALONE_FLAGS
            ||!fptr
            #endif
            )
            {
                PRINTF("  realloc failed\n");
                return -1;
            }
            #ifdef ALONE_FLAGS
            memset(surface->flags + surface->max,0,sizeof(u8)*(max-surface->max));
            #endif
            memset(surface->data + surface->max,0,sizeof(Rect)*(max-surface->max));
            PRINTF("  resize %d %d\n",surface->used,surface->max);

            surface->max = max;
        }
    }
    if (index == -1) {
        PRINTF("  use %d\n",surface->used);
        index = surface->used++;
    }

    #ifdef STATS
    surface->additions++;
    #endif
    surface->data[index] = rect;
    #ifdef ALONE_FLAGS
    surface->flags[index] = RECT_VALID;
    #else
    surface->data[index].flags = RECT_VALID;
    // rect.flags = RECT_VALID;
    #endif
    return index;
}
// rect should not exist in surface. Possibly odd behaviour.
bool surface_subtract_rect(Surface *surface, Rect *rect)
{
    FN_ENTER
    if(rect->w ==0 || rect->h == 0)
        return true;
    for (int ri = 0; ri < surface->used; ri++)
    {
        // TODO: r->flags is hit quite often so it could be worth having a seperate allocation
        //  for flags and one for rects. Fewer cache misses?
        Rect* r = surface->data + ri;
        // #define r (surface->data + ri)
        if (0 == (GET_FLAG(surface,ri) & RECT_VALID)) {
            #ifdef STATS
            surface->invalid_skips++;
            #endif
            continue;
        }
        if (GET_FLAG(surface,ri) & RECT_SKIP_ONCE)
        {
            #ifdef STATS
            surface->skips++;
            #endif
            // PRINTF("skip[%d]\n",ri);
            GET_FLAG(surface,ri) &= ~RECT_SKIP_ONCE;
            continue;
        }

        #ifdef STATS
        surface->comparisons++;
        #endif

        // #ifdef PRINT
        // PRINTF("cmp[%d]: ",ri);
        // rect_print(r);
        // PRINTF(" - ");
        // rect_print(rect);
        // PRINTF("\n");
        // #endif
        // NOTE: Be careful with <, >, <=, >=. It's easy to use the wrong one.

        if (r->x + r->w <= rect->x || r->x >= rect->x + rect->w || r->y + r->h <= rect->y || r->y >= rect->y + rect->h)
        {   
            #ifdef STATS
            surface->outside++;
            #endif
            // PRINTF("outside of rect\n");
            continue;
        }

        if (rect->x <= r->x && r->x + r->w <= rect->x + rect->w && 
            rect->y <= r->y && r->y + r->h <= rect->y + rect->h)
        {
            #ifdef STATS
            surface->inside++;
            #endif
            // PRINTF("fully in rect\n");
            GET_FLAG(surface,ri) &= ~RECT_VALID;
            continue;
        }
        GET_FLAG(surface,ri) &= ~RECT_VALID;

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

        // PRINTF("overlap t:%d l:%d b:%d r:%d\n", overlap_t, overlap_l, overlap_b, overlap_r);
#ifdef ALONE_FLAGS
#define POST_SURFACE_ADD if (index == -1) return false; if (index > ri) surface->flags[index] |= RECT_SKIP_ONCE;
#else
#define POST_SURFACE_ADD if (index == -1) return false; if (index > ri) surface->data[index].flags |= RECT_SKIP_ONCE;
#endif
#define ZERO_RECT create_rect(r->x, r->y, r->w, r->h)

        // #region
        switch (overlap_count) {
            case 4:{
                fprintf(stderr, "BUG overlap_count == 4!");
                return false;
            }
            case 3: 
        {
            int overlap_index = (non_overlap + 2) % 4;
            PRINTF("%s cut\n", overlap_strs[overlap_index]);
            
            #ifdef STATS
            surface->halves++;
            #endif

            Rect new_rect = ZERO_RECT;
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
            POST_SURFACE_ADD
            break;
        }
            case 2:
        {
            if (overlaps[0] == overlaps[2] && overlaps[1] == overlaps[3])
            {
                #ifdef STATS
                surface->slices++;
                #endif
                if (overlaps[1]) {
                    PRINTF("horizontal cut\n");
                }else{
                    PRINTF("vertical cut\n");
                }

                Rect r0 = ZERO_RECT;
                Rect r1 = ZERO_RECT;
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
                POST_SURFACE_ADD
                index = surface_add(surface, r1);
                POST_SURFACE_ADD
            }
            else
            {
                
                #ifdef STATS
                surface->corners++;
                #endif
                PRINTF("corner ");
                for (int j = 0; j < 4; j++)
                {
                    if (overlaps[j])
                        PRINTF("%s ", overlap_strs[j]);
                }
                PRINTF("\n");

                Rect r0 = ZERO_RECT;
                Rect r1 = ZERO_RECT;
                
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
                POST_SURFACE_ADD
                index = surface_add(surface, r1);
                POST_SURFACE_ADD
            }
            break;
        }
            case 1:
        {
            #ifdef STATS
            surface->split3++;
            #endif
            PRINTF("3-split on ");
            for (int j = 0; j < 4; j++)
            {
                if (overlaps[j])
                    PRINTF("%s ", overlap_strs[j]);
            }
            PRINTF("\n");
            Rect r0 = ZERO_RECT;
            Rect r1 = ZERO_RECT;
            Rect r2 = ZERO_RECT;
            
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
            POST_SURFACE_ADD
            index = surface_add(surface, r1);
            POST_SURFACE_ADD
            index = surface_add(surface, r2);
            POST_SURFACE_ADD
            break;
        }
            case 0:
        {
            
            #ifdef STATS
            surface->centers++;
            #endif
            PRINTF("\n");
            Rect r0 = ZERO_RECT;
            Rect r1 = ZERO_RECT;
            Rect r2 = ZERO_RECT;
            Rect r3 = ZERO_RECT;

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
            POST_SURFACE_ADD
            index = surface_add(surface, r1);
            POST_SURFACE_ADD
            index = surface_add(surface, r2);
            POST_SURFACE_ADD
            index = surface_add(surface, r3);
            POST_SURFACE_ADD
            break;
        }
        }
        // #endregion
        #undef r
    }
    return true;
}

bool surface_subtract(Surface *surface, Surface *rects)
{
    FN_ENTER
    for (int i = 0; i < rects->used; i++)
    {
        Rect *r = rects->data + i;
        if (0 == (GET_FLAG(rects,i) & RECT_VALID))
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
    #ifdef PRINT
    PRINTF("Surface: used: %d, max: %d\n", surface->used, surface->max);
    for (int i = 0; i < surface->used; i++)
    {
        Rect *r = surface->data + i;
        if ((GET_FLAG(surface,i) & RECT_VALID) == 0)
            continue;
        PRINTF(" %d: %d %d %d %d\n", i, r->x, r->y, r->w, r->h);
    }
    #endif
}
float random() {return (float)rand()/(float)RAND_MAX;}
int random_range(int a, int b) {return a + (b-a)*random();}

void test_random(int sw, int sh, int loops){
    srand(10);
    // srand((u32)time(0));

    Surface surface = surface_create(100);
    Surface sub = surface_create(loops);
    printf("Surface bytes: %llu\n", surface.max * sizeof(Rect));
    printf("Surface (sub) bytes: %llu\n", sub.max * sizeof(Rect));
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
        surface_add(&sub, create_rect(x,y,w,h));
    }
    const int iter = 10;
    double times[iter];
    double total = 0;
    for(int j=0;j<iter;j++){
        surface_clear(&surface);
        surface_add(&surface, create_rect(0, 0, sw, sh));

        u64 start = time_start();
        surface_subtract(&surface, &sub);
        double tim = time_end(start);
        times[j] = tim;
        total += tim;
        // printf("%s in %lf seconds\n",__func__,time_end(start));
    }
    double mid = 0;
    double avg = total / iter;
    
    for(int i=0;i<iter;i++){
        for (int j=0;j<iter-1;j++){
            if(times[j] > times[j+1]){
                double tmp = times[j];
                times[j] = times[j+1];
                times[j+1] = tmp;
            }
        }
    }
    avg /= iter;
    mid = times[iter/2];

    printf("mid %lf avg %lf total %lf seconds\n",mid, total/iter, total);

    visualize(&surface, &sub, "states/state0.js");
    // visualize(&surface, 0, "states/state0.js");

    surface_stats(&surface);

    surface_cleanup(&sub);
    surface_cleanup(&surface);
}
void test_custom(){
    Surface surface = surface_create(0);
    // surface_add(&surface, create_rect(0, 0, 10, 1, 0));
    surface_add(&surface, create_rect(0, 0, 40, 40));

#define RECT(...) surface_add(&sub, create_rect(__VA_ARGS__));
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
    // test_random(1920,1080,1000);
    test_random(10000, 10000, 100000);
    // test_random(200,200,100);
    // test_random(200,200,10);

    return 1;
}