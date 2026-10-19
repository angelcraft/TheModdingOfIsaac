.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

TARGET := TheModdingOfIsaacLuaPOC
BUILD := build
SOURCES := source
INCLUDES := third_party/lua51-switch/src
APP_TITLE := The Modding of Isaac - Lua POC
APP_AUTHOR := The Modding of Isaac
APP_VERSION := 0.1

ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS := -g -Wall -O2 -ffunction-sections -fdata-sections $(ARCH)
CFLAGS += -D__SWITCH__ $(INCLUDE)
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions
ASFLAGS := -g $(ARCH)
LDFLAGS = -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS := -llua -lm -lnx
LIBDIRS := third_party/lua51-switch/src $(PORTLIBS) $(LIBNX)
