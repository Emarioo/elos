/*
    @TODO

    Consider memory allocation for line text. Current approach is frail.
        Realloc or allocate more text buffer memory.
        Since the memory is used in lines we can't realloc because it would
        invalidate the memory.
        We would need to allocate new memory, go through all lines and copy text
        to new buffer then free old buffer.
        Or we can have a linked list of memory chunks we can allocate from.
        A heap allocator implementation in user land that is optimized
        for small strings may be a good idea.
        Or some other good idea for small strings.

    Opening big files. How large files do we want to support. 512MB?
        1GB but special optimizations for memory with read-only limitation?

    Show column and line number where cursor is, for multiple cursors pick first cursor?
        Scroll in X direction. important for long lines
        Pick text_content_x based on how many lines are in the file. Large numbers (1002) touches the text area.

    Insert, delete, replace
    Undo, redo
    Multiple cursors
    Text selection
    Copy and paste
    Syntax highlight
    Handle large files (1G)
    Search, regex matching and replace
    Hex editor

    IN ELOS implement repeat kernel service.
    Don't use repeat events from the keyboard device.
    Make our own where we can choose repeat delay and frequency.
    We can also support multiple repeats. For example you hold down left and up arrow. Only up arrow is repeated.
    You release up arrow and nothing happens even though left arrow is still down. This we can fix with repeat kernel service.

Tests
    Open /pkg/slate/slate.elf and replace .rodata strings with something else and save.
    Restart the program and see if different text is printed.

*/

#include "slate/slate.h"

#include "prism/prism.h"
#include "elos/syscalls.h"
#include "elos/common/intrinsics.h"
#include "elos/common/string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stdui.h"

#include "wave.h"



void editor_loop();


#define SLATE_OFF_WHITE      0xFFD0D0D0
#define SLATE_DARK_BLUE      0xFF0F172A
#define SLATE_DARKER_BLUE    0xFF051120

PrismInstance* g_instance;
PrismSurface* g_surface;
PrismSurfaceInfo g_surfaceInfo;
u64 ticks_per_second;
ELOS_UserEventBuffer* userEvents;
SlateSession slateSession;

bool get_event(ELOS_UserEvent* event) {
    // @TODO Not thread or context switch safe.
    u32 tail = userEvents->tail % userEvents->maxEvents;
    u32 head = userEvents->head % userEvents->maxEvents;
    if (tail == head) {
        return false;
    }
    *event = userEvents->events[tail];
    userEvents->tail++;
    return true;
}


void play_sound(const char* path);
void dumpdir(const char* path, int depth);

void _start() {

    // @NOCHECKIN Temporary
    // dumpdir("/", 0);
    

    SYS_ticks_per_second(&ticks_per_second);



    g_instance = prism_init();
    if (!g_instance) {
        printf("slate: Could not init PRISM client\n");
        exit(1);
    }

    g_surface = prism_createSurface(g_instance, 800, 600);
    if (!g_surface) {
        printf("slate: Could not create surface\n");
        exit(1);
    }

    prism_surfaceInfo(g_surface, &g_surfaceInfo);

    prism_moveSurface(g_surface, 200, 200);

    ELOS_Error error;
    error = SYS_request_user_event_buffer(100, &userEvents);
    if (error != ELOS_OK) {
        printf("slate: Could not create user event buffer\n");
        exit(1);
    }

    stdui_set_surface(&g_surfaceInfo);

    // play_sound("/pkg/wav/dream.wav");

    editor_loop();
}


#define LINE_LENGTH(LINE) ((LINE)->text.len)


void apply_numpad(ELOS_Keycode* keycode, int mod) {
    if (!(!(mod & ELOSKEY_MOD_NUM_LOCK) ^ !(mod & ELOSKEY_MOD_SHIFT))) {
        switch ((int)*keycode) {
            case ELOSKEY_NUMPAD_0: *keycode = ELOSKEY_INSERT; return;
            case ELOSKEY_NUMPAD_1: *keycode = ELOSKEY_END; return;
            case ELOSKEY_NUMPAD_2: *keycode = ELOSKEY_DOWN_ARROW; return;
            case ELOSKEY_NUMPAD_3: *keycode = ELOSKEY_PAGE_DOWN; return;
            case ELOSKEY_NUMPAD_4: *keycode = ELOSKEY_LEFT_ARROW; return;
            case ELOSKEY_NUMPAD_6: *keycode = ELOSKEY_RIGHT_ARROW; return;
            case ELOSKEY_NUMPAD_7: *keycode = ELOSKEY_HOME; return;
            case ELOSKEY_NUMPAD_8: *keycode = ELOSKEY_UP_ARROW; return;
            case ELOSKEY_NUMPAD_9: *keycode = ELOSKEY_PAGE_UP; return;
            case ELOSKEY_NUMPAD_COMMA: *keycode = ELOSKEY_DELETE; return;
        }
    }
}

