/* This file defines the function to program V4L2 device */

#include <unistd.h>
#include <errno.h>
#include <string>
#include <string.h>
#include <fcntl.h>
#include <iostream>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <linux/v4l2-controls.h>
#include "v4l2_obj.h"
#include "cmem_buf.h"
#include "error.h"




/*
* Initialize the app resources with default parameters
*/
void VPEObj::default_parameters(void) {

    m_fd = 0;
    m_dev_name = "/dev/video0";

    src.num_buffers = NBUF;
    src.fourcc = V4L2_PIX_FMT_YUYV;
    src.width = CAP_WIDTH;
    src.height = CAP_HEIGHT;
    src.size = src.width * src.height * 2;
    src.v4l2buf = NULL;
    src.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    src.coplanar = 0;
    src.field = V4L2_FIELD_ANY;
    src.colorspace = V4L2_COLORSPACE_SMPTE170M;

    dst.num_buffers = NBUF;
    dst.fourcc = V4L2_PIX_FMT_RGB24;
    dst.width = MODEL_WIDTH;
    dst.height = MODEL_HEIGHT;
    dst.size = dst.width * dst.height * 3;
    dst.v4l2buf = NULL;
    dst.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    dst.coplanar = 0;
    dst.field = V4L2_FIELD_ANY;
    dst.colorspace = V4L2_COLORSPACE_SRGB;

    return;
}


bool VPEObj::open_fd() {

  /* Open the capture device */
  m_fd = open(m_dev_name.c_str(), O_RDWR);

  if (m_fd <= 0) {
      ERROR("Cannot open %s device\n\n", m_dev_name.c_str());
      return false;
  }

  MSG("\n%s: Opened Channel\n", m_dev_name.c_str());
  return true;
}


int VPEObj::set_ctrl()
{
	int ret;
	struct	v4l2_control ctrl;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.id = V4L2_CID_PRIVATE_BASE;
	ctrl.value = m_translen;
	ret = ioctl(m_fd, VIDIOC_S_CTRL, &ctrl);
	if (ret < 0)
		ERROR("vpe: S_CTRL failed\n");

	return 0;
}

