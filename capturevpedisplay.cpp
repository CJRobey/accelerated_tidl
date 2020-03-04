#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <algorithm>
#include <chrono>
#include "error.h"

#include <linux/videodev2.h>
#include <linux/v4l2-controls.h>

extern "C" {
  #include <omap_drm.h>
  #include <omap_drmif.h>
  #include <xf86drmMode.h>
  #include <linux/dma-buf.h>
}

#include <sys/mman.h>
#include <sys/ioctl.h>
#include "capturevpedisplay.h"
#include "save_utils.h"
using namespace std;
using namespace chrono;

#define FOURCC(a, b, c, d) ((uint32_t)(uint8_t)(a) | \
    ((uint32_t)(uint8_t)(b) << 8) | ((uint32_t)(uint8_t)(c) << 16) | \
    ((uint32_t)(uint8_t)(d) << 24 ))
#define FOURCC_STR(str)    FOURCC(str[0], str[1], str[2], str[3])


CamDisp::CamDisp() {
  /* The VIP and VPE default constructors will be called since they are member
   * variables
   */
  src_w = CAP_WIDTH;
  src_h = CAP_HEIGHT;
  dst_w = TIDL_MODEL_WIDTH;
  dst_h = TIDL_MODEL_HEIGHT;
  frame_num = 0;
  usb = true;
}


CamDisp::CamDisp(int _src_w, int _src_h, int _dst_w, int _dst_h,
  string dev_name, bool usb) {
  src_w = _src_w;
  src_h = _src_h;
  dst_w = _dst_w;
  dst_h = _dst_h;

  if (usb) {
    vip = VIPObj(dev_name, src_w, src_h, FOURCC_STR("YUYV"), 3, V4L2_BUF_TYPE_VIDEO_CAPTURE, V4L2_MEMORY_MMAP);
  }
  else {
    vip = VIPObj(dev_name, src_w, src_h, FOURCC_STR("YUYV"), 3, V4L2_BUF_TYPE_VIDEO_CAPTURE, V4L2_MEMORY_DMABUF);
  }
  vpe = VPEObj(src_w, src_h, 2, V4L2_PIX_FMT_YUYV, dst_w, dst_h, 3, V4L2_PIX_FMT_BGR24, 3);

  frame_num = 0;
}


