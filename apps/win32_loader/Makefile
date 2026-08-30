
APP_ROOT := $(abspath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))).)

CFLAGS   += -I$(ROOT)/apps/prism/include
SRC_DIRS += $(ROOT)/apps/prism/src/prism_client
SRC_DIRS += $(ROOT)/apps/std_32

CFLAGS += -m32
AFLAGS += --32
LDFLAGS += -m32

include $(APP_ROOT)/../template/Makefile



# ROOT       := $(abspath $(dir $(abspath $(lastword $(MAKEFILE_LIST))))/../..)

APP_NAME2   := wintest
INT_DIR2    ?= int/$(APP_NAME)
APP_OUTPUT2 ?= $(INT_DIR)/$(APP_NAME).exe
TOOLCHAIN2   ?= i686-w64-mingw32-

wintest: $(APP_OUTPUT2)

$(APP_OUTPUT2): $(APP_ROOT)/wintest.c $(APP_ROOT)/winlayer.c
	$(SILENT) mkdir -p $(INT_DIR2) $(@D) > /dev/null
	$(SILENT) $(TOOLCHAIN2)gcc -shared $(APP_ROOT)/winlayer.c -o $(INT_DIR2)/libwinlayer.so -I $(ROOT)/kernel/include -I $(ROOT)/kernel/src -Wl,--out-implib,$(INT_DIR2)/libwinlayer.a
	$(SILENT) $(TOOLCHAIN2)gcc -o $(APP_OUTPUT2) -nostdlib -nodefaultlibs -nostartfiles  $(APP_ROOT)/wintest.c -L$(INT_DIR2) -lwinlayer
