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
#include "cmem_buf.h"

#include "save_utils.h"

#define FOURCC(a, b, c, d) ((uint32_t)(uint8_t)(a) | \
    ((uint32_t)(uint8_t)(b) << 8) | ((uint32_t)(uint8_t)(c) << 16) | \
    ((uint32_t)(uint8_t)(d) << 24 ))
#define FOURCC_STR(str)    FOURCC(str[0], str[1], str[2], str[3])

class CamDisp {
public:
  VIPObj vip;
  VPEObj vpe;
  BufObj bo_vpe_in;
  int frame_num;
  int src_w;
  int src_h;
  int dst_w;
  int dst_h;
  bool stop_after_one = false;

  CamDisp();
  CamDisp(int src_w, int src_h, int dst_w, int dst_h);
  bool init_capture_pipeline();
  void *grab_image();
  void init_vpe_stream();

};
