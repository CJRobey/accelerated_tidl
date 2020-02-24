# include ../make.common
#
# CROSS_COMPILE ?=
#
# ALL_DEBUG := -g3 -gdwarf-2 -Wall
# ALL_RELEASE := -O2 -fomit-frame-pointer -Wall
# INCLUDES := -I$(SDK_PATH_TARGET)/usr/include/omap -I$(SDK_PATH_TARGET)/usr/include/libdrm
# #DEFS := -DLIBYUV_NEON
#
# LIBS 		+= -ldrm -ldrm_omap -lpthread -lticmem
# LIBS     += -lopencv_highgui -lopencv_imgcodecs -lopencv_videoio\
# 			-lopencv_imgproc -lopencv_core
# LIBS     += -ljson-c
#
# SOURCES = main.cpp ../common/object_classes.cpp ../common/utils.cpp \
# 	../common/video_utils.cpp #vip_obj.cpp vpe_obj.cpp cmem_buf.cpp save_utils.cpp
#
#
# ALL_CFLAGS := $(ALL_RELEASE) $(DEFS) $(INCLUDES) $(CFLAGS)
# ALL_CXXFLAGS := $(ALL_RELEASE) $(DEFS) $(INCLUDES) $(CFLAGS)
# LDFLAGS :=
#
# all: vpe_ssd_multibox
#
# vpe_ssd_multibox: $(TIDL_API_LIB) $(HEADERS) $(SOURCES)
# 	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SOURCES) $(LIBS) -o $@


# Copyright (c) 2018 Texas Instruments Incorporated - http://www.ti.com/
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
# * Neither the name of Texas Instruments Incorporated nor the
# names of its contributors may be used to endorse or promote products
# derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

include ../make.common

LIBS     += -lopencv_highgui -lopencv_imgcodecs -lopencv_videoio\
			-lopencv_imgproc -lopencv_core -lticmem
LIBS     += -ljson-c

SOURCES = main.cpp ../common/object_classes.cpp ../common/utils.cpp \
	../common/video_utils.cpp vip_obj.cpp vpe_obj.cpp cmem_buf.cpp capturevpedisplay.cpp

REALTIME_SOURCES = main-multithread.cpp ../common/object_classes.cpp ../common/utils.cpp \
	../common/video_utils.cpp

all: ssd_multibox ssd_multibox_realtime

ssd_multibox: $(TIDL_API_LIB) $(HEADERS) $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) $(TIDL_API_LIB) $(LDFLAGS) $(LIBS) -o $@

ssd_multibox_realtime: $(TIDL_API_LIB) $(HEADERS) $(SOURCES)
	$(CXX) $(CXXFLAGS) $(REALTIME_SOURCES) $(TIDL_API_LIB) $(LDFLAGS) $(LIBS) -o $@

clean::
	$(RM) -f frame_*.png multibox_*.png
