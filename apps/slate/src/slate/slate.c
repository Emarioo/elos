
#include "slate/slate.h"

#include "prism/prism.h"
#include "elos/syscalls.h"
#include "elos/common/intrinsics.h"
#include "elos/common/string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stdui.h"


extern SlateSession slateSession;



void* slate_font_allocator(Allocator* allocator, u64 size, void* old_ptr) {
    return realloc(old_ptr, size);
}

bool slate_load_font() {
    const char* path = "/boot/STDFONT.PSF";
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

    Allocator allocator = { slate_font_allocator };
    bool res = font__load_from_bytes(data, fileSize, &g_default_font, &allocator);
    return res;
}

typedef struct {
    char* ptr;
    int   len;
    int   max;
} Chunk;

Chunk* chunks;
int    chunks_len;
int    chunks_max;

char* textBuffer;
int   textBuffer_len;
int   textBuffer_max;

void slate_open(SlateSession* session, const char* path) {
    FILE* file;
    char* buffer;

    // printf("slate_open: Opening %s\n", path);

    file = fopen(path, "r");
    if (!file) {
        printf("slate_open: Could not open %s\n", path);
        goto exit;
    }
    fseek(file, 0, SEEK_END);
    int fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    buffer = malloc(fileSize + 1);
    if (!buffer) {
        printf("slate_open: Could not allocate %d for %s\n", fileSize, path);
        goto exit;
    }
    
    int readBytes = fread(buffer, 1, fileSize, file);
    if (readBytes != fileSize) {
        printf("slate_open: Could not read %d from %s\n", fileSize, path);
        goto exit;
    }
    buffer[fileSize] = '\0';

    // Reset buffer that store line data
    reset_chunks();

    session->cursor_x = 0;
    session->cursor_y = 0;
    session->modified = false;
    strncpy(session->currentFile, path, sizeof(session->currentFile));
    session->lines_len = 0;

    if (!session->lines) {
        session->lines_max = 100;
        session->lines = malloc(sizeof(Line) * session->lines_max);
    }

    int head = 0;
    int lineStart = 0;
    while (head < fileSize) {
        char chr = buffer[head];
        char chr_next = 0;
        if (head + 1 < fileSize)
            chr_next = buffer[head + 1];
        head++;
        
        if (chr == '\n' || (chr == '\r' && chr_next == '\n')) {
            int len = head - lineStart - 1;
            if (chr == '\r') {
                head++;
            }
            Line* nextLine = &session->lines[session->lines_len];
            session->lines_len++;

            line_init(nextLine, buffer + lineStart, len);
            lineStart = head;
        }
    }
    {
        int len = head - lineStart;
        Line* nextLine = &session->lines[session->lines_len];
        session->lines_len++;
        line_init(nextLine, buffer + lineStart, len);
        lineStart = head;
    }

    // We use buffer memory in line parts so we can't free this.
    buffer = NULL;

    printf("Opened %s (%d lines)\n", path, session->lines_len);

exit:
    if (buffer) {
        free(buffer);
    }
    if (file) {
        fclose(file);
    }
}

void slate_save(SlateSession* session, const char* path) {
    FILE* file;

    file = fopen(path, "w");
    if (!file) {
        printf("slate_save: Could not open %s\n", path);
        goto exit;
    }

    for (int i=0;i<session->lines_len;i++) {
        Line* line = &session->lines[i];
        // Copy bytes into a buffer and write chunks at a time.
        // Some lines have just a few characters.
        // File system has no internal buffering at the moment.
        int writtenBytes;
        if (line->text.len != 0) {
            writtenBytes = fwrite(line->text.ptr, 1, line->text.len, file);
            if (writtenBytes != line->text.len) {
                printf("slate_save: Could not write %d (%d) bytes to %s\n", line->text.len, writtenBytes, path);
                goto exit;
            }
        }
        char newLine = '\n';
        writtenBytes = fwrite(&newLine, 1, 1, file);
        if (writtenBytes != 1) {
            printf("slate_save: Could not write %d (%d) bytes to %s\n", 1, writtenBytes, path);
            goto exit;
        }
    }

    // @TODO Feedback somewhere
    printf("Saved %s (%d lines)\n", path, session->lines_len);

exit:
    if (file) {
        fclose(file);
    }
}