bool VPEObj::vpe_input_init(int *fd)
{
	int ret, i;
	struct v4l2_format fmt;
	struct v4l2_requestbuffers rqbufs;
  struct v4l2_capability capability;
  struct v4l2_streamparm streamparam;

	set_ctrl();

  MSG("\n%s: Opened Channel\n", m_dev_name.c_str());

  /* Check if the device is capable of streaming */
  if (ioctl(m_fd, VIDIOC_QUERYCAP, &capability) < 0) {
      ERROR("VIDIOC_QUERYCAP");
  }

  if (capability.capabilities & V4L2_CAP_STREAMING)
      MSG("%s: Capable of streaming\n", m_dev_name.c_str());
  else {
      ERROR("%s: Not capable of streaming\n", m_dev_name.c_str());
  }

  streamparam.type = src.type;
  if (ioctl(m_fd, VIDIOC_G_PARM, &streamparam) < 0){
      ERROR("VIDIOC_G_PARM");
  }

  src.fmt.type = src.type;
  ret = ioctl(m_fd, VIDIOC_G_FMT, &src.fmt);
  if (ret < 0) {
      ERROR("VIDIOC_G_FMT failed: %s (%d)", strerror(errno), ret);
      return false;
  }
	//memset(&fmt, 0, sizeof fmt);
	fmt.type = src.type;
	fmt.fmt.pix_mp.width = src.width;
	fmt.fmt.pix_mp.height = src.height;
	fmt.fmt.pix_mp.pixelformat = src.fourcc;
	fmt.fmt.pix_mp.colorspace = src.colorspace;
	fmt.fmt.pix_mp.num_planes = src.coplanar ? 2 : 1;

	switch (m_deinterlace) {
	case 1:
		fmt.fmt.pix_mp.field = V4L2_FIELD_ALTERNATE;
		break;
	case 2:
		fmt.fmt.pix_mp.field = V4L2_FIELD_SEQ_TB;
		break;
	case 0:
	default:
		fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
		break;
	}

	ret = ioctl(m_fd, VIDIOC_S_FMT, &fmt);
	if (ret < 0) {
		ERROR( "%s: vpe i/p: S_FMT failed: %s\n", m_dev_name.c_str(), strerror(errno));
    return false;
  }
  else {
    src.size = fmt.fmt.pix_mp.plane_fmt[0].sizeimage;
    src.size_uv = fmt.fmt.pix_mp.plane_fmt[1].sizeimage;
  }

	ret = ioctl(m_fd, VIDIOC_G_FMT, &fmt);
	if (ret < 0) {
		ERROR( "%s: vpe i/p: G_FMT_2 failed: %s\n", m_dev_name.c_str(), strerror(errno));
    return false;
  }
	MSG("%s: vpe i/p: G_FMT: width = %u, height = %u, 4cc = %.4s\n",
			m_dev_name.c_str(), fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height,
			(char*)&fmt.fmt.pix_mp.pixelformat);

//	set_crop(vpe);

	memset(&rqbufs, 0, sizeof(rqbufs));
	rqbufs.count = NBUF;
	rqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	rqbufs.memory = V4L2_MEMORY_DMABUF;

	ret = ioctl(m_fd, VIDIOC_REQBUFS, &rqbufs);
	if (ret < 0) {
		ERROR( "%s: vpe i/p: REQBUFS failed: %s\n", m_dev_name.c_str(), strerror(errno));
    return false;
  }
	src.num_buffers = rqbufs.count;
  src.v4l2buf = (struct v4l2_buffer *) calloc(src.num_buffers, sizeof(struct v4l2_buffer));
  if (!src.v4l2buf) {
      ERROR("allocation failed");
      return -1;
  }

  for (i = 0; i < rqbufs.count; i++) {
      struct v4l2_plane planes[2];
      memset(&planes, 0, sizeof planes);
      memset(&src.v4l2buf[i], 0, sizeof(struct v4l2_buffer));
      src.v4l2buf[i].type =  rqbufs.type;
      src.v4l2buf[i].memory = rqbufs.memory;
      src.v4l2buf[i].index = i;
      src.v4l2buf[i].m.planes = &planes[0];

      MSG("buffer index %d", i);
      ret = ioctl(m_fd, VIDIOC_QUERYBUF, &src.v4l2buf[i]);
      src.v4l2buf[i].m.fd = fd[i];
      if (ret) {
          ERROR("VIDIOC_QUERYBUF failed: %s (%d)", strerror(errno), ret);
          return false;
      }

  }
	return true;

}

