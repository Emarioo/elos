
#include "supper/supper.h"

#include "prism/prism.h"
#include "stdui.h"
#include "stdnet.h"

#include "math.h"

#include "elos/elos.h"
#include "async_io.h"


#include "supper/supper_net.h"

bool supper_load_font();

void init_network();
void network_send_updated_position();
void network_handle_messages();
Model* create_model();

void render_entity(Entity* entity);

void render_triangle(Triangle3D* triangle);
void update_camera_matrix();
void update_game();
void render_game();

Camera g_camera;
Model* g_model;
Entity g_entity;

float* g_depthBuffer;



ivec2 positions[3] = {
    // { 10, 50},
    // { 190, 100 },
    // { 60, 170 },

    { 130, 100},
    { 190, 80 },
    { 60, 170 },
};
int selectedPoint = 0;

int heldKeys[ELOSKEY_MAX];

static u32 g_random_state = 123456789;

u32 random_u32(void)
{
    u32 x = g_random_state;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;

    g_random_state = x;
    return x;
}
float random_float(void)
{
    return (double)random_u32() / 4294967296.0;
}


void game_loop() {
    SupperSession* session = &g_supperSession;


    bool res = supper_load_font();
    if (!res) {
        exit(1);
    }

    init_network();

    g_depthBuffer = malloc(g_surfaceInfo.height * g_surfaceInfo.stride * sizeof(float));

    session->entities_max = 100;
    session->entities_len = 0;
    session->entities = malloc(session->entities_max * sizeof(Entity));
    
    session->players_max = 20;
    session->players_len = 0;
    session->players = malloc(session->players_max * sizeof(Entity));

    g_model = create_model();


    for (int i=0;i<100;i++) {
        Entity* entity = &session->entities[session->entities_len];
        session->entities_len++;

        memset(entity, 0, sizeof(*entity));

        entity->model = g_model;
        entity->rot.W = 1.0f;

        entity->pos.X = (random_float()-0.5)*10;
        entity->pos.Y = (random_float()-0.5)*10;
        entity->pos.Z = -5 - random_float()*20;

        entity->scale = random_float()*0.8 + 0.8;
    }

    g_entity = (Entity){ 0 };
    g_entity.model = g_model;
    g_entity.rot.W = 1;
    g_entity.scale = 1;

    g_camera.pos = (HMM_Vec3){ { 0, 0, 5 } };
    g_camera.rot = (HMM_Vec3){ { 0, 0, 0 } };
    
    while (1) {
        ELOS_UserEvent event;
        while (1) {
            bool has = get_event(&event);
            if (!has) {
                break;
            }

            heldKeys[event.key.keycode] = event.key.value > 0;

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
                    positions[selectedPoint].X -= step;
                } else if (key.keycode == ELOSKEY_RIGHT_ARROW) {
                    positions[selectedPoint].X += step;
                } else if (key.keycode == ELOSKEY_UP_ARROW) {
                    positions[selectedPoint].Y -= step;
                } else if (key.keycode == ELOSKEY_DOWN_ARROW) {
                    positions[selectedPoint].Y += step;
                }
            }
        }

        // @TODO Resize depth buffer if window size changes.

        draw_rect(0, 0, g_surfaceInfo.width, g_surfaceInfo.height, 0xFF002000);
        for (int i=0;i<g_surfaceInfo.height * g_surfaceInfo.stride;i++) {
            g_depthBuffer[i] = __FLT_MAX__;
        }

        // cstring text = PTR_CSTR("1/2/3 and arrow keys");
        // draw_glyphs_from_text_bcolor(10, 10, 30, text, g_default_font, WHITE, 0);
        
        char buffer[256];
        cstring text;
        text.ptr = buffer;
        text.len = snprintf(buffer, sizeof(buffer), "%d %d %d, %d %d %d\n", (int)(100*g_camera.pos.X), (int)(100*g_camera.pos.Y), (int)(100*g_camera.pos.Z), (int)(100*g_camera.rot.X), (int)(100*g_camera.rot.Y), (int)(100*g_camera.rot.Z));
        draw_glyphs_from_text_bcolor(10, 10, 20, text, g_default_font, WHITE, 0);


        // draw_triangle(positions[0].x, positions[0].y, positions[1].x, positions[1].y, positions[2].x, positions[2].y, 0xFF777777);
        
        // draw_line(positions[0].x, positions[0].y, positions[1].x, positions[1].y, 1, WHITE);
        // draw_line(positions[1].x, positions[1].y, positions[2].x, positions[2].y, 1, WHITE);
        // draw_line(positions[2].x, positions[2].y, positions[0].x, positions[0].y, 1, WHITE);

        update_camera_matrix();

        update_game();

        network_send_updated_position();

        network_handle_messages();

        render_game();

        prism_presentSurface(g_surface);
        // sleep((1000/10)*1000000);
        sleep((1000/60)*1000000);
        // sleep((1000/30)*1000000);
    }

}



