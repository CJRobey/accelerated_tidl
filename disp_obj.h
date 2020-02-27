#include <xf86drmMode.h>
#include <linux/videodev2.h>
#include <string>
#include "v4l2_obj.h"
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
  DmaBuffer *alloc_buffer();
	void free_vid_buffers(unsigned int channel);
	bool get_vid_buffers(unsigned int channel, unsigned int _n,
			unsigned int _fourcc, unsigned int _w, unsigned int _h);
  DRMDeviceInfo();
	~DRMDeviceInfo();

	unsigned int get_drm_prop_val(drmModeObjectPropertiesPtr props,
			const char *name);
	unsigned int find_drm_prop_id(drmModeObjectPropertiesPtr props,
		  const char *name);
	void add_property(int fd, drmModeAtomicReqPtr req, drmModeObjectPropertiesPtr props,
		  unsigned int plane_id,
		  const char *name, int value);
	void drm_add_plane_property(drmModeAtomicReqPtr req, VIPObj vip);
	unsigned int drm_reserve_plane(unsigned int *ptr_plane_id, int num_planes);
	void drm_crtc_resolution();
	void drm_restore_props();
	int drm_init_device();
	int drm_init_dss(VIPObj vip);
	void drm_exit_device();

	int fd;
	int width;
	int height;
	unsigned int w;
	unsigned int h;
	unsigned int fourcc;
	unsigned int n;
	bool use_cmem = false;
	char dev_name[9];
	char name[4];
	unsigned int bo_flags;

	/* There are two buffers for the two planes that will exist in the DSS
	 * plane_data_buffer[0]
	 */
	DmaBuffer **plane_data_buffer[2];
	struct omap_device *dev;
	unsigned int crtc_id;
	unsigned int plane_id[2];
	unsigned int prop_fbid;
	unsigned int prop_crtcid;
	uint64_t zorder_val_primary_plane;
	uint64_t trans_key_mode_val;
	uint32_t zorder_val[MAX_DRM_PLANES-1];

  unsigned int main_cam;
  unsigned int num_cams;
  unsigned int num_jpeg;
  unsigned int display_xres, display_yres;
  bool pip;
  bool jpeg;
  bool exit;
};