void reset_chunks() {
    chunks_len = 0;
}

void reserve_chunk(int bytes) {
    if (textBuffer_len + bytes <= textBuffer_max) {
        return;
    }

    // printf("Chunk reserve %d+%d >= %d\n", textBuffer_len, bytes, textBuffer_max);

    if (chunks_len >= chunks_max) {
        int newMax = 4 + 2*chunks_max;
        chunks = realloc(chunks, newMax * sizeof(Chunk));
        ASSERT(chunks);
        chunks_max = newMax;
    }
    
    Chunk* prevChunk = chunks_len > 0 ? &chunks[chunks_len-1] : NULL;
    Chunk* chunk = &chunks[chunks_len];
    chunks_len++;

    // Allocate in page sizes for memory efficiency
    const int PAGE_SIZE = 0x1000;
    int bufferMax = 3 * PAGE_SIZE;
    if (prevChunk) {
        prevChunk->len = textBuffer_len;
        bufferMax = PAGE_SIZE + prevChunk->max * 2;
    }

    chunk->ptr = malloc(bufferMax);
    chunk->max = bufferMax;
    chunk->len = 0;

    textBuffer = chunk->ptr;
    textBuffer_len = chunk->len;
    textBuffer_max = chunk->max;

    ASSERT(textBuffer_len + bytes <= textBuffer_max);
}



void line_init(Line* line, char* ptr, int len) {
    line->text.ptr = ptr;
    line->text.max = 0;
    line->text.len = len;
}

void slate_move(ELOS_Keycode direction) {
    SlateSession* session = &slateSession;

    int lines_per_pageJump = 10;

    Line* cursorLine = &session->lines[session->cursor_y];

    if (direction == KEY_LEFT_ARROW) {
        if (session->cursor_x > 0) {
            session->cursor_x--;
        } else if (session->cursor_y > 0) {
            session->cursor_y--;
            session->cursor_x = session->lines[session->cursor_y].text.len;
        }
    } else if (direction == KEY_RIGHT_ARROW) {
        if (session->cursor_x < cursorLine->text.len) {
            session->cursor_x++;
        } else if (session->cursor_y+1 < session->lines_len) {
            session->cursor_x = 0;
            session->cursor_y++;
        }
    } else if (direction == KEY_DOWN_ARROW) {
        if (session->cursor_y+1 < session->lines_len) {
            session->cursor_y++;
        }
    } else if (direction == KEY_UP_ARROW) {
        if (session->cursor_y > 0) {
            session->cursor_y--;
        }
    } else if (direction == KEY_HOME) {
        session->cursor_x = 0;
    } else if (direction == KEY_END) {
        session->cursor_x = cursorLine->text.len;
    } else if (direction == KEY_PAGE_UP) {
        if (session->cursor_y >= lines_per_pageJump) {
            session->cursor_y -= lines_per_pageJump;
        } else {
            session->cursor_y = 0;
        }
    } else if (direction == KEY_PAGE_DOWN) {
        if (session->cursor_y + lines_per_pageJump < session->lines_len) {
            session->cursor_y += lines_per_pageJump;
        } else {
            session->cursor_y = session->lines_len-1;
        }
    } else {
        printf("slate_move: Unhandled keycode %d\n", direction);
        exit(1);
    }
}

