
# Parameters to set:

BOOT_APP ?= BOOTX64.EFI

# Declarations

ROOT := $(abspath $(dir $(abspath $(lastword $(MAKEFILE_LIST))))..)

INT_DIR := int

LD := x86_64-w64-mingw32-ld
CC := x86_64-w64-mingw32-gcc

INCLUDE_EFI_DIR := $(ROOT)/extern/efi

CFLAGS := -ggdb -ffreestanding -fno-asynchronous-unwind-tables -fno-exceptions
CFLAGS += -I$(INCLUDE_EFI_DIR) -I$(INCLUDE_EFI_DIR)/x86_64 -I$(INCLUDE_EFI_DIR)/protocol -I$(ROOT)/boot/efi
CFLAGS += -Wall -Werror -fshort-wchar -Werror=implicit-function-declaration
CFLAGS += -Wno-multichar
CFLAGS += -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable

$(info $(ROOT)/boot/efi/*.c)

SRC_FILES := $(wildcard $(ROOT)/boot/efi/*.c )
OBJ_FILES := $(patsubst %.c,%.o,$(SRC_FILES))
DEP_FILES := $(patsubst %.o,%.d,$(OBJ_FILES))

$(info $(SRC_FILES))

# Rules
ifeq (0, $(words $(findstring $(MAKECMDGOALS), clean)))
	-include $(DEP_FILES)
endif

all: $(BOOT_APP)

%.o: %.c
	$(CC) $(CFLAGS) -c -MD -o $@ $<

$(BOOT_APP): $(OBJ_FILES)
	$(LD) -nostdlib --dll --image-base=0 -shared --subsystem 10 -e efi_main -o $@ $^

clean:
	rm $(OBJ_FILES) $(DEP_FILES) $(BOOT_APP)
