/*
    Main terminal

    Terminal app
        scrolling
        history
        keyboard input

    Shell executor
        start processes
            printed output sent to terminal, stdout handle.
*/

#include "elos/system_console.h"

#include "elos/keyboard.h"
#include "elos/kernel_console.h"
#include "elos/kernel/video/frame.h"
#include "elos/cpu.h"
#include "elos/execution.h"
#include "elos/vfs.h"
#include "elos/physical_memory.h"
#include "elos/audio.h"
#include "elos/kernel/audio/wav.h"

#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

#define printf(...) KCON_printf(__VA_ARGS__);

static bool g_systemConsole_is_enabled;


typedef struct {
    char text[200];
} Line;

#define LINE_LIMIT 10000
Line lines[1000];
int lines_len;
int lineStart;
int lineScroll;

Line commandHistory[200];
int commandHistory_len;

Line inputBuffer;
int inputBuffer_len;

int g_terminal_cursor_pos;
int g_terminal_x;
int g_terminal_y;
int g_terminal_width;
int g_terminal_height;

int fontHeight = 16;


#define TERMINAL_BACK  0xFF053612

int textColor = WHITE;
int backColor = TERMINAL_BACK;


char cwd[256];
int  cwd_len;


void play_sound(const char* path);

void send_command(cstring text);
void edit_text(char* text_ptr, int text_max, int* inout_text_len, ELOS_Keycode keycode, int character, int mods, int* cursor);

void SCON_enable(bool enabled) {
    if (enabled) {
        disableDefaultMonitorForUsers();
    } else {
        enableDefaultMonitorForUsers();
    }
    g_systemConsole_is_enabled = enabled;
}
bool SCON_is_enabled() {
    return g_systemConsole_is_enabled;
}

static u64 ticks_per_sec;

/*
    Entry point of the system console.
    A thread should be created since it will never return.
*/
void SCON_main() {
    cwd_len = snprintf(cwd, sizeof(cwd), "/");
    ticks_per_sec = CPU_ticks_per_second();

    int screenWidth, screenHeight;
    draw_frame_info(&screenWidth, &screenHeight);

    g_terminal_height = 800;
    g_terminal_width = screenHeight - 20;
    g_terminal_x = screenWidth - g_terminal_width;
    g_terminal_y = 10;

    inputBuffer_len = 0;
    inputBuffer.text[0] = 0;

    printf("SCON main\n");

    const char* path = "/pkg/wav/dream.wav";
    play_sound(path);

    u64 startBlink = rdtsc();
    #define BLINK_CYCLE 1000

    #define REFRESH_RATE (1000000000LU / 60LU)

    while (1) {
        
        // Poll keyboard events
        // Edit input buffer based on key events.
        KeyEvent keyEvent;
        bool hasKeyEvent = KBD_poll_key_event(&keyEvent);

        if (!g_systemConsole_is_enabled) {
            // Yield process. until next frame.
            EXEC_sleep(REFRESH_RATE);
            continue;
        }
        
        while (hasKeyEvent) {
            if (keyEvent.keycode == ELOSKEY_ENTER && keyEvent.pressed) {
                if (inputBuffer_len > 0) {
                    cstring cmd = { inputBuffer.text, inputBuffer_len };
                    inputBuffer.text[inputBuffer_len] = 0;
                    send_command(cmd);
                    inputBuffer_len = 0;
                    inputBuffer.text[inputBuffer_len] = 0;
                }
                g_terminal_cursor_pos = 0;
            } else if (keyEvent.pressed) {
                edit_text(inputBuffer.text, sizeof(inputBuffer), &inputBuffer_len, keyEvent.keycode, keyEvent.character, keyEvent.mods, &g_terminal_cursor_pos);
            }

            hasKeyEvent = KBD_poll_key_event(&keyEvent);
        }

        /*
            Rendering to monitor frame buffer requires special care.
            If you draw two different colors on the same area then flicker will occur
            because it is displayed as soon you write to memory (as soon as it can).
            We could write to a temporary buffer then memcpy it to the monitors buffer.
            Since this is slightly slower we don't at the moment.
        */

        // @TODO We need to fix scrolling and wrapping.
        //    Where should system console be displayed? Ontop of everything?
        //    Should you be able to move it, resize?

        int maxLines = g_terminal_height / fontHeight;
        int lineIndex = (lineStart + lineScroll) % LINE_LIMIT;
        while (lineIndex < lines_len) {
            Line* line = &lines[lineIndex];
            cstring lineText = PTR_CSTR(line->text);
            int lineTextWidth = draw_text_width(lineText, fontHeight, g_default_font);
            int text_y = g_terminal_y + fontHeight * lineIndex;
            draw_glyphs_from_text_bcolor(g_terminal_x, text_y, fontHeight, lineText, g_default_font, textColor, backColor);

            // We cover right part of input text box with background but
            // this means flickering of green and white when we draw cursor on top later.
            int remainingWidth = screenWidth - g_terminal_x - lineTextWidth;
            draw_rect(g_terminal_x + lineTextWidth, text_y,  remainingWidth, fontHeight, backColor);

            lineIndex++;
        }

        int text_y = g_terminal_y + fontHeight * lines_len;
        
        cstring lineText = { inputBuffer.text, inputBuffer_len };
        // printf("lineText %x %d\n", lineText.ptr, lineText.len);
        cstring pretext = PTR_CSTR("> ");
        int pretextWidth = draw_text_width(pretext, fontHeight, g_default_font);


        draw_glyphs_from_text_bcolor(g_terminal_x, text_y, fontHeight, pretext, g_default_font, textColor, backColor);

        int text_x = g_terminal_x + pretextWidth;
        
        int lineTextWidth = draw_text_width(lineText, fontHeight, g_default_font);
        draw_glyphs_from_text_bcolor(text_x, text_y, fontHeight, lineText, g_default_font, textColor, backColor);

        // We cover right part of input text box with background but
        // this means flickering of green and white when we draw cursor on top later.
        int remainingWidth = screenWidth - text_x - lineTextWidth;
        draw_rect(text_x + lineTextWidth, text_y,  remainingWidth, fontHeight, backColor);
        
        cstring temp;
        temp.ptr = inputBuffer.text;
        temp.len = g_terminal_cursor_pos;
        int width = draw_text_width(temp, fontHeight, g_default_font);

        u64 nowBlink = rdtsc();
        u64 time_ms = (nowBlink - startBlink) / (ticks_per_sec / 1000);
        if ((time_ms % BLINK_CYCLE) > BLINK_CYCLE/2)
        draw_rect(text_x + width, text_y, 2, fontHeight, WHITE);

        EXEC_sleep(REFRESH_RATE);
    }

}