void update_game() {
    SupperSession* session = &g_supperSession;

    float speed = 0.13;
    float rotSpeed = 0.069;

    if (heldKeys[ELOSKEY_LEFT_SHIFT]) {
        speed *= 2;
        rotSpeed *= 2;
    }

    HMM_Vec3 forwardVector = {{ sinf(g_camera.rot.Y), 0, cosf(g_camera.rot.Y) }};
    HMM_Vec3 rightVector   = {{ cosf(g_camera.rot.Y), 0, -sinf(g_camera.rot.Y) }};

    if (heldKeys[ELOSKEY_W]) {
        g_camera.pos.Z -= forwardVector.Z * speed;
        g_camera.pos.X -= forwardVector.X * speed;
    }
    if (heldKeys[ELOSKEY_A]) {
        g_camera.pos.Z -= rightVector.Z * speed;
        g_camera.pos.X -= rightVector.X * speed;
    }
    if (heldKeys[ELOSKEY_S]) {
        g_camera.pos.Z += forwardVector.Z * speed;
        g_camera.pos.X += forwardVector.X * speed;
    }
    if (heldKeys[ELOSKEY_D]) {
        g_camera.pos.Z += rightVector.Z * speed;
        g_camera.pos.X += rightVector.X * speed;
    }
    if (heldKeys[ELOSKEY_LEFT_CTRL]) {
        g_camera.pos.Y -= speed;
    }
    if (heldKeys[ELOSKEY_SPACE]) {
        g_camera.pos.Y += speed;
    }


    if (heldKeys[ELOSKEY_UP_ARROW]) {
        g_camera.rot.X += rotSpeed;
    }
    if (heldKeys[ELOSKEY_LEFT_ARROW]) {
        g_camera.rot.Y += rotSpeed;
    }
    if (heldKeys[ELOSKEY_DOWN_ARROW]) {
        g_camera.rot.X -= rotSpeed;
    }
    if (heldKeys[ELOSKEY_RIGHT_ARROW]) {
        g_camera.rot.Y -= rotSpeed;
    }


    float spinSpeed = 0.01f;

    HMM_Quat delta =
        HMM_QFromAxisAngle_LH(
            HMM_V3(0.3, 1, 0.08),
            spinSpeed
        );

    g_entity.rot = HMM_MulQ(delta, g_entity.rot);
    g_entity.rot = HMM_NormQ(g_entity.rot);

}

void update_camera_matrix() {
    g_camera.perspectiveMatrix = HMM_Perspective_RH_NO(90.0f, (float)g_surfaceInfo.width / (float)g_surfaceInfo.height, 0.01f, 400.f);
    
    g_camera.rotationMatrix = HMM_Rotate_RH(g_camera.rot.Z, (HMM_Vec3){{0,0,1}});
    g_camera.rotationMatrix = HMM_MulM4(g_camera.rotationMatrix, HMM_Rotate_RH(g_camera.rot.X, (HMM_Vec3){{1,0,0}}));
    g_camera.rotationMatrix = HMM_MulM4(g_camera.rotationMatrix, HMM_Rotate_RH(g_camera.rot.Y, (HMM_Vec3){{0,1,0}}));

    g_camera.viewMatrix = HMM_Rotate_RH(-g_camera.rot.Z, (HMM_Vec3){{0,0,1}});
    g_camera.viewMatrix = HMM_MulM4(g_camera.viewMatrix, HMM_Rotate_RH(-g_camera.rot.X, (HMM_Vec3){{1,0,0}}));
    g_camera.viewMatrix = HMM_MulM4(g_camera.viewMatrix, HMM_Rotate_RH(-g_camera.rot.Y, (HMM_Vec3){{0,1,0}}));

    g_camera.viewMatrix = HMM_MulM4(g_camera.viewMatrix, HMM_Translate(HMM_MulV3F(g_camera.pos, -1.0f)));

    g_camera.viewProjectionMatrix = HMM_MulM4(g_camera.perspectiveMatrix, g_camera.viewMatrix);


}

