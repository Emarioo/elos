#
# Makefile to build EFI Application
#

# Parameters to set:

APPNAME := supper

ifeq ($(OS),Windows_NT)
  INT_DIR    ?= int/$(APPNAME)_server_windows
  APP_OUTPUT ?= bin/$(APPNAME).exe
else
  INT_DIR    ?= int/$(APPNAME)_linux
  APP_OUTPUT ?= bin/$(APPNAME)
endif

SILENT ?= @

################################


ROOT := $(abspath $(dir $(abspath $(lastword $(MAKEFILE_LIST))))..)

APP_SRC := $(abspath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))

CC := gcc

CFLAGS := -ggdb -ffreestanding -fno-asynchronous-unwind-tables -fno-exceptions -fno-stack-protector
CFLAGS += -I$(ROOT)/kernel/include
CFLAGS += -Wall -Werror -fshort-wchar -Werror=implicit-function-declaration
CFLAGS += -Wno-multichar
CFLAGS += -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable

LDFLAGS += -g

SRC_FILES := $(wildcard $(APP_SRC)/*.c)
OBJ_FILES := $(patsubst $(APP_SRC)/%.c,$(INT_DIR)/%.o,$(SRC_FILES))
DEP_FILES := $(patsubst %.o,%.d,$(OBJ_FILES))

# $(info $(OBJ_FILES) $(SRC_FILES))

ifeq ($(OS),Windows_NT)
	RM_CMD := cmd /C del /Q
	MKDIR  := cmd /C mkdir
	LDFLAGS += -lws2_32
else
	RM_CMD := rm
	MKDIR  := mkdir -p
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

$(INT_DIR)/%.o: $(APP_SRC)/%.c | $(INT_DIR)
	$(SILENT) $(CC) $(CFLAGS) -c -MD -o $@ $<

$(APP_OUTPUT): $(OBJ_FILES) | $(INT_DIR)
	$(SILENT) $(CC) -o $@ $^ $(LDFLAGS)

clean:
ifeq ($(OS),Windows_NT)
	$(SILENT) $(RM_CMD) $(subst /,\,$(OBJ_FILES) $(DEP_FILES) $(APP_OUTPUT))
else
	$(SILENT) $(RM_CMD) $(OBJ_FILES) $(DEP_FILES) $(APP_OUTPUT)
endif
