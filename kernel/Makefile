#
# Makefile to build ELOS Kernel
#

# Parameters to set:
INT_DIR      ?= int/kernel
KERNEL_ELF   ?= $(INT_DIR)/kernel.elf
KERNEL_IMAGE ?= $(INT_DIR)/kernel.img

SILENT       ?= @

################################


ROOT := $(abspath $(dir $(abspath $(lastword $(MAKEFILE_LIST))))..)


CC      := gcc
AS      := as
LD      := ld
OBJCOPY := objcopy


CFLAGS := -ggdb -ffreestanding
CFLAGS += -I$(ROOT)/kernel/src -I$(ROOT)/kernel/include
CFLAGS += -fshort-wchar -Werror=implicit-function-declaration
CFLAGS += -fno-asynchronous-unwind-tables -fno-exceptions
CFLAGS += -Wall -Werror -Wno-multichar
CFLAGS += -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-label
CFLAGS += -mno-red-zone
# CFLAGS += -mno-sse -mno-sse2 -mno-mmx # For debugging disabling SSE can be useful

ASFLAGS := -g

KERNEL_LD_SCRIPT := $(ROOT)/kernel/sections.ld

LDFLAGS := -nostdlib  -T $(KERNEL_LD_SCRIPT) --no-warn-execstack

SRC_DIRS := \
	$(ROOT)/kernel/src/elos \
	$(ROOT)/kernel/src/super_user

SRC_FILES := \
	$(foreach dir,$(SRC_DIRS),$(shell find $(dir) \( -name '*.c' -o -name '*.s' \) )) \
	$(INT_DIR)/interrupts.s

OBJ_FILES := \
	$(patsubst $(ROOT)/%.c,$(INT_DIR)/%.o,$(SRC_FILES)) \
	$(INT_DIR)/default_font.o

OBJ_FILES := $(patsubst $(ROOT)/%.s,$(INT_DIR)/%.o,$(OBJ_FILES))
OBJ_FILES := $(patsubst $(INT_DIR)/%.s,$(INT_DIR)/%.o,$(OBJ_FILES))

DEP_FILES := $(patsubst %.o,%.d,$(OBJ_FILES))


all: $(KERNEL_IMAGE) $(KERNEL_ELF)

ifeq (0, $(words $(findstring $(MAKECMDGOALS), clean)))
    -include $(DEP_FILES)
endif


$(INT_DIR):
	$(SILENT) mkdir -p $@

$(INT_DIR)/interrupts.s: $(ROOT)/kernel/gen_interrupts.py | $(INT_DIR)
	$(SILENT) $(ROOT)/kernel/gen_interrupts.py $@
	$(SILENT) chmod -w $@ 2>/dev/null || true # This will fail in WSL on NTFS paths

$(INT_DIR)/%.o: $(ROOT)/%.c | $(INT_DIR)
	$(SILENT) mkdir -p $(@D)
	$(SILENT) $(CC) $(CFLAGS) -c -MD -o $@ $<

$(INT_DIR)/%.o: $(INT_DIR)/%.c | $(INT_DIR)
	$(SILENT) mkdir -p $(@D)
	$(SILENT) $(CC) $(CFLAGS) -c -MD -o $@ $<
	
$(INT_DIR)/%.o: $(ROOT)/%.s | $(INT_DIR)
	$(SILENT) mkdir -p $(@D)
	$(SILENT) $(AS) $(ASFLAGS) -c -MD $(patsubst %.o,%.d,$@) -o $@ $<

$(INT_DIR)/%.o: $(INT_DIR)/%.s | $(INT_DIR)
	$(SILENT) mkdir -p $(@D)
	$(SILENT) $(AS) $(ASFLAGS) -c -MD $(patsubst %.o,%.d,$@) -o $@ $<

$(INT_DIR)/default_font.o: $(ROOT)/res/Lat2-Terminus16.psf | $(INT_DIR)
	$(SILENT) cd $(ROOT)/res && $(LD) -r -b binary -o $(INT_DIR)/default_font.trash Lat2-Terminus16.psf && \
		$(OBJCOPY) --redefine-sym _binary_Lat2_Terminus16_psf_end=__default_font_end \
        --redefine-sym _binary_Lat2_Terminus16_psf_start=__default_font_start \
        $(INT_DIR)/default_font.trash $@

$(KERNEL_ELF): $(OBJ_FILES) $(KERNEL_LD_SCRIPT) | $(INT_DIR)
	$(SILENT) mkdir -p $(@D)
	$(SILENT) $(LD) $(LDFLAGS) -o $@ $(OBJ_FILES)
	
$(KERNEL_IMAGE): $(KERNEL_ELF) | $(INT_DIR)
	$(SILENT) mkdir -p $(@D)
	$(SILENT) $(OBJCOPY) -O binary $< $@


clean:
	$(SILENT) rm -rf int
