/*
    Sample config

    
    # Boot config
    static_ip = "192.168.0.68"


    netboot_port = 2493
    netboot_servers = [
        "192.168.100.50" # QEMU tap0
        "192.168.0.60"   # BEAST (dev machine)
    ]

    log_level = 3

*/

#include "elos/kernel/cfg/kernel_cfg.h"

#include "elos/common/string.h"

#include "elos/kernel/net/protocol.h"


typedef struct ParseContext {
    // in/out parameters
    const char* text;
    int text_len;
    KernelConfig* config;
    char** error;

    // internal
    int head;
    int line;
} ParseContext;


static inline bool string_equal(const string key, const char* name) {
    return !strncmp(key.ptr, name, key.len);
}

static bool parse_address(string str, u32* address);
static bool parse_key(ParseContext* ctx, string* key);
static bool parse_string(ParseContext* ctx, string* key);
static bool parse_int(ParseContext* ctx, int* key);
static void parse_whitespace(ParseContext* ctx);

static char last_message[256];


#define PARSE_ERROR(FMT, ...) do { \
    snprintf(last_message, sizeof(last_message), "config:%d: " FMT, ctx->line __VA_OPT__(,) __VA_ARGS__); \
    *ctx->error = last_message; \
    return false; \
    } while (0)

bool CFG_parse(const char* text, int text_len, KernelConfig* config, char** error) {
    
    memset(config, 0, sizeof(*config));


    ParseContext context = {
        .text = text,
        .text_len = text_len,
        .config = config,
        .error = error,
        .head = 0,
        .line = 1,
    };
    ParseContext* ctx = &context;

    while (ctx->head < ctx->text_len) {

        parse_whitespace(ctx);
        
        if (ctx->head >= ctx->text_len)
            break;

        string key = {0};
        parse_key(ctx, &key);

        if (key.len == 0) {
            PARSE_ERROR("Invalid key");
        }

        parse_whitespace(ctx);

        if (ctx->text[ctx->head] != '=') {
            PARSE_ERROR("Missing = after key");
        }
        ctx->head++;

        parse_whitespace(ctx);

        if (string_equal(key, "netboot_ips")) {
            // u32 netboot_server_ips[8];
            char chr = ctx->text[ctx->head];
            if (chr != '[') {
                PARSE_ERROR("Invalid array after key");
            }
            ctx->head++;

            while (ctx->head < ctx->text_len) {
                parse_whitespace(ctx);

                char chr = ctx->text[ctx->head];
                if (chr == ']') {
                    break;
                }

                string str = {0};

                bool found = parse_string(ctx, &str);
                if (!found) {
                    PARSE_ERROR("Invalid string in array.");
                }

                if (ctx->config->netboot_ips_len >= ARRAY_LENGTH(ctx->config->netboot_ips)) {
                    PARSE_ERROR("Too many address in array (max is %d).", ARRAY_LENGTH(ctx->config->netboot_ips));
                }
                
                u32 address;
                found = parse_address(str, &address);
                if (!found) {
                    PARSE_ERROR("Invalid address after key");
                }

                ctx->config->netboot_ips[ctx->config->netboot_ips_len] = address;
                ctx->config->netboot_ips_len++;

                parse_whitespace(ctx);

                chr = ctx->text[ctx->head];
                if (chr == ',') {
                    // commas are optional
                    ctx->head++;
                }
            }
            chr = ctx->text[ctx->head];
            if (chr != ']') {
                PARSE_ERROR("Expected ] after array elements");
            }
            ctx->head++;
            
        } else if (string_equal(key, "netboot_port")) {
            int port = 0;
            bool found = parse_int(ctx, &port);
            if (!found) {
                PARSE_ERROR("Invalid integer after key");
            }
            if (port < 0 || port > 0xFFFF) {
                PARSE_ERROR("Port must be in range [0, 65535]");
            }
            ctx->config->netboot_port = port;
            
        } else if (string_equal(key, "log_level")) {
            int level = 0;
            bool found = parse_int(ctx, &level);
            if (!found) {
                PARSE_ERROR("Invalid integer after key");
            }
            ctx->config->log_level = level;

        } else if (string_equal(key, "static_ip")) {
            string str = {0};
            bool found = parse_string(ctx, &str);
            if (!found) {
                PARSE_ERROR("Invalid string after key");
            }

            u32 address;
            found = parse_address(str, &address);
            if (!found) {
                PARSE_ERROR("Invalid address after key");
            }

            ctx->config->static_ip = address;

        } else {
            char buffer[256];
            memcpy(buffer, key.ptr, key.len);
            buffer[key.len] = '\0';
            PARSE_ERROR("Unknown config option: %s", buffer);
        }
    }

    return true;
}


bool parse_address(string str, u32* address) {
    char buffer[100];
    if (str.len+1 > sizeof(buffer))
        return false;

    memcpy(buffer, str.ptr, str.len);
    buffer[str.len] = '\0';
    
    char* string = buffer;
    u32 num;

    num  = (u32)strtol(string  , &string, 10);
    if (!string) return false;
    if (*string != '.') return false;
    
    num |= (u32)strtol(string+1, &string, 10) << 8;
    if (!string) return false;
    if (*string != '.') return false;
    
    num |= (u32)strtol(string+1, &string, 10) << 16;
    if (!string) return false;
    if (*string != '.') return false;
    
    num |= (u32)strtol(string+1, &string, 10) << 24;
    if (!string) return false;

    *address = num;

    return true;
}

bool parse_key(ParseContext* ctx, string* key) {
    int start_head = ctx->head;
    while (ctx->head < ctx->text_len) {
        char chr = ctx->text[ctx->head];
        if (((chr|32) >= 'a' && (chr|32) <= 'z') || chr == '_') {
            ctx->head++;
            continue;
        }
        if (chr >= '0' && chr <= '9' && start_head != ctx->head) {
            ctx->head++;
            continue;
        }
        break;
    }
    key->ptr = (char*)ctx->text + start_head;
    key->len = ctx->head - start_head;
    return key->len > 0;
}

bool parse_string(ParseContext* ctx, string* key) {
    int start_head = ctx->head;

    char chr = ctx->text[ctx->head];
    if (chr != '"') {
        return false;
    }
    ctx->head++;
    
    while (ctx->head < ctx->text_len) {
        char chr = ctx->text[ctx->head];
        if (chr == '"') {
            break;
        }
        ctx->head++;
    }
    chr = ctx->text[ctx->head];
    if (chr != '"') {
        return false;
    }
    ctx->head++;

    key->ptr = (char*)ctx->text + start_head + 1;
    key->len = ctx->head - start_head - 2;
    return key->len > 0;
}


bool parse_int(ParseContext* ctx, int* key) {
    char* endptr = NULL;
    *key = strtol(ctx->text + ctx->head, &endptr, 0);
    if (endptr) {
        ctx->head = endptr - ctx->text;
    }
    return endptr;
}

void parse_whitespace(ParseContext* ctx) {
    
    while (ctx->head < ctx->text_len) {
        char chr = ctx->text[ctx->head];

        if (chr == '#') {
            // @TODO slash and multi-line comments
            ctx->head++;

            while (ctx->head < ctx->text_len && ctx->text[ctx->head] != '\n') {
                ctx->head++;
            }
            continue;
        }

        if (chr == '\n' || chr == '\r' || chr == ' ' || chr == '\t' || chr == '\f') {
            if (chr == '\n')
                ctx->line++;
            ctx->head++;
            continue;
        }
        break;
    }

}

