#
# Makefile to build EFI Application
#

# Parameters to set:
INT_DIR  ?= int

ifeq ($(OS),Windows_NT)
  NETBOOT_APP ?= $(INT_DIR)/netboot.exe
else
  NETBOOT_APP ?= $(INT_DIR)/netboot
endif

################################

SILENT := 

ROOT := $(abspath $(dir $(abspath $(lastword $(MAKEFILE_LIST))))../..)

CC := gcc

CFLAGS := -ggdb -ffreestanding -fno-asynchronous-unwind-tables -fno-exceptions
CFLAGS += -I$(ROOT)/boot/include
CFLAGS += -Wall -Werror -fshort-wchar -Werror=implicit-function-declaration
CFLAGS += -Wno-multichar
CFLAGS += -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable

LDFLAGS += -g

SRC_FILES := $(wildcard $(ROOT)/boot/netboot_server/*.c)
OBJ_FILES := $(patsubst $(ROOT)/boot/netboot_server/%.c,$(INT_DIR)/%.o,$(SRC_FILES))
DEP_FILES := $(patsubst %.o,%.d,$(OBJ_FILES))

ifeq ($(OS),Windows_NT)
	RM_CMD := cmd /c del /Q
	LDFLAGS += -lws2_32
else
	RM_CMD := rm
endif

all: $(NETBOOT_APP)

ifeq (0, $(words $(findstring $(MAKECMDGOALS), clean)))
    -include $(DEP_FILES)
endif


$(INT_DIR):
	$(SILENT) mkdir $(INT_DIR)

$(INT_DIR)/%.o: $(ROOT)/boot/netboot_server/%.c | $(INT_DIR)
	$(SILENT) $(CC) $(CFLAGS) -c -MD -o $@ $<

$(NETBOOT_APP): $(OBJ_FILES) | $(INT_DIR)
	$(SILENT) $(CC) -o $@ $^ $(LDFLAGS)

clean:
ifeq ($(OS),Windows_NT)
	$(SILENT) $(RM_CMD) $(subst /,\,$(OBJ_FILES) $(DEP_FILES) $(NETBOOT_APP))
else
	$(SILENT) $(RM_CMD) $(OBJ_FILES) $(DEP_FILES) $(NETBOOT_APP)
endif