bool CamDisp::init_capture_pipeline() {

  int num_planes = 2;
  drm_device.drm_init_device(num_planes);
  vip.device_init();
  vpe.open_fd();

  int export_fds[vip.src.num_buffers];
  // Create an "omap_device" from the fd
  struct omap_device *dev = omap_device_new(drm_device.fd);
  bo_vpe_in = (class DmaBuffer **) malloc(vip.src.num_buffers * sizeof(class DmaBuffer *));

  if (!bo_vpe_in) {
    MSG("mem failure, exiting \n");
    exit(EXIT_FAILURE);
  }


  MSG("0x%x", (unsigned int) bo_vpe_in[0]);
  for (int i = 0; i < vip.src.num_buffers; i++) {
      bo_vpe_in[i] = (class DmaBuffer *) malloc(sizeof(class DmaBuffer));
      bo_vpe_in[i]->width = src_w;
      bo_vpe_in[i]->height = src_h;
      bo_vpe_in[i]->fourcc = vip.src.fourcc;

      // allocate space for buffer object (bo)
      bo_vpe_in[i]->bo = (struct omap_bo **) calloc(4, sizeof(omap_bo *));
      // define the object
  		bo_vpe_in[i]->bo[0] = omap_bo_new(dev, src_w*src_h*2, OMAP_BO_SCANOUT | OMAP_BO_WC);
      // give the object a file descriptor for dmabuf v4l2 calls
      bo_vpe_in[i]->fd[0] = omap_bo_dmabuf(bo_vpe_in[i]->bo[0]);
      if (vip.src.memory == V4L2_MEMORY_MMAP) {
        bo_vpe_in[i]->bo_addr = (void **) calloc(4, sizeof(unsigned int));
        bo_vpe_in[i]->bo_addr[0] = omap_bo_map(bo_vpe_in[i]->bo[0]);
      }

      MSG("Exported file descriptor for bo_vpe_in[%d]: %d", i, bo_vpe_in[i]->fd[0]);
      export_fds[i] = bo_vpe_in[i]->fd[0];
  }

  if(!vip.request_export_buf(export_fds)) {
  // if (!vip.request_buf()) {
    ERROR("VIP buffer requests failed.");
    return false;
  }
  MSG("Successfully requested VIP buffers\n\n");

  if (!vpe.vpe_input_init()) {
    ERROR("Input layer initialization failed.");
    return false;
  }
  MSG("Input layer initialization done\n");


  if(!vpe.vpe_output_init()) {
    ERROR("Output layer initialization failed.");
    return false;
  }
  MSG("Output layer initialization done\n");

  for (int i=0; i < vip.src.num_buffers; i++) {
    // for (int p=0;p<vip.src.num_buffers; p++)
    //   MSG("bo_vpe_in[%d]: %d", p, bo_vpe_in[p]->fd[0]);
    if(!vip.queue_buf(bo_vpe_in[i]->fd[0], i)) {
      ERROR("initial queue VIP buffer #%d failed", i);
      return false;
    }
  }
  MSG("VIP initial buffer queues done\n");

  for (int i=0; i < vpe.m_num_buffers; i++) {
    if(!vpe.output_qbuf(i)) {
      ERROR(" initial queue VPE output buffer #%d failed", i);
      return false;
    }
  }
  MSG("VPE initial output buffer queues done\n");

  // begin streaming the capture through the VIP
  if (!vip.stream_on()) return false;
  MSG("Streaming VIP");

  // begin streaming the output of the VPE
  if (!vpe.stream_on(1)) return false;

  vpe.m_field = V4L2_FIELD_ANY;
  drm_device.export_buffer(bo_vpe_in, vpe.m_num_buffers, 2, 0);

  drm_device.get_vid_buffers(3, FOURCC_STR("RA24"), dst_w, dst_h, 4, 1);
  drm_device.drm_init_dss(&vip);



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
      vip.queue_buf(bo_vpe_in[frame_num]->fd[0], frame_num);
    }

    /* dequeue the vip */
    frame_num = vip.dequeue_buf();
    if (vip.src.memory == V4L2_MEMORY_MMAP) {
      memcpy(bo_vpe_in[frame_num]->bo_addr[0], vip.src.base_addr[frame_num], vip.src.size);
    }
    // if no display, then the api is not called

    drm_device.disp_frame(frame_num);

    /* queue that frame onto the vpe */
    if (!vpe.input_qbuf(bo_vpe_in[frame_num]->fd[0], frame_num)) {
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
      frame_num = vip.dequeue_buf();
      vpe.input_qbuf(bo_vpe_in[frame_num]->fd[0], frame_num);
    }
    else {
      /* Begin streaming the input of the vpe */
      vpe.stream_on(0);
      stop_after_one = true;
      return;
    }
  count ++;
  }
}

void CamDisp::turn_off() {
  vip.stream_off();
  vpe.stream_off(1);
  vpe.stream_off(0);
}

/* Testing functionality: To use this, just type "make test-vpe" and then run
 * ./test-vpe <num_frames> <0/1 (to save data)> - Make sure to uncomment the
 * "main" section beforehand if not already done.
 */
//
// int main(int argc, char *argv[]) {
//   int cap_w = 800;
//   int cap_h = 600;
//   int model_w = 768;
//   int model_h = 320;
//
//   // capture w, capture h, output w, output h, device name, is usb?
//   CamDisp cam(cap_w, cap_h, model_w, model_h, "/dev/video2", true);
//
//   // This is the display object
//   DRMDeviceInfo drm_device;
//   drm_device.drm_init_device();
//
//   cam.init_capture_pipeline(drm_device.fd);
//
//   drm_device.export_buffer(cam.bo_vpe_in, cam.vpe.m_num_buffers);
//   auto start = std::chrono::high_resolution_clock::now();
//
//   int num_frames = 300;
//   if (argc > 1){
//     num_frames = stoi(argv[1]);
//   }
//
//   drm_device.drm_init_dss(&cam.vip);
//   for (int i=0; i<num_frames; i++) {
//     if (argc <= 2)
//       cam.grab_image(&drm_device);
//     else
//       save_data(cam.grab_image(&drm_device), model_w, model_h, 3, 3);
//     //drm_device.disp_frame(NULL);
//   }
//   auto stop = std::chrono::high_resolution_clock::now();
//   auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
//   MSG("******************");
//   MSG("Capture at %dx%d\nResized to %dx%d\nFrame rate %f",cap_w, cap_h,
//
//   model_w, model_h, (float) num_frames/((float)duration.count()/1000));
//   MSG("Total time to capture %d frames: %f seconds", num_frames, (float)
//       duration.count()/1000);
//   MSG("******************");
//   cam.turn_off();
// }
