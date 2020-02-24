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
#include "capturevpedisplay.h"

#define FOURCC(a, b, c, d) ((uint32_t)(uint8_t)(a) | \
    ((uint32_t)(uint8_t)(b) << 8) | ((uint32_t)(uint8_t)(c) << 16) | \
    ((uint32_t)(uint8_t)(d) << 24 ))
#define FOURCC_STR(str)    FOURCC(str[0], str[1], str[2], str[3])


CamDisp::CamDisp() {
  src_w = CAP_WIDTH;
  src_h = CAP_HEIGHT;
  dst_w = MODEL_WIDTH;
  dst_h = MODEL_HEIGHT;
  // request vip buffers that point to the input buffer of the vpe
  bo_vpe_in = BufObj(src_w, src_h, 2, FOURCC_STR("YUYV"), 1, NBUF);
  frame_num = 0;
}


CamDisp::CamDisp(int _src_w, int _src_h, int _dst_w, int _dst_h) {
  src_w = _src_w;
  src_h = _src_h;
  dst_w = _dst_w;
  dst_h = _dst_h;

  vip = VIPObj("/dev/video1", src_w, src_h, FOURCC_STR("YUYV"), 3, V4L2_BUF_TYPE_VIDEO_CAPTURE);
  vpe = VPEObj(src_w, src_h, 2, V4L2_PIX_FMT_YUYV, dst_w, dst_h, 3, V4L2_PIX_FMT_RGB24, 3);

  // request vip buffers that point to the input buffer of the vpe
  bo_vpe_in = BufObj(src_w, src_h, 2, FOURCC_STR("YUYV"), 1, NBUF);
  frame_num = 0;
}


bool CamDisp::init_capture_pipeline() {

  MSG("Opening vpe");

  if(!vip.request_buf(bo_vpe_in.m_fd)) {
    ERROR("VIP buffer requests failed.");
    return false;
  }
  MSG("Successfully requested VIP buffers\n\n");

  if (!vpe.vpe_input_init(NULL)) {
    ERROR("Input layer initialization failed.");
    return false;
  }
  MSG("Input layer initialization done\n");

  if(!vpe.vpe_output_init()) {
    ERROR("Output layer initialization failed.");
    return false;
  }
  MSG("Output layer initialization done\n");

  for (int i=0; i < NBUF; i++) {
    if(!vip.queue_buf(bo_vpe_in.m_fd[i], i)) {
      ERROR(" initial queue VIP buffer #%d failed", i);
      return false;
    }
  }
  MSG("VIP initial buffer queues done\n");

  for (int i=0; i < NBUF; i++) {
    if(!vpe.output_qbuf(i)) {
      ERROR(" initial queue VPE output buffer #%d failed", i);
      return false;
    }
  }
  MSG("VPE initial output buffer queues done\n");

  // begin streaming the capture through the VIP
  if (!vip.stream_on()) return false;

  // begin streaming the output of the VPE
  if (!vpe.stream_on(1)) return false;

  vpe.m_field = V4L2_FIELD_ANY;
  return true;
}

void *CamDisp::grab_image() {
    /* This step actually releases the frame back into the pipeline, but we
     * don't want to do this until the user calls for another frame. Otherwise,
     * the data pointed to by *imagedata could be corrupted.
     */
    if (stop_after_one) {
      vpe.output_qbuf(frame_num);
      frame_num = vpe.input_dqbuf();
      vip.queue_buf(bo_vpe_in.m_fd[frame_num], frame_num);
    }

    /* dequeue the vip */
    frame_num = vip.dequeue_buf(&vpe);

    /* queue that frame onto the vpe */
    if (!vpe.input_qbuf(bo_vpe_in.m_fd[frame_num], frame_num)) {
      ERROR("vpe input queue buffer failed");
      return NULL;
    }

    /* If this is the first run, initialize deinterlacing (if being used) and
     * start the vpe input streaming
     */
    if (!stop_after_one) {
      init_vpe_stream();
    }

    /* Dequeue the frame of the ready data */
    frame_num = vpe.output_dqbuf();

    /**********DATA IS HERE!!************/
    void *imagedata = (void *)vpe.dst.base_addr[frame_num];

    return imagedata;
}

void CamDisp::init_vpe_stream() {
  int count = 1;
  for (int i = 1; i <= NBUF; i++) {
    /* To star deinterlace, minimum 3 frames needed */
    if (vpe.m_deinterlace && count != 3) {
      frame_num = vip.dequeue_buf(&vpe);
      vpe.input_qbuf(bo_vpe_in.m_fd[frame_num], frame_num);
    }
    else {
      /* Begin streaming the input of the vpe */
      vpe.stream_on(0);
      stop_after_one = true;
    }
  count ++;
  }
}
