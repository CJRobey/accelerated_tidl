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
  alpha = 255;
  frame_num = 0;
  usb = true;
}


CamDisp::CamDisp(int _src_w, int _src_h, int _dst_w, int _dst_h, int _alpha,
  string dev_name, bool usb, std::string net_type) {
  src_w = _src_w;
  src_h = _src_h;
  dst_w = _dst_w;
  dst_h = _dst_h;
  alpha = _alpha;
  if (usb) {
    vip = VIPObj(dev_name, src_w, src_h, FOURCC_STR("YUYV"), 3,
    V4L2_BUF_TYPE_VIDEO_CAPTURE, V4L2_MEMORY_MMAP);
  }
  else {
    vip = VIPObj(dev_name, src_w, src_h, FOURCC_STR("YUYV"), 3,
    V4L2_BUF_TYPE_VIDEO_CAPTURE, V4L2_MEMORY_DMABUF);
  }
  int vpe_src_bytes_pp = 2;
  int vpe_dst_bytes_pp = 4;

  vpe = VPEObj(src_w, src_h, vpe_src_bytes_pp, FOURCC_STR("YUYV"),
    V4L2_MEMORY_DMABUF, dst_w, dst_h, vpe_dst_bytes_pp, FOURCC_STR("AR24"),
    V4L2_MEMORY_DMABUF, 3);

  frame_num = 0;
}