void editor_loop() {
    SlateSession* session = &slateSession;

    session->config = (SlateConfig){
        .color_text                   = WHITE,
        .color_cursor                 = WHITE,
        .color_background             = SLATE_DARK_BLUE,
        .color_commandBackgroundColor = SLATE_DARKER_BLUE,
        .color_lineNumber             = SLATE_OFF_WHITE,
        
        .showLineNumbers  = true,
    };

    SlateConfig* config = &session->config;

    bool res = slate_load_font();
    if (!res) {
        exit(1);
    }

    // Normally we want to edit /boot/TEMPLATE.CFG but it exists on USB drive which
    // we don't have disk support for so we edit this one from initrd instead.
    const char* defaultPath = "/TEMPLATE.CFG";

    slate_open(session, defaultPath);

    session->commandBuffer_len = strlen(defaultPath);
    memcpy(session->commandBuffer, defaultPath, session->commandBuffer_len);
    session->commandBuffer[session->commandBuffer_len] = 0;

    // We should have at least one line even if file is empty or
    // couldn't be found.
    // ASSERT(session->lines_len != 0);


    int text_height = 20;
    int text_lineNumberHeight = 14;

    int textContent_x = 20;

    int characterWidth;
    {
        cstring temp = PTR_CSTR("hello");
        int tempWidth = draw_text_width(temp, text_height, g_default_font);
        characterWidth = tempWidth / temp.len;
    }

    
    while (1) {
        
        int textContent_width = g_surfaceInfo.width - textContent_x;
        
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

            apply_numpad(&event.key.keycode, event.key.mods);


            ELOS_UserEvent_Key key = event.key;
            
            if (key.keycode == ELOSKEY_Q && (key.mods & ELOSKEY_MOD_CTRL)) {
                printf("Exit slate\n");
                exit(0);
            } else if (key.keycode == ELOSKEY_O && (key.mods & ELOSKEY_MOD_CTRL)) {
                session->command = CMD_OPEN_FILE;
            } else if (key.keycode == ELOSKEY_S && (key.mods & ELOSKEY_MOD_CTRL)) {
                session->command = CMD_SAVE_FILE;
            } else {
                if (session->command == CMD_OPEN_FILE || session->command == CMD_SAVE_FILE) {
                    if (key.keycode == ELOSKEY_LEFT_ARROW) {
                        if (session->commandCursor_x > 0) {
                            session->commandCursor_x--;
                        }
                    } else if (key.keycode == ELOSKEY_RIGHT_ARROW) {
                        if (session->commandCursor_x < session->commandBuffer_len) {
                            session->commandCursor_x++;
                        }
                    } else if (key.keycode == ELOSKEY_HOME) {
                        session->commandCursor_x = 0;
                    } else if (key.keycode == ELOSKEY_END) {
                        session->commandCursor_x = session->commandBuffer_len;
                    } else if (key.keycode == ELOSKEY_BACKSPACE) {
                        if (session->commandCursor_x > 0) {
                            memmove(session->commandBuffer + session->commandCursor_x - 1,
                                session->commandBuffer + session->commandCursor_x,
                                session->commandBuffer_len - session->commandCursor_x);
                            session->commandCursor_x--;
                            session->commandBuffer_len--;
                            session->commandBuffer[session->commandBuffer_len] = '\0';
                        }
                    } else if (key.keycode == ELOSKEY_DELETE) {
                        if (session->commandCursor_x < session->commandBuffer_len) {
                            memmove(session->commandBuffer + session->commandCursor_x,
                                session->commandBuffer + session->commandCursor_x + 1,
                                session->commandBuffer_len - (session->commandCursor_x+1));
                            session->commandBuffer_len--;
                            session->commandBuffer[session->commandBuffer_len] = '\0';
                        }
                    } else if (key.keycode == ELOSKEY_ESCAPE) {
                        session->command = CMD_NONE;
                    } else if (key.keycode == ELOSKEY_ENTER) {
                        if (session->command == CMD_OPEN_FILE) {
                            slate_open(session, session->commandBuffer);
                        } else if (session->command == CMD_SAVE_FILE) {
                            slate_save(session, session->commandBuffer);
                        }
                        session->command = CMD_NONE;
                    } else if(key.character != 0) {
                        if (session->commandBuffer_len < sizeof(session->commandBuffer)-1) {
                            memmove(session->commandBuffer + session->commandCursor_x + 1,
                                    session->commandBuffer + session->commandCursor_x,
                                    session->commandBuffer_len - session->commandCursor_x);
                            session->commandBuffer[session->commandCursor_x] = key.character;
                            session->commandBuffer_len++;
                            session->commandCursor_x++;
                            session->commandBuffer[session->commandBuffer_len] = '\0';
                        }
                    }
                } else {
                    if (key.keycode == ELOSKEY_LEFT_ARROW) {
                        slate_move(key.keycode);
                    } else if (key.keycode == ELOSKEY_RIGHT_ARROW) {
                        slate_move(key.keycode);
                    } else if (key.keycode == ELOSKEY_DOWN_ARROW) {
                        slate_move(key.keycode);
                    } else if (key.keycode == ELOSKEY_UP_ARROW) {
                        slate_move(key.keycode);
                    } else if (key.keycode == ELOSKEY_HOME) {
                        slate_move(key.keycode);
                    } else if (key.keycode == ELOSKEY_END) {
                        slate_move(key.keycode);
                    } else if (key.keycode == ELOSKEY_PAGE_UP) {
                        slate_move(key.keycode);
                    } else if (key.keycode == ELOSKEY_PAGE_DOWN) {
                        slate_move(key.keycode);
                    } else if (key.keycode == ELOSKEY_BACKSPACE) {
                        slate_deletion(key.keycode);
                    } else if (key.keycode == ELOSKEY_DELETE) {
                        slate_deletion(ELOSKEY_DELETE);
                    } else if(key.character != 0) {
                        slate_insert(key.character);
                    }
                }
            }
        }

        draw_rect(0, 0, g_surfaceInfo.width, g_surfaceInfo.height, session->config.color_background);

        int maxLinesOnScreen = g_surfaceInfo.height / text_height;
        if (session->cursor_y > session->scroll_y + maxLinesOnScreen - 3) {
            session->scroll_y = session->cursor_y - maxLinesOnScreen + 3;
        }
        if (session->cursor_y < session->scroll_y + 3) {
            session->scroll_y = session->cursor_y < 3 ? 0 : session->cursor_y - 3;
        }

        if(session->lines) {
            for (int li=session->scroll_y;li<session->lines_len && li < session->scroll_y + maxLinesOnScreen;li++) {
                Line* line = &session->lines[li];
                int text_y = text_height * (li - session->scroll_y);

                if(config->showLineNumbers) {
                    char lineNumber[20];
                    int len = snprintf(lineNumber, sizeof(lineNumber), "%d", li + 1);
                    cstring lineNrText = { lineNumber, len };
                    draw_glyphs_from_text_bcolor(0, text_y + 3, text_lineNumberHeight, lineNrText, g_default_font, session->config.color_lineNumber, 0);
                }

                if (LINE_LENGTH(line) == 0)
                    continue;

                cstring text = { .ptr = line->text.ptr, .len = LINE_LENGTH(line) };
                
                int textWidth = draw_text_width(text, text_height, g_default_font);
                
                if (textWidth > textContent_width ) {
                    text.len = (textContent_width) / characterWidth;
                }


                
                draw_glyphs_from_text_bcolor(textContent_x, text_y, text_height, text, g_default_font, session->config.color_text, 0);
            }
        }
        
        // We require monospace font here
        
        if (session->command == CMD_NONE) {
            draw_rect(textContent_x + characterWidth * session->cursor_x, text_height * (session->cursor_y - session->scroll_y), 3, text_height, config->color_cursor);
        } else {
            int command_x = 2;
            int command_y = g_surfaceInfo.height - text_height - 2;

            cstring prompt;
            if (session->command == CMD_OPEN_FILE) {
                prompt = PTR_CSTR("Open file");
            } else if (session->command == CMD_SAVE_FILE) {
                prompt = PTR_CSTR("Save file");
            }
            draw_glyphs_from_text_bcolor(command_x, command_y - text_height, text_height, prompt, g_default_font, config->color_text, config->color_commandBackgroundColor);
            
            cstring text = { session->commandBuffer, session->commandBuffer_len };
            draw_rect(command_x, command_y, characterWidth * 20, text_height, session->config.color_commandBackgroundColor);
            draw_glyphs_from_text_bcolor(command_x, command_y, text_height, text, g_default_font, config->color_text, config->color_commandBackgroundColor);

            draw_rect(command_x + characterWidth * session->commandCursor_x, command_y, 3, text_height, config->color_cursor);
        }

        prism_presentSurface(g_surface);

        sleep((1000/60)*1000000);
    }

}