void edit_text(char* text_ptr, int text_max, int* inout_text_len, ELOS_Keycode keycode, int character, int mods, int* cursor) {

    int text_len = *inout_text_len;

    if (keycode == ELOSKEY_LEFT_ARROW) {
        if (*cursor > 0) {
            (*cursor)--;
        }
        // @TODO Handle shift selection and delete.
        //   ctrl+c as well?
    } else if (keycode == ELOSKEY_RIGHT_ARROW) {
        if (*cursor < text_len) {
            (*cursor)++;
        }
    } else if (keycode == ELOSKEY_DELETE) {
        if (text_len <= *cursor ) {
            return;
        }
        memmove(text_ptr + *cursor, text_ptr + *cursor+1, text_len - *cursor - 1);
        text_len--;
    } else if (keycode == ELOSKEY_HOME) {
        *cursor = 0;
    } else if (keycode == ELOSKEY_END) {
        *cursor = text_len;
    } else if (keycode == ELOSKEY_BACKSPACE) {
        if (*cursor <= 0 ) {
            return;
        } else {
            memmove(text_ptr + *cursor - 1, text_ptr + *cursor, text_len - *cursor);
            (*cursor)--;
            text_len--;
        }
    } else {
        if (character == 0)
            return; // not a char code.
        if (text_len >= text_max) {
            return;
        }
        if (*cursor == text_len) {
            text_ptr[*cursor] = character;
            text_len++;
            (*cursor)++;
            text_ptr[*cursor] = '\0';
        } else {
            memmove(text_ptr + *cursor + 1, text_ptr + *cursor, text_len - *cursor);
            text_ptr[*cursor] = character;
            text_len++;
            (*cursor)++;
        }
    }
    *inout_text_len = text_len;
}

void respond_message(cstring text) {
    int head = 0;
    int startHead = 0;
    while (head < text.len) {
        char chr = text.ptr[head];
        head++;

        if (chr == '\n' || head == text.len) {
            Line* nextLine = &lines[lines_len];
            lines_len++;
            
            int len;
            if (chr == '\n')
                len = head-1 - startHead;
            else
                len = head - startHead;

            int cap = len < sizeof(nextLine->text)-1 ? len : sizeof(nextLine->text)-1;
            memcpy(nextLine->text, text.ptr + startHead, cap);

            startHead = head;
        }
    }
}

void printCallback(const char* buffer, size_t size, void* userData) {
    cstring text = { .ptr = buffer, .len = size };
    respond_message(text);
}


void resolvePath(char* fullpath, int fullpathMax, const char* ls_path) {
    if (ls_path[0] == '/') {
        snprintf(fullpath, fullpathMax, "%s", ls_path);
    } else if (cwd[cwd_len-1] == '/') {
        snprintf(fullpath, fullpathMax, "%s%s", cwd, ls_path);
    } else {
        snprintf(fullpath, fullpathMax, "%s/%s", cwd, ls_path);
    }
}