bool CamDisp::init_capture_pipeline(string net_type) {

  /* set num_planes to 1 for no output layer and num_planes to 2 for the output
   * layer to be shown
   */
  int num_planes = 1;
  if (num_planes < 2)
    alpha = 0;
  drm_device.drm_init_device(num_planes);
  vip.device_init();
  vpe.open_fd();

  int in_export_fds[vip.src.num_buffers];
  int out_export_fds[vpe.m_num_buffers];
  MSG("ckpt 1");
  // Create an "omap_device" from the fd
  struct omap_device *dev = omap_device_new(drm_device.fd);
  bo_vpe_in = (class DmaBuffer **) malloc(vip.src.num_buffers * sizeof(class DmaBuffer *));
  bo_vpe_out = (class DmaBuffer **) malloc(vip.src.num_buffers * sizeof(class DmaBuffer *));
  MSG("ckpt 2");

  if (!bo_vpe_in || !bo_vpe_out) {
    ERROR("memory allocation failure, exiting \n");
    exit(EXIT_FAILURE);
  }
  MSG("ckpt 3");

  for (int i = 0; i < vip.src.num_buffers; i++) {
      bo_vpe_in[i] = (class DmaBuffer *) malloc(sizeof(class DmaBuffer));
      bo_vpe_out[i] = (class DmaBuffer *) malloc(sizeof(class DmaBuffer));

      bo_vpe_in[i]->width = src_w;
      bo_vpe_out[i]->width = dst_w;

      bo_vpe_in[i]->height = src_h;
      bo_vpe_out[i]->height = dst_h;

      bo_vpe_in[i]->fourcc = vip.src.fourcc;
      bo_vpe_out[i]->fourcc = vpe.dst.fourcc;

      // allocate space for buffer object (bo)
      bo_vpe_in[i]->bo = (struct omap_bo **) malloc(4 *sizeof(omap_bo *));
      bo_vpe_out[i]->bo = (struct omap_bo **) malloc(4 *sizeof(omap_bo *));
      MSG("ckpt 4");

      // define the object
      MSG("vpe src bytes_pp %d", vpe.src.bytes_pp);
      MSG("vpe dst bytes_pp %d", vpe.dst.bytes_pp);

  		bo_vpe_in[i]->bo[0] = omap_bo_new(dev, src_w*src_h*vpe.src.bytes_pp,
        OMAP_BO_SCANOUT | OMAP_BO_WC);
      bo_vpe_out[i]->bo[0] = omap_bo_new(dev, dst_w*dst_h*vpe.dst.bytes_pp,
        OMAP_BO_SCANOUT | OMAP_BO_WC);
      MSG("ckpt 5");

      // give the object a file descriptor for dmabuf v4l2 calls
      bo_vpe_in[i]->fd[0] = omap_bo_dmabuf(bo_vpe_in[i]->bo[0]);
      bo_vpe_out[i]->fd[0] = omap_bo_dmabuf(bo_vpe_out[i]->bo[0]);
      MSG("ckpt 6");

      if (vip.src.memory == V4L2_MEMORY_MMAP) {
        bo_vpe_in[i]->buf_mem_addr = (void **) calloc(4, sizeof(unsigned int));
        bo_vpe_in[i]->buf_mem_addr[0] = omap_bo_map(bo_vpe_in[i]->bo[0]);
        bo_vpe_out[i]->buf_mem_addr = (void **) calloc(4, sizeof(unsigned int));
        bo_vpe_out[i]->buf_mem_addr[0] = omap_bo_map(bo_vpe_out[i]->bo[0]);
      }

      // MSG("Exported file descriptor for bo_vpe_in[%d]: %d", i, bo_vpe_in[i]->fd[0]);
      // MSG("Exported file descriptor for bo_vpe_out[%d]: %d", i, bo_vpe_out[i]->fd[0]);
      in_export_fds[i] = bo_vpe_in[i]->fd[0];
      out_export_fds[i] = bo_vpe_out[i]->fd[0];
      MSG("ckpt 7");
  }

  if(!vip.request_export_buf(in_export_fds)) {
    ERROR("VIP buffer requests failed.");
    return false;
  }
  MSG("Successfully requested VIP buffers\n\n");

  if (!vpe.vpe_input_init()) {
    ERROR("Input layer initialization failed.");
    return false;
  }
  MSG("Input layer initialization done\n");

  if(!vpe.vpe_output_init(out_export_fds)) {
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
  for (int j=0; j<vpe.m_num_buffers; j++)
    print_v4l2buffer(vpe.dst.v4l2bufs[j]);
  MSG("VIP initial buffer queues done\n");

  for (int i=0; i < vpe.m_num_buffers; i++) {
    if(!vpe.output_qbuf(i, out_export_fds[i])) {
      ERROR(" initial queue VPE output buffer #%d failed", i);
      return false;
    }
  }

  MSG("VPE initial output buffer queues done\n");



  vpe.m_field = V4L2_FIELD_ANY;
  for (int j=0; j<vpe.m_num_buffers; j++)
    print_v4l2buffer(vpe.dst.v4l2bufs[j]);
  MSG("###########################");
  drm_device.export_buffer(bo_vpe_out, vpe.m_num_buffers, vpe.dst.bytes_pp, 0);
  for (int j=0; j<vpe.m_num_buffers; j++)
    print_v4l2buffer(vpe.dst.v4l2bufs[j]);
  MSG("Buffer from vpe exported");

  // initialize the second plane of data
  if (num_planes > 1) {
    if (net_type == "seg")
      drm_device.get_vid_buffers(3, FOURCC_STR("RX12"), dst_w, dst_h, 2, 1);
    else if (net_type == "ssd")
      drm_device.get_vid_buffers(3, FOURCC_STR("AR24"), dst_w, dst_h, 4, 1);
  }

  // begin streaming the capture through the VIP
  if (!vip.stream_on()) return false;
  // begin streaming the output of the VPE
  if (!vpe.stream_on(1)) return false;

  // for (int j=0; j<vpe.m_num_buffers; j++)
  //   print_v4l2buffer(vpe.dst.v4l2bufs[j]);

  drm_device.drm_init_dss(&vpe.dst, alpha);

  // for (int j=0; j<vpe.m_num_buffers; j++)
  //   print_v4l2buffer(vpe.dst.v4l2bufs[j]);

  return true;
}

void CamDisp::disp_frame() {
  drm_device.disp_frame(disp_frame_num);
}

void *CamDisp::grab_image() {
    /* This step actually releases the frame back into the pipeline, but we
     * don't want to do this until the user calls for another frame. Otherwise,
     * the data pointed to by *imagedata could be corrupted.
     */
    if (stop_after_one) {
      MSG("Before vpe output qbuf");

      vpe.output_qbuf(frame_num, bo_vpe_out[frame_num]->fd[0]);
      MSG("After vpe output qbuf");
      frame_num = vpe.input_dqbuf();
      vip.queue_buf(bo_vpe_in[frame_num]->fd[0], frame_num);
    }
    MSG("After vpe output qbuf, vpe input_dqbuf, vip queue_buf");

    /* dequeue the vip */
    frame_num = vip.dequeue_buf();
    if (vip.src.memory == V4L2_MEMORY_MMAP) {
      memcpy(bo_vpe_in[frame_num]->buf_mem_addr[0], vip.src.base_addr[frame_num], vip.src.size);
    }


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

    // if no display, then the api is not called
    disp_frame_num = frame_num;

    /**********DATA IS HERE!!************/
    void *imagedata = (void *) bo_vpe_out[frame_num]->buf_mem_addr[0];

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

int main(int argc, char *argv[]) {
  int cap_w = 800;
  int cap_h = 600;
  int model_w = 768;
  int model_h = 320;

  // This is the type of neural net that is being targeted
  std::string net_type = "seg";

  // capture w, capture h, output w, output h, device name, is usb?
  CamDisp cam(cap_w, cap_h, model_w, model_h, 150, "/dev/video2", true, net_type);

  cam.init_capture_pipeline(net_type);
  auto start = std::chrono::high_resolution_clock::now();

  int num_frames = 300;
  if (argc > 1){
    num_frames = stoi(argv[1]);
  }

  for (int i=0; i<num_frames; i++) {
    if (argc <= 2) {
      MSG("grabbing");
      cam.grab_image();
      MSG("grabbed");
      // cam.disp_frame();
    }
    else
      save_data(cam.grab_image(), model_w, model_h, 3, 3);
    //drm_device.disp_frame(NULL);
  }
  auto stop = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
  MSG("******************");
  MSG("Capture at %dx%d\nResized to %dx%d\nFrame rate %f",cap_w, cap_h,

  model_w, model_h, (float) num_frames/((float)duration.count()/1000));
  MSG("Total time to capture %d frames: %f seconds", num_frames, (float)
      duration.count()/1000);
  MSG("******************");
  cam.turn_off();
}