void slate_insert(char c) {
    SlateSession* session = &slateSession;
    session->modified = true;

    Line* line = &session->lines[session->cursor_y];


    if (c == '\n') {
        // split line
        ASSERT(session->lines_len < session->lines_max);
        memmove(&session->lines[session->cursor_y+2], &session->lines[session->cursor_y+1], sizeof(Line) * (session->lines_len - (session->cursor_y+1)));
        session->lines_len++;

        Line* splitLine = &session->lines[session->cursor_y+1];
        line_init(splitLine, line->text.ptr + session->cursor_x, line->text.len - session->cursor_x);

        line->text.len = session->cursor_x;
        session->cursor_x = 0;
        session->cursor_y++;
    } else {
        if (session->cursor_x == line->text.len && line->text.ptr + line->text.len == textBuffer + textBuffer_len) {
            reserve_chunk(1);
            ASSERT(textBuffer_len + 1 <= textBuffer_max);
            
            textBuffer[textBuffer_len] = c;
            line->text.len += 1;
            line->text.max += 1;
            textBuffer_len += 1;
            session->cursor_x++;
        } else {
            reserve_chunk(line->text.len + 1);

            ASSERT(textBuffer_len + line->text.len + 1 <= textBuffer_max);

            memcpy(textBuffer + textBuffer_len, line->text.ptr, session->cursor_x);
            if (session->cursor_x != line->text.len) {
                memcpy(textBuffer + textBuffer_len + session->cursor_x + 1, line->text.ptr + session->cursor_x, line->text.len - session->cursor_x);
            }
            textBuffer[textBuffer_len + session->cursor_x] = c;
            line->text.ptr = textBuffer + textBuffer_len;
            line->text.len += 1;
            line->text.max = line->text.len;
            textBuffer_len += line->text.len;
            session->cursor_x++;
        }
    }

}

void slate_deletion(ELOS_Keycode direction) {
    SlateSession* session = &slateSession;
    session->modified = true;

    if (direction == KEY_BACKSPACE) {
        if (session->cursor_x > 0) {
            Line* line = &session->lines[session->cursor_y];
            memmove(line->text.ptr + session->cursor_x-1, line->text.ptr + session->cursor_x, line->text.len - session->cursor_x);
            line->text.len--;
            session->cursor_x--;
        } else if (session->cursor_y > 0) {
            Line* prevLine = &session->lines[session->cursor_y-1];
            Line* line = &session->lines[session->cursor_y];

            reserve_chunk(prevLine->text.len + line->text.len);
            
            ASSERT(textBuffer_len + prevLine->text.len + line->text.len <= textBuffer_max);
            
            session->cursor_x = prevLine->text.len;
            memcpy(textBuffer + textBuffer_len, prevLine->text.ptr, prevLine->text.len);
            memcpy(textBuffer + textBuffer_len + prevLine->text.len, line->text.ptr, line->text.len);
            prevLine->text.len += line->text.len;
            prevLine->text.ptr = textBuffer + textBuffer_len;
            prevLine->text.max = prevLine->text.len;
            textBuffer_len += prevLine->text.len;

            memmove(&session->lines[session->cursor_y], &session->lines[session->cursor_y+1], sizeof(Line) * (session->lines_len - session->cursor_y+1));
            session->lines_len--;
            session->cursor_y--;
        }
    } else if (direction == KEY_DELETE) {
        Line* line = &session->lines[session->cursor_y];
        if (session->cursor_x < line->text.len) {
            memmove(line->text.ptr + session->cursor_x, line->text.ptr + session->cursor_x+1, line->text.len - (session->cursor_x+1));
            line->text.len--;
        } else if (session->cursor_y+1 < session->lines_len) {
            Line* line = &session->lines[session->cursor_y];
            Line* nextLine = &session->lines[session->cursor_y+1];
            
            reserve_chunk(line->text.len + nextLine->text.len);

            ASSERT(textBuffer_len + line->text.len + nextLine->text.len <= textBuffer_max);

            memcpy(textBuffer + textBuffer_len, line->text.ptr, line->text.len);
            memcpy(textBuffer + textBuffer_len + line->text.len, nextLine->text.ptr, nextLine->text.len);
            line->text.len += nextLine->text.len;
            line->text.ptr = textBuffer + textBuffer_len;
            line->text.max = line->text.len;
            textBuffer_len += line->text.len;

            memmove(&session->lines[session->cursor_y+1], &session->lines[session->cursor_y+2], sizeof(Line) * (session->lines_len - session->cursor_y+2));
            session->lines_len--;
        }
    } else {
        printf("slate_move: Unhandled keycode %d\n", direction);
        exit(1);
    }
}