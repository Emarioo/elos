
APP_ROOT := $(abspath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))).)

CFLAGS   += -I$(ROOT)/apps/prism/include
SRC_DIRS += $(ROOT)/apps/prism/src/prism_client
SRC_DIRS += $(ROOT)/apps/std_32

CFLAGS += -m32
AFLAGS += --32
LDFLAGS += -m32

include $(APP_ROOT)/../template/Makefile
