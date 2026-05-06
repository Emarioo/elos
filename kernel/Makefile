#
# Makefile to build EFI Application
#

# Parameters to set:
INT_DIR      ?= int
KERNEL_ELF   ?= $(INT_DIR)/kernel.elf
KERNEL_IMAGE ?= $(INT_DIR)/kernel.img

################################

SILENT := @

ROOT := $(abspath $(dir $(abspath $(lastword $(MAKEFILE_LIST))))..)


CC      := gcc
AS      := as
LD      := ld
OBJCOPY := objcopy



CFLAGS := -ggdb -ffreestanding
CFLAGS += -I$(ROOT)/boot/include -I$(ROOT)/kernel/src -I$(ROOT)/kernel/include
CFLAGS += -fshort-wchar -Werror=implicit-function-declaration
CFLAGS += -fno-asynchronous-unwind-tables -fno-exceptions
CFLAGS += -Wall -Werror -Wno-multichar
CFLAGS += -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable

ASFLAGS := -g

LDFLAGS := -nostdlib  -T $(ROOT)/kernel/sections.ld --no-warn-execstack

SRC_FILES := $(shell find $(ROOT)/kernel/src/elos -name '*.c') $(wildcard $(ROOT)/kernel/*.s)
SRC_FILES +=$(INT_DIR)/interrupts.s

OBJ_FILES := $(patsubst $(ROOT)/kernel/%.c,$(INT_DIR)/%.o,$(SRC_FILES))
OBJ_FILES := $(patsubst $(ROOT)/kernel/%.s,$(INT_DIR)/%.o,$(OBJ_FILES))
OBJ_FILES := $(patsubst $(INT_DIR)/%.s,$(INT_DIR)/%.o,$(OBJ_FILES))

OBJ_FILES += $(INT_DIR)/default_font.o

DEP_FILES := $(patsubst %.o,%.d,$(OBJ_FILES))

# $(info $(SRC_FILES))

all: $(KERNEL_IMAGE) $(KERNEL_ELF)

ifeq (0, $(words $(findstring $(MAKECMDGOALS), clean)))
    -include $(DEP_FILES)
endif


$(INT_DIR):
	$(SILENT) mkdir $(INT_DIR)

$(INT_DIR)/%.o: $(ROOT)/kernel/%.c | $(INT_DIR)
	$(SILENT) mkdir -p $(@D)
	$(SILENT) $(CC) $(CFLAGS) -c -MD -o $@ $<

$(INT_DIR)/%.o: $(INT_DIR)/%.c | $(INT_DIR)
	$(SILENT) $(CC) $(CFLAGS) -c -MD -o $@ $<
	
$(INT_DIR)/%.o: $(ROOT)/kernel/%.s | $(INT_DIR)
	$(SILENT) $(AS) $(ASFLAGS) -c -MD $(patsubst %.o,%.d,$@) -o $@ $<

$(KERNEL_ELF): $(OBJ_FILES) $(ROOT)/kernel/sections.ld | $(INT_DIR)
	$(SILENT) $(LD) $(LDFLAGS) -o $@ $(OBJ_FILES)
	
$(KERNEL_IMAGE): $(KERNEL_ELF) | $(INT_DIR)
	$(SILENT) $(OBJCOPY) -O binary $< $@

$(INT_DIR)/default_font.o: $(ROOT)/res/Lat2-Terminus16.psf | $(INT_DIR)
	$(SILENT) cd $(ROOT)/res && $(LD) -r -b binary -o $(INT_DIR)/default_font.trash Lat2-Terminus16.psf && \
		$(OBJCOPY) --redefine-sym _binary_Lat2_Terminus16_psf_end=__default_font_end \
        --redefine-sym _binary_Lat2_Terminus16_psf_start=__default_font_start \
        $(INT_DIR)/default_font.trash $@

$(INT_DIR)/interrupts.s: $(ROOT)/kernel/gen_interrupts.py | $(INT_DIR)
	$(SILENT) $(ROOT)/kernel/gen_interrupts.py $@


clean:
	$(SILENT) rm $(OBJ_FILES) $(DEP_FILES) $(BOOT_APP)
