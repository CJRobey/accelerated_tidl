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
  int frame_num = 0, count = 0;
  bool stop_after_one = false;
  // int vpe_out_w = MODEL_WIDTH;
  // int vpe_out_h = MODEL_HEIGHT;
  // int vpe_in_w = CAP_WIDTH;
  // int vpe_in_h = CAP_HEIGHT;

  // request vip buffers that point to the input buffer of the vpe
  //BufObj bo_vpe_out(vpe_out_w, vpe_out_h, 3, FOURCC_STR("RGB3"), 1, NBUF);
  //BufObj bo_vpe_in(vpe_in_w, vpe_in_h, 2, FOURCC_STR("YUYV"), 1, NBUF);


  if(!vip.request_buf()) {
    ERROR("VIP buffer requests failed.");
    return -1;
  }
  MSG("Successfully requested VIP buffers\n\n");

  if (!vpe.vpe_input_init()) {
    ERROR("Input layer initialization failed.");
    return -1;
  }
  MSG("Input layer initialization done\n");


  //TODO: The VPE allocates cmem buffers
  //TODO: vip.request_buf(fd_of_cmem buffers)

  if(!vpe.vpe_output_init()) {
    ERROR("Output layer initialization failed.");
    return -1;
  }
  MSG("Output layer initialization done\n");

  // for testing
  for (int i=0; i < NBUF; i++)
    vip.queue_buf(i);

  for (int i=0; i < NBUF; i++)
    vpe.output_qbuf(i);

  if (!vip.stream_on()) return -1;
  if (!vpe.stream_on(1)) return -1;

  while(1) {

    frame_num = vip.dequeue_buf();
    MSG("vip dq done");
    if (!vpe.input_qbuf(frame_num)) {
      ERROR("input qbuf failed");
      return -1;
    }

    MSG("Done with vip dq and vpe_in_qbuf");
    if (!stop_after_one) {
			count++;
			for (int i = 1; i <= NBUF; i++) {
				/** To star deinterlace, minimum 3 frames needed */
				if (vpe.m_deinterlace && count != 3) {
					frame_num = vip.dequeue_buf();
					vpe.input_qbuf(frame_num);
				}
        else {
					vpe.stream_on(0);
					stop_after_one = true;
					break;
				}
				count ++;
			}
		}
    frame_num = vpe.output_dqbuf();
    vpe.output_qbuf(frame_num);
    frame_num = vpe.input_dqbuf();
    vip.queue_buf(frame_num);

  }


  return 0;
}