void render_game() {
    SupperSession* session = &g_supperSession;

    for (int i=0;i<session->entities_len;i++) {
        Entity* entity = &session->entities[i];
        render_entity(entity);

    }

    // @TODO Delete player if we didn't get a message for 5-10 seconds.
    for (int i=0;i<session->players_len;i++) {
        Entity* entity = &session->players[i];
        render_entity(entity);

    }

    
    // Entity entity = {0};
    // entity = (Entity){ 0 };
    // entity.model = g_model;
    // entity.pos.X = 0.3;
    // entity.pos.Y = 0.2;
    // entity.pos.Z = 1.3;
    // entity.rot.W = 1;

    // render_entity(&entity);
}

void render_entity(Entity* entity) {
    SupperSession* session = &g_supperSession;

    Model* model = entity->model;

    HMM_Vec3 basePos = entity->pos;
    HMM_Vec3 centerPos = HMM_V3(-entity->scale/2.0f, -entity->scale/2.0f, -entity->scale/2.0f);

    HMM_Mat4 entityMatrix = HMM_Translate(basePos);
    entityMatrix = HMM_MulM4(entityMatrix, HMM_QToM4(entity->rot));
    entityMatrix = HMM_MulM4(entityMatrix, HMM_Translate(centerPos));
    entityMatrix = HMM_MulM4(entityMatrix, HMM_Scale((HMM_Vec3){{entity->scale,entity->scale,entity->scale}}));

    for (int i=0;i<model->triangles_len;i++) {
        Triangle3D triangle = model->triangles[i];

        for (int pi=0;pi<3;pi++) {
            HMM_Vec4 p = { 0 };
            p.X = triangle.points[pi].X;
            p.Y = triangle.points[pi].Y;
            p.Z = triangle.points[pi].Z;
            p.W = 1;
            p = HMM_MulM4V4(entityMatrix, p);
            triangle.points[pi] = p.XYZ;
        }
        
        render_triangle(&triangle);
    }
}

void render_triangle(Triangle3D* triangle) {
    SupperSession* session = &g_supperSession;
    Triangle2D tri2 = { 0 };

    // Projection math

    HMM_Vec3 ab = HMM_SubV3(triangle->points[1], triangle->points[0]);
    HMM_Vec3 ac = HMM_SubV3(triangle->points[2], triangle->points[0]);
    HMM_Vec3 normal = HMM_NormV3(HMM_Cross(ab, ac));

    HMM_Vec3 light_dir = HMM_NormV3((HMM_Vec3){{ -0.19, -0.9, -0.4 }});
    float brightness = -HMM_DotV3(normal, light_dir);

    int ambientGray = 30;
    int gray = brightness < (float)ambientGray/256.0 ? ambientGray : brightness * 256;
    u32 color = 0xFF000000 | (gray << 16) | (gray << 8) | (gray);

    HMM_Vec4 points[3];

    for (int i=0;i<3;i++) {
        points[i] = HMM_V4V(triangle->points[i], 1.0f);
        points[i] = HMM_MulM4V4(g_camera.viewProjectionMatrix, points[i]);

        // Cull triangles behind the camera
        if (points[i].W <= 0)
            return;

        points[i].X /= points[i].W;
        points[i].Y /= points[i].W;
        points[i].Z /= points[i].W;
    }

    // Cull backside of triangle
    float facing = (points[1].X - points[0].X) * (points[2].Y - points[0].Y) -
                   (points[1].Y - points[0].Y) * (points[2].X - points[0].X);
    if (facing <= 0) {
        return;
    }

    for (int i=0;i<3;i++) {
        // printf("%d %d\n", (int)(100*point.X), (int)(100*point.Y));
        tri2.points[i].X = g_surfaceInfo.width * (1 + points[i].X) * 0.5f;
        tri2.points[i].Y = g_surfaceInfo.height * (1 - points[i].Y) * 0.5f;
        tri2.depth[i] = points[i].Z;
    }

    // Rasterize triangle

    draw_triangle(&tri2, g_depthBuffer, color);

}


