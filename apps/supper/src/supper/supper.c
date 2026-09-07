
#include "supper/supper.h"

#include "prism/prism.h"
#include "stdui.h"


bool supper_load_font();

Model* create_model();

void render_entity(Entity* entity);

void render_triangle(Triangle3D* triangle);

Model* g_model;
Camera g_camera;


typedef struct {
    int x, y;
} ivec2;

ivec2 positions[3] = {
    // { 10, 50},
    // { 190, 100 },
    // { 60, 170 },

    { 130, 100},
    { 190, 80 },
    { 60, 170 },
};
int selectedPoint = 0;

void game_loop() {
    SupperSession* session = &g_supperSession;


    bool res = supper_load_font();
    if (!res) {
        exit(1);
    }

    g_model = create_model();

    g_camera.pos = (HMM_Vec3){ { 0, 0, -5 } };
    g_camera.rot = (HMM_Vec3){ { 0, 0, 0 } };
    
    while (1) {
        ELOS_UserEvent event;
        while (1) {
            bool has = get_event(&event);
            if (!has) {
                break;
            }
            if (event.type != ELOS_USER_EVENT_KEY || event.key.value == 0) {
                continue;
            }
            // printf("scan=0x%x code=%d chr=%c pressed=%d\n", event.key.scancode, event.key.keycode, (char)event.key.character, event.key.value);


            const ELOS_UserEvent_Key key = event.key;
            
            if (key.value > 0) {
                if (key.keycode == ELOSKEY_1) {
                    selectedPoint = 0;
                } else if (key.keycode == ELOSKEY_2) {
                    selectedPoint = 1;
                } else if (key.keycode == ELOSKEY_3) {
                    selectedPoint = 2;
                }
                
                int step = 10;
                if (key.keycode == ELOSKEY_LEFT_ARROW) {
                    positions[selectedPoint].x -= step;
                } else if (key.keycode == ELOSKEY_RIGHT_ARROW) {
                    positions[selectedPoint].x += step;
                } else if (key.keycode == ELOSKEY_UP_ARROW) {
                    positions[selectedPoint].y -= step;
                } else if (key.keycode == ELOSKEY_DOWN_ARROW) {
                    positions[selectedPoint].y += step;
                }

            }


        }

        draw_rect(0, 0, g_surfaceInfo.width, g_surfaceInfo.height, 0xFF002000);

        cstring text = PTR_CSTR("1/2/3 and arrow keys");
        draw_glyphs_from_text_bcolor(10, 10, 30, text, g_default_font, WHITE, 0);


        draw_triangle(positions[0].x, positions[0].y, positions[1].x, positions[1].y, positions[2].x, positions[2].y, 0xFF777777);
        
        draw_line(positions[0].x, positions[0].y, positions[1].x, positions[1].y, 1, WHITE);
        draw_line(positions[1].x, positions[1].y, positions[2].x, positions[2].y, 1, WHITE);
        draw_line(positions[2].x, positions[2].y, positions[0].x, positions[0].y, 1, WHITE);


        // update_game();
        // render_game();

        prism_presentSurface(g_surface);
        sleep((1000/60)*1000000);
    }

}



void update_game() {
    SupperSession* session = &g_supperSession;



}


void render_game() {
    SupperSession* session = &g_supperSession;

    g_camera.perspectiveMatrix = HMM_Perspective_RH_NO(90.0f, (float)g_surfaceInfo.width / (float)g_surfaceInfo.height, 0.01f, 400.f);


    Entity entity = {
        {{ 0, 0, 0 }},
        g_model,
    };

    render_entity(&entity);
}

void render_entity(Entity* entity) {
    SupperSession* session = &g_supperSession;

    Model* model = entity->model;

    for (int i=0;i<model->triangles_len;i++) {
        Triangle3D* triangle = &model->triangles[i];
        render_triangle(triangle);

    }

}

void render_triangle(Triangle3D* triangle) {
    SupperSession* session = &g_supperSession;

    // Projection math
    Triangle2D tri2 = { 0 };

    // @TODO Optimize, too expensive per triangle.

    for (int i=0;i<3;i++) {
        HMM_Vec3 point = triangle->points[i];
        HMM_Sub(point, g_camera.pos);

        HMM_Mat4 rot = HMM_Rotate_RH(g_camera.rot.Y, (HMM_Vec3){{0,1,0}});
        rot = HMM_MulM4(rot, HMM_Rotate_RH(g_camera.rot.X, (HMM_Vec3){{1,0,0}}));
        rot = HMM_MulM4(rot, HMM_Rotate_RH(g_camera.rot.Z, (HMM_Vec3){{0,0,1}}));
        
        rot = HMM_InvRotate(rot);

        HMM_Mat4 pointMatrix = HMM_Translate(point);
        pointMatrix = HMM_MulM4(pointMatrix, rot);

        pointMatrix = HMM_MulM4(pointMatrix, g_camera.perspectiveMatrix);

        tri2.points[i].X = g_surfaceInfo.width * (1 + pointMatrix.Columns[3].X) / 2;
        tri2.points[i].Y = g_surfaceInfo.height * (1 - pointMatrix.Columns[3].Y) / 2;
    }

    // Rasterize triangle

    draw_triangle(
        tri2.points[0].X, tri2.points[0].Y,
        tri2.points[1].X, tri2.points[1].Y,
        tri2.points[2].X, tri2.points[2].Y,
        0xFF777777
    );

}


Model* create_model() {
    Model* model = malloc(sizeof(Model));
    memset(model, 0, sizeof(*model));
    model->triangles_max = 1000;
    model->triangles = malloc(sizeof(*model->triangles) * model->triangles_max);
    model->triangles_len = 0;

    return model;
}




static void* font_allocator(Allocator* allocator, u64 size, void* old_ptr) {
    return realloc(old_ptr, size);
}

bool supper_load_font() {
    const char* path = "/pkg/slate/STDFONT.PSF";
    FILE* handle = fopen(path, "rb");
    if (!handle) {
        printf("Couldn't open %s\n", path);
        return false;
    }

    fseek(handle, 0, SEEK_END);
    int fileSize = ftell(handle);
    fseek(handle, 0, SEEK_SET);

    u8* data = malloc(fileSize);
    if (!data) {
        printf("Couldn't allocate %d for %s\n", fileSize, path);
        return false;
    }

    int readBytes = fread(data, 1, fileSize, handle);
    if (readBytes != fileSize) {
        printf("Could not read font %s, (read %d bytes, texture is %d bytes)\n", path, readBytes, fileSize);
        return false;
    }

    Allocator allocator = { font_allocator };
    bool res = font__load_from_bytes(data, fileSize, &g_default_font, &allocator);
    return res;
}

