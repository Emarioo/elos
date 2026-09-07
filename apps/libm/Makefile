#
# Makefile to build user app
#

# Parameters to set:

APP_ROOT ?= $(abspath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))).)
ROOT     := $(abspath $(dir $(abspath $(lastword $(MAKEFILE_LIST))))/../..)

# Clone musl here and build it.
# You must build because it generates some headers.
MUSL_DIR := $(ROOT)/../VENDOR/musl

APP_NAME ?= $(notdir $(APP_ROOT))

BIN_DIR    ?= bin
INT_DIR    ?= int/$(APP_NAME)
APP_OUTPUT ?= $(INT_DIR)/$(APP_NAME).a

SILENT ?= @

################################

APP_DIR_NAME := $(notdir $(APP_ROOT))

TOOLCHAIN := 
# TOOLCHAIN := x86_64-elf-
CC := $(TOOLCHAIN)gcc

AR := $(TOOLCHAIN)ar

CFLAGS += -g -fno-stack-protector -fno-plt
# CFLAGS += $(OFLAG)
CFLAGS += -O0
CFLAGS += -Wall -Werror -fshort-wchar  -nostdlib -nostartfiles -nodefaultlibs -Werror=implicit-function-declaration
CFLAGS += -mno-red-zone
CFLAGS += -Wno-multichar -Wno-builtin-declaration-mismatch -Wno-parentheses
CFLAGS += -Wno-unused-variable -Wno-unused-value -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-result -Wno-unused-label
CFLAGS +=  -O2 -fno-align-jumps -fno-align-functions -fno-align-loops -fno-align-labels -fira-region=one -fira-hoist-pressure -freorder-blocks-algorithm=simple -fno-prefetch-loop-arrays -fno-tree-ch -pipe -fomit-frame-pointer -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections -Wno-pointer-to-int-cast -Werror=implicit-function-declaration -Werror=implicit-int -Werror=pointer-sign -Werror=pointer-arith -Werror=int-conversion -Werror=incompatible-pointer-types -Werror=discarded-qualifiers -Werror=discarded-array-qualifiers -Waddress -Warray-bounds -Wchar-subscripts -Wduplicate-decl-specifier -Winit-self -Wreturn-type -Wsequence-point -Wstrict-aliasing -Wunused-function -Wunused-label -Wunused-variable

CFLAGS += -I$(MUSL_DIR)/src/internal -I$(MUSL_DIR)/src/include -I$(MUSL_DIR)/obj/include -I$(MUSL_DIR)/include -I$(MUSL_DIR)/arch/x86_64 -I$(MUSL_DIR)/arch/generic -I$(MUSL_DIR)/src/math -I$(ROOT)/apps/std

# LDFLAGS := -g $(LDFLAGS) -nostdlib -nodefaultlibs -nostartfiles -Wl,--no-warn-execstack
# LDFLAGS := -g $(LDFLAGS) -nostdlib -nodefaultlibs -nostartfiles -Wl,--no-warn-execstack -Bstatic -lm

# SRC_DIRS += \
# 	$(MUSL_DIR)/src/math/x86_64 \
# 	#

# SRC_FILES := \
# 	$(foreach dir,$(SRC_DIRS),$(shell find $(dir) \( -name '*.c' -o -name '*.s' \) ))

# SRC_FILES += $(wildcard $(MUSL_DIR)/src/math/%.c)
SRC_FILES := math/acosf.c math/asinf.c math/atanf.c math/atan2f.c math/ceilf.c math/cosf.c math/fabsf.c math/floorf.c math/logf.c math/log2f.c math/log10f.c math/roundf.c math/powf.c math/sinf.c math/sqrtf.c math/tanf.c \
	math/__cosdf.c math/__sindf.c math/__tandf.c math/__rem_pio2f.c math/__rem_pio2_large.c math/scalbn.c


SRC_FILES := $(addprefix $(MUSL_DIR)/src/,$(SRC_FILES))

OBJ_FILES := $(patsubst $(ROOT)/%.c,$(INT_DIR)/%.o,$(SRC_FILES))
OBJ_FILES := $(patsubst $(ROOT)/%.s,$(INT_DIR)/%.o,$(OBJ_FILES))
DEP_FILES := $(patsubst %.o,%.d,$(OBJ_FILES))

ifeq ($(OS),Windows_NT)
	RM_CMD := cmd /C del /Q
	MKDIR  := cmd /C mkdir
	CP     := cp
else
	RM_CMD := rm -f
	MKDIR  := mkdir -p
	CP     := cp
endif

all: $(APP_OUTPUT)

ifeq (0, $(words $(findstring $(MAKECMDGOALS), clean)))
    -include $(DEP_FILES)
endif


$(INT_DIR):
ifeq ($(OS),Windows_NT)
	$(SILENT) $(MKDIR) $(subst /,\,$(INT_DIR))
else
	$(SILENT) $(MKDIR) $(INT_DIR)
endif

$(INT_DIR)/%.o: $(ROOT)/%.c | $(INT_DIR)
	$(SILENT) mkdir -p $(@D)
	$(SILENT) $(CC) $(CFLAGS) -c -MD -o $@ $<

$(INT_DIR)/%.o: $(ROOT)/%.s | $(INT_DIR)
	$(SILENT) mkdir -p $(@D)
	$(SILENT) $(AS) $(AFLAGS) -c -MD $(patsubst %.o,%.d,$@) -o $@ $<

$(APP_OUTPUT): $(OBJ_FILES) | $(INT_DIR)
	$(SILENT) mkdir -p $(@D)
	$(SILENT) $(AR) rcs $@ $^


clean:
ifeq ($(OS),Windows_NT)
	$(SILENT) $(RM_CMD) $(subst /,\,$(OBJ_FILES) $(DEP_FILES) $(APP_OUTPUT))
else
	$(SILENT) $(RM_CMD) $(OBJ_FILES) $(DEP_FILES) $(APP_OUTPUT)
endif
