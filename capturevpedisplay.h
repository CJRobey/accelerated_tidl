#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <algorithm>
#include "error.h"

#include <linux/videodev2.h>
#include <linux/v4l2-controls.h>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include "v4l2_obj.h"
#include "disp_obj.h"

#include "save_utils.h"

#define FOURCC(a, b, c, d) ((uint32_t)(uint8_t)(a) | \
    ((uint32_t)(uint8_t)(b) << 8) | ((uint32_t)(uint8_t)(c) << 16) | \
    ((uint32_t)(uint8_t)(d) << 24 ))
#define FOURCC_STR(str)    FOURCC(str[0], str[1], str[2], str[3])

struct dmabuf_buffer {
	uint32_t fourcc, width, height;
	int num_buffer_objects;
	void *cmem_buf;
	struct omap_bo **bo;
	uint32_t pitches[4];
	int fd[4];		/* dmabuf */
	unsigned fb_id;
};

class CamDisp {
public:
  VIPObj vip;
  VPEObj vpe;
  DmaBuffer **bo_vpe_in;
  DRMDeviceInfo drm_device;
  int frame_num;
  int vip_frame_num = -1;
  int src_w;
  int src_h;
  int dst_w;
  int dst_h;
  bool usb = true;
  bool stop_after_one = false;

  CamDisp();
  CamDisp(int src_w, int src_h, int dst_w, int dst_h, std::string dev_name, bool usb);
  bool init_capture_pipeline();
  void *grab_image();
  void disp_frame();
  void init_vpe_stream();
  void turn_off();

};
