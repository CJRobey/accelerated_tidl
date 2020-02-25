#include <xf86drmMode.h>
#include <linux/videodev2.h>
#include <string>
#include "cmem_buf.h"

#define PAGE_SHIFT 12
#define MAX_DRM_PLANES 5
#define CAP_WIDTH 800
#define CAP_HEIGHT 600
#define PIP_POS_X  25
#define PIP_POS_Y  25
#define MAX_ZORDER_VAL 3 //For AM57x device, max zoder value is 3

class DmaBuffer {
public:
	uint32_t fourcc, width, height;
	int nbo;
	void *cmem_buf;
	struct omap_bo *bo[4];
	uint32_t pitches[4];
	int fd[4];		/* dmabuf */
	unsigned fb_id;
};

class ConnectorInfo {
public:
	unsigned int id;
	char mode_str[64];
	drmModeModeInfo *mode;
	drmModeEncoder *encoder;
	int crtc;
	int pipe;
};


/*
* drm output device structure declaration
*/
class DRMDeviceInfo
{
public:
  bool alloc_buffer(unsigned int fourcc, unsigned int w,
  		unsigned int h);

	int fd;
	int width;
	int height;
	char dev_name[9];
	char name[4];
	unsigned int bo_flags;
	struct dmabuf_buffer **buf[2];
	struct omap_device *dev;
	unsigned int crtc_id;
	unsigned int plane_id[2];
	unsigned int prop_fbid;
	unsigned int prop_crtcid;
	uint64_t zorder_val_primary_plane;
	uint64_t trans_key_mode_val;
	uint32_t zorder_val[MAX_DRM_PLANES-1];
  BufObj disp_dmabuf;

  unsigned int main_cam;
  unsigned int num_cams;
  unsigned int num_jpeg;
  unsigned int display_xres, display_yres;
  bool pip;
  bool jpeg;
  bool exit;
  bool use_cmem;
};
