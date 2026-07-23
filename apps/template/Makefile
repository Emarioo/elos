#
# Makefile to build user app
#

# Parameters to set:

APP_ROOT ?= $(abspath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))).)
ROOT     := $(abspath $(dir $(abspath $(lastword $(MAKEFILE_LIST))))/../..)

APP_NAME ?= $(notdir $(APP_ROOT))

BIN_DIR    ?= bin
INT_DIR    ?= int/$(APP_NAME)
APP_OUTPUT ?= $(INT_DIR)/$(APP_NAME).elf

SILENT ?= @

################################

APP_DIR_NAME := $(notdir $(APP_ROOT))

CC := gcc

CFLAGS += -g -fno-stack-protector -fno-plt
# CFLAGS += $(OFLAG)
CFLAGS += -O0
CFLAGS += -Wall -Werror -fshort-wchar -Werror=implicit-function-declaration
CFLAGS += -mno-red-zone
CFLAGS += -Wno-multichar -Wno-builtin-declaration-mismatch
CFLAGS += -Wno-unused-variable -Wno-unused-value -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-result -Wno-unused-label

CFLAGS += -I$(ROOT)/kernel/include -I$(ROOT)/kernel/src -I$(APP_ROOT)/include -I$(ROOT)/apps/std

LDFLAGS := -g $(LDFLAGS) -nostdlib -nostartfiles -Wl,--no-warn-execstack

SRC_DIRS += \
	$(APP_ROOT)/src/$(APP_DIR_NAME) \
	$(ROOT)/kernel/src/elos/common \
	$(ROOT)/apps/std \
	#

SRC_FILES := \
	$(foreach dir,$(SRC_DIRS),$(shell find $(dir) \( -name '*.c' -o -name '*.s' \) ))

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
	$(SILENT) $(AS) $(ASFLAGS) -c -MD $(patsubst %.o,%.d,$@) -o $@ $<

$(APP_OUTPUT): $(OBJ_FILES) | $(INT_DIR)
	$(SILENT) mkdir -p $(@D)
	$(SILENT) $(CC) -o $@ $^  $(LDFLAGS)


clean:
ifeq ($(OS),Windows_NT)
	$(SILENT) $(RM_CMD) $(subst /,\,$(OBJ_FILES) $(DEP_FILES) $(APP_OUTPUT))
else
	$(SILENT) $(RM_CMD) $(OBJ_FILES) $(DEP_FILES) $(APP_OUTPUT)
endif