void send_command(cstring text) {

    // Shell executor

    if (!strcmp(text.ptr, "help")) {

        char* message = 
            "Commands:\n"
            "   cd ls             (you know what these do, flags not supported)\n"
            "   doom slate prism  (programs to start, killed if already exists)\n"
        ;

        respond_message((cstring){ .ptr = message, .len = strlen(message) });

    } else if (!strcmp(text.ptr, "cd") || !strncmp(text.ptr, "cd ", 3)) {

        char tempPath[256];
        if (text.len > 3) {
            const char* ls_path = text.ptr + 3;
            resolvePath(tempPath, sizeof(tempPath), ls_path);
        } else {
            snprintf(tempPath, sizeof(tempPath), "%s", cwd);
        }
        strncpy(cwd, tempPath, sizeof(cwd));
        cwd_len = strlen(cwd);

        char msg[256];
        int  msg_len = snprintf(msg, sizeof(msg), "CWD: %s\n", cwd);

        respond_message((cstring){ .ptr = msg, .len = msg_len });

    } else if (!strcmp(text.ptr, "ls") || !strncmp(text.ptr, "ls ", 3)) {

        char fullpath[256];
        if (text.len > 3) {
            const char* ls_path = text.ptr + 3;
            resolvePath(fullpath, sizeof(fullpath), ls_path);
        } else {
            snprintf(fullpath, sizeof(fullpath), "%s", cwd);
        }

        cstring basePath = { .ptr = fullpath, .len = strlen(fullpath) };
        respond_message(basePath);


        ELOS_DirectoryEntry dirEntries[3];
        int                 dirEntries_cap = ARRAY_LENGTH(dirEntries);

        u64 cookie = 0;
        u64 entryCount;
        
        while (1) {
            entryCount = dirEntries_cap;
            bool yes = VFS_readdir(fullpath, &cookie, &entryCount, dirEntries);
            if (!yes) {
                printf("VFS_readdir: returned false, error\n");
                break;
            }
            char msg[256];
            for (int i=0;i<entryCount;i++) {
                ELOS_DirectoryEntry* entry = &dirEntries[i];
                int len = snprintf(msg, sizeof(msg), "%s\n", entry->name);
                printf("%s\n", entry->name);
                respond_message((cstring){ msg, len });
            }
            if (entryCount != dirEntries_cap) {
                // The end
                break;
            }
        }
    } else if (!strcmp(text.ptr, "prism")) {
        // Kill prism if it exists
        EXEC_kill("prism");
        EXEC_create_user_thread("/pkg/prism/prism.elf", -1);
    } else if (!strcmp(text.ptr, "slate")) {
        // Kill slate if it exists
        EXEC_kill("slate");
        EXEC_create_user_thread("/pkg/slate/slate.elf", -1);
    } else if (!strcmp(text.ptr, "doom")) {
        // Kill doom if it exists
        EXEC_kill("doom");
        EXEC_create_user_thread("/pkg/doom/doom.elf", -1);
    } else if (!strcmp(text.ptr, "mount")) {
        VFS_dump_mounts(printCallback, NULL);
    } else if (!strcmp(text.ptr, "sound")) {
        const char* path = "/pkg/wav/dream.wav";
        play_sound(path);
    } else {
        respond_message(PTR_CSTR("Unknown command\n"));
    }
}

static AudioDevice audioDevice;

void play_sound(const char* path) {


    int len = 1;
    AUDIO_scan_devices(&audioDevice, &len);

    if (len == 0) {
        // @TODO Response
        printf("No audio device\n");
        return;
    }

    ELOS_AudioDeviceInfo info;
    AUDIO_get_info(audioDevice, &info);
    
    printf("Playing sound from %s\n", info.name);

    WAVFile* file;
    WAVError err = ReadWAVFile(path, &file, true);

    if (err != WAV_SUCCESS) {
        // @TODO Respond
        printf("Could not read %s\n", path);
        return;
    }

    ELOS_AudioBuffer* buffer;
    ELOS_AudioFormat format = {
        .sampleRate = 48000,
        .sampleFormat = ELOS_AUDIO_16BIT_PCM,
        .channels = 2,
    };

    u32 bufferSize = 0x20000;

    bool yes = AUDIO_create_buffer(audioDevice, &format, bufferSize, &buffer);
    if (!yes) {
        printf("Could not create audio ring\n");
        return;
    }

    // play a sound
    int audioOffset = 0;
    int audioEnd = file->data_len;
    while (true) {
        if (audioOffset >= audioEnd) {
            break;
        }

        if (buffer->head - buffer->tail >= bufferSize) {
            printf("Audio full %d, %d\n", buffer->head, buffer->tail);
            pause();
            continue;
        }

        *(u16*)(buffer->data + (buffer->head & buffer->sizeMask))       = *(u16*)(file->data + audioOffset);
        *(u16*)(buffer->data + ((buffer->head + 2) & buffer->sizeMask)) = *(u16*)(file->data + audioOffset + 2);

        buffer->head += 4;
        audioOffset  += 4;
    }

    // Sound is done. Free buffers etc.

}