void dumpdir(const char* path, int depth) {

    u64 cookie = 0;
    u64 entryCount;

    ELOS_DirectoryEntry dirEntries[3]; // uneven number to encounter edge cases
    int dirEntries_cap = ARRAY_LENGTH(dirEntries);

    #define INDENT() for (int j=0;j<depth;j++) { printf("  "); }

    INDENT()
    const char* firstSlash = strrchr(path, '/');
    if (!firstSlash)
        firstSlash = path;
    printf("%s\n", firstSlash);
    
    while (1) {
        entryCount = dirEntries_cap;
        ELOS_Error err = elos_readdir(path, &cookie, &entryCount, dirEntries);
        if (err != ELOS_OK) {
            printf("elos_readdir: returned false, error\n");
            break;
        }
        char fullpath[256];
        for (int i=0;i<entryCount;i++) {
            ELOS_DirectoryEntry* entry = &dirEntries[i];

            if (entry->isDirectory) {
                if (path[strlen(path)-1] == '/')
                    snprintf(fullpath, sizeof(fullpath), "%s%s", path, entry->name);
                else
                    snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->name);
                dumpdir(fullpath, depth+1);
                // INDENT()
                // printf("  %s\n", entry->name);
            } else {
                INDENT()
                printf("  %s\n", entry->name);
            }
        }

        if (entryCount != dirEntries_cap) {
            // The end
            break;
        }
    }

}