Model* create_model() {
    Model* model = malloc(sizeof(Model));
    memset(model, 0, sizeof(*model));
    model->triangles_max = 1000;
    model->triangles = malloc(sizeof(*model->triangles) * model->triangles_max);
    model->triangles_len = 0;

    HMM_Vec3 points[8] = {
        {{ 0, 0, 0 }}, // 0
        {{ 1, 0, 0 }}, // 1
        {{ 0, 0, 1 }}, // 2
        {{ 1, 0, 1 }}, // 3
        {{ 0, 1, 0 }}, // 4
        {{ 1, 1, 0 }}, // 5
        {{ 0, 1, 1 }}, // 6
        {{ 1, 1, 1 }}, // 7
    };

    // Bottom
    model->triangles[model->triangles_len].points[0] = points[0];
    model->triangles[model->triangles_len].points[1] = points[1];
    model->triangles[model->triangles_len].points[2] = points[3];
    model->triangles_len++;
    model->triangles[model->triangles_len].points[0] = points[3];
    model->triangles[model->triangles_len].points[1] = points[2];
    model->triangles[model->triangles_len].points[2] = points[0];
    model->triangles_len++;
    
    // Top
    model->triangles[model->triangles_len].points[0] = points[5];
    model->triangles[model->triangles_len].points[1] = points[4];
    model->triangles[model->triangles_len].points[2] = points[7];
    model->triangles_len++;
    model->triangles[model->triangles_len].points[0] = points[6];
    model->triangles[model->triangles_len].points[1] = points[7];
    model->triangles[model->triangles_len].points[2] = points[4];
    model->triangles_len++;
    
    // Front
    model->triangles[model->triangles_len].points[0] = points[1];
    model->triangles[model->triangles_len].points[1] = points[0];
    model->triangles[model->triangles_len].points[2] = points[5];
    model->triangles_len++;
    model->triangles[model->triangles_len].points[0] = points[4];
    model->triangles[model->triangles_len].points[1] = points[5];
    model->triangles[model->triangles_len].points[2] = points[0];
    model->triangles_len++;
    
    // Back
    model->triangles[model->triangles_len].points[0] = points[2];
    model->triangles[model->triangles_len].points[1] = points[3];
    model->triangles[model->triangles_len].points[2] = points[7];
    model->triangles_len++;
    model->triangles[model->triangles_len].points[0] = points[7];
    model->triangles[model->triangles_len].points[1] = points[6];
    model->triangles[model->triangles_len].points[2] = points[2];
    model->triangles_len++;
    
    // Left
    model->triangles[model->triangles_len].points[0] = points[0];
    model->triangles[model->triangles_len].points[1] = points[2];
    model->triangles[model->triangles_len].points[2] = points[6];
    model->triangles_len++;
    model->triangles[model->triangles_len].points[0] = points[6];
    model->triangles[model->triangles_len].points[1] = points[4];
    model->triangles[model->triangles_len].points[2] = points[0];
    model->triangles_len++;
    
    // Right
    model->triangles[model->triangles_len].points[0] = points[3];
    model->triangles[model->triangles_len].points[1] = points[1];
    model->triangles[model->triangles_len].points[2] = points[7];
    model->triangles_len++;
    model->triangles[model->triangles_len].points[0] = points[5];
    model->triangles[model->triangles_len].points[1] = points[7];
    model->triangles[model->triangles_len].points[2] = points[1];
    model->triangles_len++;
    
    return model;
}




ELOS_Net_Handle g_net_handle;

void init_network() {
    // We use UDP because it's simple
    // First check if server exists.
    
    ELOS_Net_Address address = {0};
    address.protocol = ELOS_NET_PROTO_UDP_IPV4;
    // address.udp_tcp4.address = net_ipv4_from_str("127.0.0.1");
    address.udp_tcp4.address = 0;
    address.udp_tcp4.port    = 5007;
    // snprintf(address.identifier, sizeof(address.identifier), "127.0.0.1:8080");

    ELOS_Error error = net_open(&address, &g_net_handle);
    
    if (error != ELOS_OK) {
        printf("NET_OPEN was not OK\n");
        g_net_handle = NULL;
        return;
    }
    printf("NET_OPEN success, handle=%p\n", g_net_handle);
}