bool VPEObj::vpe_output_init(BufObj vpe_output_buffer)
{
	int ret; //, i;
	struct v4l2_format fmt;
	struct v4l2_requestbuffers rqbufs;
	bool saved_multiplanar;

	memset(&fmt, 0, sizeof fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	fmt.fmt.pix_mp.width = dst.width;
	fmt.fmt.pix_mp.height = dst.height;
	fmt.fmt.pix_mp.pixelformat = dst.fourcc;
	fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
	fmt.fmt.pix_mp.colorspace = dst.colorspace;
	fmt.fmt.pix_mp.num_planes = dst.coplanar ? 2 : 1;

	ret = ioctl(m_fd, VIDIOC_S_FMT, &fmt);
	if (ret < 0) {
		ERROR( "%s: vpe o/p: S_FMT failed: %s\n", m_dev_name.c_str(), strerror(errno));
    return false;
  }

	ret = ioctl(m_fd, VIDIOC_G_FMT, &fmt);
	if (ret < 0) {
		ERROR( "%s: vpe o/p: G_FMT_2 failed: %s\n", m_dev_name.c_str(), strerror(errno));
    return false;
  }

	MSG("%s: vpe o/p: G_FMT: width = %u, height = %u, 4cc = %.4s\n",
			 m_dev_name.c_str(), fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height,
			(char*)&fmt.fmt.pix_mp.pixelformat);

	memset(&rqbufs, 0, sizeof(rqbufs));
	rqbufs.count = NBUF;
	rqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	rqbufs.memory = V4L2_MEMORY_DMABUF;

	ret = ioctl(m_fd, VIDIOC_REQBUFS, &rqbufs);
	if (ret < 0) {
		ERROR( "%s: vpe o/p: REQBUFS failed: %s\n", m_dev_name.c_str(), strerror(errno));
    return false;
  }

	dst.num_buffers = rqbufs.count;
	MSG("%s: vpe o/p: allocated buffers = %d\n", m_dev_name.c_str(), rqbufs.count);

	/*
	 * disp->multiplanar is used when allocating buffers to enable
	 * allocating multiplane buffer in separate buffers.
	 * VPE does handle mulitplane NV12 buffer correctly
	 * but VIP can only handle single plane buffers
	 * So by default we are setup to use single plane and only overwrite
	 * it when allocating strictly VPE buffers.
	 * Here we saved to current value and restore it after we are done
	 * allocating the buffers VPE will use for output.
	 */
   /*
	saved_multiplanar = disp->multiplanar;
	disp->multiplanar = true;
	disp_bufs = disp_get_vid_buffers(vpe->disp, NUMBUF, vpe->dst.fourcc,
					      vpe->dst.width, vpe->dst.height);
	vpe->disp->multiplanar = saved_multiplanar;
	if (!vpe->disp_bufs)
		ERROR("allocating display buffer failed\n");

	for (i = 0; i < NUMBUF; i++) {
		vpe->output_buf_dmafd[i] = omap_bo_dmabuf(vpe->disp_bufs[i]->bo[0]);
		vpe->disp_bufs[i]->fd[0] = vpe->output_buf_dmafd[i];

		if(vpe->dst.coplanar) {
			vpe->output_buf_dmafd_uv[i] = omap_bo_dmabuf(vpe->disp_bufs[i]->bo[1]);
			vpe->disp_bufs[i]->fd[1] = vpe->output_buf_dmafd_uv[i];
		}

		vpe->disp_bufs[i]->noScale = true;
		dprintf("vpe->disp_bufs_fd[%d] = %d\n", i, vpe->output_buf_dmafd[i]);
	}
*/
	//dprintf("allocating display buffer success\n");
	return true;
}

bool VPEObj::output_qbuf(int index)
{
	int ret;
	struct v4l2_buffer buf;
	struct v4l2_plane planes[2];

	memset(&buf, 0, sizeof buf);
	memset(&planes, 0, sizeof planes);

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	buf.memory = V4L2_MEMORY_DMABUF;
	buf.index = index;
	buf.m.planes = &planes[0];
	if(dst.coplanar)
		buf.length = 2;
	else
		buf.length = 1;

	ret = ioctl(m_fd, VIDIOC_QBUF, &buf);
	if (ret < 0) {
		ERROR( "vpe o/p: QBUF failed: %s, index = %d\n",
			strerror(errno), index);
      return false;
  }
	return true;
}

/*
* Enable streaming for V4L2 capture device
*/
int VPEObj::stream_on(){
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    int ret = 0;

    ret = ioctl(m_fd, VIDIOC_STREAMON, &type);

    if (ret) {
        ERROR("VIDIOC_STREAMON failed: %s (%d)", strerror(errno), ret);
    }

    return ret;
}



VPEObj::VPEObj(){
    default_parameters();
    open_fd();
}

VPEObj::~VPEObj(){
    free(src.v4l2buf);
    free(dst.v4l2buf);
    close(m_fd);
    return;
}
