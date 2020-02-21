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

void print_fds(VPEObj x) {
  for (int i=0;i<x.src.num_buffers;i++)
    MSG("src.v4l2buf[%d].m.planes[0].m.fd = %d", i, x.src.v4l2buf[i].m.planes[0].m.fd);
}


int main() {

  VIPObj vip = VIPObj();
  VPEObj vpe = VPEObj();
  int frame_num = 0, count = 0;
  bool stop_after_one = false;
  // int vpe_out_w = MODEL_WIDTH;
  // int vpe_out_h = MODEL_HEIGHT;
  int vpe_in_w = CAP_WIDTH;
  int vpe_in_h = CAP_HEIGHT;

  // request vip buffers that point to the input buffer of the vpe
  //BufObj bo_vpe_out(vpe_out_w, vpe_out_h, 3, FOURCC_STR("RGB3"), 1, NBUF);
  BufObj bo_vpe_in(vpe_in_w, vpe_in_h, 2, FOURCC_STR("YUYV"), 1, NBUF);
  //BufObj bo_vip_in(vpe_in_w, vpe_in_h, 2, FOURCC_STR("YUYV"), 1, NBUF);


  if(!vip.request_buf(bo_vpe_in.m_fd)) {
    ERROR("VIP buffer requests failed.");
    return -1;
  }
  MSG("Successfully requested VIP buffers\n\n");

  if (!vpe.vpe_input_init(NULL)) {
    ERROR("Input layer initialization failed.");
    return -1;
  }
  MSG("Input layer initialization done\n");

  if(!vpe.vpe_output_init()) {
    ERROR("Output layer initialization failed.");
    return -1;
  }
  MSG("Output layer initialization done\n");

  for (int i=0; i < NBUF; i++) {
    if(!vip.queue_buf(bo_vpe_in.m_fd[i], i)) {
      ERROR(" initial queue VIP buffer #%d failed", i);
      return -1;
    }
  }
  MSG("VIP initial buffer queues done\n");

  for (int i=0; i < NBUF; i++) {
    if(!vpe.output_qbuf(i)) {
      ERROR(" initial queue VPE output buffer #%d failed", i);
      return -1;
    }
  }
  MSG("VPE initial output buffer queues done\n");

  // begin streaming the capture through the VIP
  if (!vip.stream_on()) return -1;

  // begin streaming the output of the VPE
  if (!vpe.stream_on(1)) return -1;

  vpe.m_field = V4L2_FIELD_ANY;
  int num = 0;
  char name[50];
  while(1) {

    frame_num = vip.dequeue_buf(&vpe);

    // sprintf(name, "images/%dvpe_in_800x600data.yuv", num++);
    // MSG("Saving file %s", name);
    // write_binary_file((void *)bo_vpe_in.m_buf[frame_num], name, 768*332*3);

    if (!vpe.input_qbuf(bo_vpe_in.m_fd[frame_num], frame_num)) {
      ERROR("vpe input queue buffer failed");
      return -1;
    }

    if (!stop_after_one) {
			count++;
			for (int i = 1; i <= NBUF; i++) {
				/** To star deinterlace, minimum 3 frames needed */
				if (vpe.m_deinterlace && count != 3) {
					frame_num = vip.dequeue_buf(&vpe);
					vpe.input_qbuf(bo_vpe_in.m_fd[frame_num], frame_num);
				}
        else {
          //begin streaming the input of the vpe
					vpe.stream_on(0);
					stop_after_one = true;
					break;
				}
				count ++;
			}
		}
    frame_num = vpe.output_dqbuf();

    /**********DATA IS HERE!!************/
    void *imagedata = (void *)vpe.dst.base_addr[frame_num];

    vpe.output_qbuf(frame_num);
    frame_num = vpe.input_dqbuf();
    vip.queue_buf(bo_vpe_in.m_fd[frame_num], frame_num);

  }


  return 0;
}
