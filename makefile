CROSS_COMPILE ?=

DEBUG = -DDEBUG
#CFLAGS += $(DEBUG) -march=armv7-a -D__ARM_NEON__ -mtune=cortex-a15 -mfpu=neon -ftree-vectorize
CFLAGS += $(DEBUG) -D__ARM_NEON__ -mfpu=neon -ftree-vectorize
CC := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++

ALL_DEBUG := -g3 -gdwarf-2 -Wall
ALL_RELEASE := -O2 -fomit-frame-pointer -Wall
INCLUDES := -I$(SDK_PATH_TARGET)/usr/include/omap -I$(SDK_PATH_TARGET)/usr/include/libdrm
#DEFS := -DLIBYUV_NEON

ALL_CFLAGS := $(ALL_RELEASE) $(DEFS) $(INCLUDES) $(CFLAGS)
ALL_CXXFLAGS := $(ALL_RELEASE) $(DEFS) $(INCLUDES) $(CFLAGS)
LDFLAGS :=

capturevpedisplay: capturevpedisplay.cpp vip_obj.cpp vpe_obj.cpp cmem_buf.cpp save_utils.cpp
	$(CXX) $(ALL_CFLAGS) -o $@ $^ -ldrm -ldrm_omap -lpthread -lticmem -lopencv_highgui -lopencv_imgcodecs -lopencv_imgproc -lopencv_core
