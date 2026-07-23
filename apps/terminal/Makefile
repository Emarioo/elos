
APP_ROOT := $(abspath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))).)

CFLAGS   += -I$(ROOT)/apps/prism/include
SRC_DIRS += $(ROOT)/apps/prism/src/prism_client


include $(APP_ROOT)/../template/Makefile
