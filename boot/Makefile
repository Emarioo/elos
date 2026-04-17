#
# Makefile to build EFI Application
#

# Parameters to set:
INT_DIR  ?= int
BOOT_EFI ?= $(INT_DIR)/BOOTX64.EFI

################################

SILENT := @

ROOT := $(abspath $(dir $(abspath $(lastword $(MAKEFILE_LIST))))..)

LD := x86_64-w64-mingw32-ld
CC := x86_64-w64-mingw32-gcc

INCLUDE_EFI_DIR := $(ROOT)/extern/efi

CFLAGS := -ggdb -ffreestanding -fno-asynchronous-unwind-tables -fno-exceptions
CFLAGS += -I$(INCLUDE_EFI_DIR) -I$(INCLUDE_EFI_DIR)/x86_64 -I$(INCLUDE_EFI_DIR)/protocol -I$(ROOT)/boot/efi \
	-I$(ROOT)/boot/include -I$(ROOT)/kernel/src
CFLAGS += -Wall -Werror -fshort-wchar -Werror=implicit-function-declaration
CFLAGS += -Wno-multichar
CFLAGS += -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable

LDFLAGS := -nostdlib --dll --image-base=0 -shared --subsystem 10 -e efi_main

SRC_FILES := $(wildcard $(ROOT)/boot/efi/*.c)
SRC_FILES += $(wildcard $(ROOT)/kernel/src/elos/common/*.c)
OBJ_FILES := $(patsubst $(ROOT)/boot/efi/%.c,$(INT_DIR)/%.o,$(SRC_FILES))
OBJ_FILES := $(patsubst $(ROOT)/kernel/src/elos/common/%.c,$(INT_DIR)/%.o,$(OBJ_FILES))
DEP_FILES := $(patsubst %.o,%.d,$(OBJ_FILES))


all: $(BOOT_EFI)

ifeq (0, $(words $(findstring $(MAKECMDGOALS), clean)))
    -include $(DEP_FILES)
endif


$(INT_DIR):
	$(SILENT) mkdir $(INT_DIR)

$(INT_DIR)/%.o: $(ROOT)/boot/efi/%.c | $(INT_DIR)
	$(SILENT) $(CC) $(CFLAGS) -c -MD -o $@ $<

$(INT_DIR)/%.o: $(ROOT)/kernel/src/elos/common/%.c | $(INT_DIR)
	$(SILENT) $(CC) $(CFLAGS) -c -MD -o $@ $<

$(BOOT_EFI): $(OBJ_FILES) | $(INT_DIR)
	$(SILENT) $(LD) $(LDFLAGS) -o $@ $^

clean:
	$(SILENT) rm $(OBJ_FILES) $(DEP_FILES) $(BOOT_EFI)
