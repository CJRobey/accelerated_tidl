#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include "error.h"

#include <linux/videodev2.h>
#include <linux/v4l2-controls.h>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include "v4l2_obj.h"
#include "cmem_buf.h"

#define FOURCC(a, b, c, d) ((uint32_t)(uint8_t)(a) | \
    ((uint32_t)(uint8_t)(b) << 8) | ((uint32_t)(uint8_t)(c) << 16) | \
    ((uint32_t)(uint8_t)(d) << 24 ))
#define FOURCC_STR(str)    FOURCC(str[0], str[1], str[2], str[3])

int main() {

  VIPObj vip = VIPObj();
  VPEObj vpe = VPEObj();

  int vpe_out_w = MODEL_WIDTH;
  int vpe_out_h = MODEL_HEIGHT;
  int vpe_in_w = CAP_WIDTH;
  int vpe_in_h = CAP_HEIGHT;

  // request vip buffers that point to the input buffer of the vpe
  BufObj  bo_vpe_out(vpe_out_w, vpe_out_h, 2, FOURCC_STR("RGB3"), 1, NBUF);
  BufObj  bo_vpe_in(vpe_in_w, vpe_in_h, 2, FOURCC_STR("YUYV"), 1, NBUF);

  vip.request_buf(bo_vpe_in.m_fd);
  MSG("requested buffers\n\n");

  if(!vpe.vpe_input_init(bo_vpe_in.m_fd)) {
    ERROR("Input layer initialization failed.");
    return -1;
  }
  MSG("Input layer initialization done\n");

  //TODO: The VPE allocates cmem buffers
  //TODO: vip.request_buf(fd_of_cmem buffers)

  if(!vpe.vpe_output_init(bo_vpe_out)) {
    ERROR("Output layer initialization failed.");
    return -1;
  }

  // vip queue buf (which is the input buf of the vpe)
  // vpe queue the output buf

  // for testing
  for (int i=0; i < NBUF; i++)
    vip.queue_buf(vpe.m_fd);

  for (int i=0; i < NBUF; i++)
    vpe.output_qbuf(i);

  vip.stream_on();

  return 0;
}