static ELOS_AudioDevice  audioDevice;
static ELOS_AudioBuffer* audioBuffer;

WAVFile* soundFile = NULL;

void play_sound(const char* path) {

    ELOS_Error error;

    error = SYS_default_audio(&audioDevice);

    if (error != ELOS_OK) {
        // @TODO Response
        printf("No audio device\n");
        return;
    }

    ELOS_AudioDeviceInfo info;
    SYS_audio_info(audioDevice, &info);
    
    printf("Playing sound from %s\n", info.name);

    if (!soundFile) {
        WAVError err = ReadWAVFile(path, &soundFile, true);
        if (err != WAV_SUCCESS) {
            // @TODO Respond
            printf("Could not read %s\n", path);
            return;
        }
    }

    WAVFile* file = soundFile;


    ELOS_AudioFormat format = {
        .sampleRate = 48000,
        .sampleFormat = ELOS_AUDIO_16BIT_PCM,
        .channels = 2,
    };

    u32 bufferSize = 0x20000; // 2 ms latency
    // u32 bufferSize = ELOS_BYTES_PER_AUDIO_SAMPLE(format.sampleFormat) * format.channels * format.sampleRate / 50; // 2 ms latency
    // u32 bufferSize = ELOS_BYTES_PER_AUDIO_SAMPLE(format.sampleFormat) * format.channels * format.sampleRate / 500; // 2 ms latency

    if (!audioBuffer) {
        ELOS_Error error = SYS_create_audio_buffer(audioDevice, &format, bufferSize, &audioBuffer);
        if (error != ELOS_OK) {
            printf("Could not create audio ring, elos_err %s\n", elos_error(error));
            return;
        }
    }

    ELOS_AudioBuffer* buffer = audioBuffer;

    printf("FileData %p - %p, %u\n", file->data, file->data + file->data_len, file->data_len);
    printf("Buffer size/mask 0x%x 0x%x\n", bufferSize, buffer->size);

    bool firstWrite = true;

    buffer->head = buffer->tail;

    // play a sound
    int audioOffset = 0;
    int audioEnd = file->data_len;
    while (true) {
        if (audioOffset >= audioEnd) {
            break;
        }

        if ((int)((buffer->head - buffer->tail)) >= (int)bufferSize) {
            firstWrite = true;
            // printf("Audio full %d, %d\n", buffer->head, buffer->tail);
            pause();
            continue;
        }
        
        if (firstWrite) {
            // printf("Write %u %u\n", buffer->head, audioOffset);
            firstWrite = false;
        }
        *(u16*)(buffer->data + (buffer->head % buffer->size))       = *(u16*)(file->data + audioOffset);
        *(u16*)(buffer->data + ((buffer->head + 2) % buffer->size)) = *(u16*)(file->data + audioOffset + 2);

        buffer->head += 4;
        audioOffset  += 4;
    }

    // Wait for audio to finish
    while (true) {
        if ((int)(buffer->head - buffer->tail) <= 0) {
            firstWrite = true;
            break;
        }
        pause();
    }

    printf("DONE clearing audio buffer\n");

    // Audio controller will keep playing audio so we zero it here.
    memset(buffer->data, 0, bufferSize);

    // Sound is done. Free buffers etc.

}


