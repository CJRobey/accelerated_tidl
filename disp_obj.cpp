#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <linux/videodev2.h>
#include <omap_drm.h>
#include <omap_drmif.h>
#include <xf86drmMode.h>
#include "cmem_buf.h"
#include "error.h"
#include "disp_obj.h"
#include "omap_wrapper.c"
#include <linux/dma-buf.h>

/* align x to next highest multiple of 2^n */
#define ALIGN2(x,n)   (((x) + ((1 << (n)) - 1)) & ~((1 << (n)) - 1))

/* If the use case need the buffer to be accessed by CPU for some processings,
 * then CMEM buffer can be used as they support cache operations by CPU.
 * omap_drm buffers doesn't support cache read. CPU can take 10x to 60x
 * more cycles to operate on non cached buffer. USE_CMEM_BUF macro is disabled
 * by deafult. Macro can be enabled from cmem_buf.h file
 */
bool DRMDeviceInfo::alloc_buffer(unsigned int fourcc, unsigned int w,
		unsigned int h)
{
	unsigned int bo_handles[4] = {0}, offsets[4] = {0};
	int ret;
	int bytes_pp = 2; //capture buffer is in YUYV format

  // Initialize buffer with w, h, bytes per pixel, fourcc (RGB), 1 pixel
  // alignment and one buffer.
  if (use_cmem) {
    disp_dmabuf = BufObj(w, h, bytes_pp, fourcc, 1, 1);
    if (disp_dmabuf.m_buf == NULL) {
      ERROR("allocation failed");
      return false;
    }
    /* Get the omap bo from the fd allocted using CMEM */
    disp_dmabuf.bo[0] = _omap_bo_from_dmabuf(dev, disp_dmabuf.m_fd[0]);
    if (disp_dmabuf.bo[0]){
      bo_handles[0] = _omap_bo_handle(disp_dmabuf.bo[0]);
    }
  }
  else {
    //You can use DRM ioctl as well to allocate buffers (DRM_IOCTL_MODE_CREATE_DUMB)
    //and drmPrimeHandleToFD() to get the buffer descriptors
    disp_dmabuf.bo[0] = omap_bo_new(dev, w*h*bytes_pp, bo_flags | OMAP_BO_WC);
    if (disp_dmabuf.bo[0]){
      bo_handles[0] = omap_bo_handle(disp_dmabuf.bo[0]);
    }

    disp_dmabuf.m_fd[0] = omap_bo_dmabuf(disp_dmabuf.bo[0]);
  }

	ret = drmModeAddFB2(fd, disp_dmabuf.m_width, disp_dmabuf.m_height, fourcc,
		bo_handles, &disp_dmabuf.m_stride, offsets, disp_dmabuf.m_fb_id, 0);

	MSG("fourcc:%d, fb_id %d \n", fourcc, disp_dmabuf.m_fb_id);
	if (ret) {
		ERROR("drmModeAddFB2 failed: %s (%d)", strerror(errno), ret);
		return false;
	}

	return true;
}