void network_send_updated_position() {

    if (!g_net_handle) {
        return;
    }

    // @TODO Implement local network mode and online mode where we connect to public server.
    // @TODO Implement DNS resolution.
    u32 addresses[] = {
        // net_ipv4_from_str("192.168.0.60"),
        net_ipv4_from_str("192.168.0.2"),
        net_ipv4_from_str("192.168.0.3"),
    };

    HMM_Quat pitch = HMM_QFromAxisAngle_RH(
        HMM_V3(1.0f, 0.0f, 0.0f),
        g_camera.rot.X
    );

    HMM_Quat yaw = HMM_QFromAxisAngle_RH(
        HMM_V3(0.0f, 1.0f, 0.0f),
        g_camera.rot.Y
    );

    HMM_Quat roll = HMM_QFromAxisAngle_RH(
        HMM_V3(0.0f, 0.0f, 1.0f),
        g_camera.rot.Z
    );

    HMM_Quat entity_rot = HMM_MulQ(yaw, HMM_MulQ(pitch, roll));

    char messageBuffer[512];
    MessageHeader* message = (void*)messageBuffer;
    message->kind = MESSAGE_LOCATION;
    message->location.count = 1;
    message->location.locations[0].id = 1;
    message->location.locations[0].pos[0] = g_camera.pos.X;
    message->location.locations[0].pos[1] = g_camera.pos.Y;
    message->location.locations[0].pos[2] = g_camera.pos.Z;
    message->location.locations[0].rot[0] = entity_rot.X;
    message->location.locations[0].rot[1] = entity_rot.Y;
    message->location.locations[0].rot[2] = entity_rot.Z;
    message->location.locations[0].rot[3] = entity_rot.W;


    for (int i=0;i<sizeof(addresses)/sizeof(*addresses);i++) {
        ELOS_Net_Address address = {0};
        address.protocol = ELOS_NET_PROTO_UDP_IPV4;
        address.udp_tcp4.address = addresses[i];
        address.udp_tcp4.port    = 5007;

        
        ELOS_Error error;
        
        error = net_write(g_net_handle, &address, message, sizeof(MessageHeader) + message->location.count * sizeof(PlayerLocation));
        if (error != ELOS_OK) {
            // There should be one we can't send to?
            // printf("net_write: Send pos %s\n", elos_error(error));
        } else {
            // printf("net_write: Sent pos\n");
        }
    }
}

void network_handle_messages() {
    SupperSession* session = &g_supperSession;

    if (!g_net_handle) {
        return;
    }

    ELOS_Error error;

    while (1) {
        ELOS_Net_Address address = {0};
        char messageBuffer[512];
        u32  bufferSize = sizeof(messageBuffer);
        MessageHeader* message = (void*)messageBuffer;
        
        error = net_read(g_net_handle, &address, message, &bufferSize, 0);
        if (error == ELOS_ERR_TIMEOUT) {
            // no messages
            break;
        } else if (error != ELOS_OK) {
            printf("net_read: Send pos %s\n", elos_error(error));
        }

        // printf("Message %d %d %d\n", message->kind, message->location.count, (int)(message->location.locations[0].pos[0]*10));

        switch (message->kind) {
            case MESSAGE_LOCATION: {
                for (int i=0;i<message->location.count;i++) {
                    PlayerLocation* loc = &message->location.locations[i];
                    
                    int foundIndex = -1;
                    for (int ei=0;ei<session->players_len;ei++) {
                        Entity* entity = &session->players[ei];

                        if (entity->id == loc->id) {
                            foundIndex = ei;
                            break;
                        }
                    }
                    
                    Entity* entity = NULL;

                    if (foundIndex != -1) {
                        entity = &session->players[foundIndex];
                    } else if (session->players_len < session->players_max) {
                        entity = &session->players[session->players_len];
                        session->players_len++;
                        memset(entity, 0, sizeof(*entity));

                        entity->rot.W = 1;
                        entity->scale = 1;
                        entity->model = g_model;
                        entity->id = loc->id;
                    }

                    if (entity) {
                        // @TODO Update color
                        entity->pos.X = loc->pos[0];
                        entity->pos.Y = loc->pos[1];
                        entity->pos.Z = loc->pos[2];
                        entity->rot.X = loc->rot[0];
                        entity->rot.Y = loc->rot[1];
                        entity->rot.Z = loc->rot[2];
                        entity->rot.W = loc->rot[3];
                    }
                }
            } break;
            default: // skip
        }

        // printf("Got POS %d %d %d\n",
        //     (int)(message->location.locations[0].pos[0]*10),
        //     (int)(message->location.locations[0].pos[1]*10),
        //     (int)(message->location.locations[0].pos[2]*10)
        //     );
    }
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

