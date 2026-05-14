#
# Makefile to build EFI Application
#

# Parameters to set:
INT_DIR  ?= int/boot
BOOT_EFI ?= $(INT_DIR)/BOOTX64.EFI

################################

SILENT := @

ROOT := $(abspath $(dir $(abspath $(lastword $(MAKEFILE_LIST))))..)

LD := x86_64-w64-mingw32-ld
CC := x86_64-w64-mingw32-gcc

INCLUDE_EFI_DIR := $(ROOT)/extern/efi

CFLAGS := -ggdb -ffreestanding -fno-asynchronous-unwind-tables -fno-exceptions
CFLAGS += -I$(INCLUDE_EFI_DIR) -I$(INCLUDE_EFI_DIR)/x86_64 -I$(INCLUDE_EFI_DIR)/protocol -I$(ROOT)/boot/efi \
	-I$(ROOT)/kernel/include -I$(ROOT)/kernel/src
CFLAGS += -Wall -Werror -fshort-wchar -Werror=implicit-function-declaration
CFLAGS += -Wno-multichar
CFLAGS += -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable

LDFLAGS := -nostdlib --dll --image-base=0 -shared --subsystem 10 -e efi_main

SRC_DIRS := \
	$(ROOT)/boot/efi \
	$(ROOT)/kernel/src/elos/kernel/net \
	$(ROOT)/kernel/src/elos/kernel/driver \
	$(ROOT)/kernel/src/elos/kernel/cfg \
	$(ROOT)/kernel/src/elos/common \

SRC_FILES := $(foreach dir,$(SRC_DIRS),$(shell find $(dir) -name '*.c'))
OBJ_FILES := $(patsubst $(ROOT)/%.c,$(INT_DIR)/%.o,$(SRC_FILES))
DEP_FILES := $(patsubst %.o,%.d,$(OBJ_FILES))

all: $(BOOT_EFI)

ifeq (0, $(words $(findstring $(MAKECMDGOALS), clean)))
    -include $(DEP_FILES)
endif

$(INT_DIR)/%.o: $(ROOT)/%.c
	$(SILENT) mkdir -p $(dir $@)
	$(SILENT) $(CC) $(CFLAGS) -c -MD -o $@ $<

$(BOOT_EFI): $(OBJ_FILES)
	$(SILENT) mkdir -p $(dir $@)
	$(SILENT) $(LD) $(LDFLAGS) -o $@ $^

clean:
	@# Should be $(INT_DIR) not 'int' but i'm afraid $(INT_DIR) will be / or . for whatever reason
	$(SILENT) rm -rf int
