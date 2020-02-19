/* This file defines the function to program V4L2 device */

#include <unistd.h>
#include <errno.h>
#include <string>
#include <string.h>
#include <fcntl.h>
#include <iostream>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <linux/videodev2.h>
#include <linux/v4l2-controls.h>

#include "v4l2_obj.h"
#include "cmem_buf.h"
#include "error.h"


#define V4L2_CID_TRANS_NUM_BUFS         (V4L2_CID_PRIVATE_BASE)

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
    src.coplanar = false;
    src.field = V4L2_FIELD_ANY;
    src.colorspace = V4L2_COLORSPACE_SMPTE170M;
    src.memory = V4L2_MEMORY_MMAP;


    dst.num_buffers = NBUF;
    dst.fourcc = V4L2_PIX_FMT_RGB24;
    dst.width = MODEL_WIDTH;
    dst.height = MODEL_HEIGHT;
    dst.size = dst.width * dst.height * 3;
    dst.v4l2buf = NULL;
    // CAPTURE is the output buffer of the VPE
    dst.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    dst.coplanar = false;
    dst.field = V4L2_FIELD_ANY;
    dst.colorspace = V4L2_COLORSPACE_SRGB;
    dst.memory = V4L2_MEMORY_MMAP;
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


bool VPEObj::set_ctrl()
{
	int ret;
	struct	v4l2_control ctrl;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.id = V4L2_CID_TRANS_NUM_BUFS;
	ctrl.value = m_translen;
	ret = ioctl(m_fd, VIDIOC_S_CTRL, &ctrl);
	if (ret < 0) {
		ERROR("vpe: S_CTRL failed\n");
    return false;
  }
	return true;
}

bool VPEObj::vpe_input_init()
{
	int ret, i;
	struct v4l2_format fmt;
	struct v4l2_requestbuffers rqbufs;
  struct v4l2_buffer v4l2buf;
  struct v4l2_plane		buf_planes[2];
  struct v4l2_capability capability;
  struct v4l2_streamparm streamparam;

	if (!set_ctrl()) return false;

  MSG("\n%s: Opened Channel\n", m_dev_name.c_str());

  /* Check if the device is capable of streaming */
  /*
  if (ioctl(m_fd, VIDIOC_QUERYCAP, &capability) < 0) {
      ERROR("VIDIOC_QUERYCAP");
  }

  if (capability.capabilities & V4L2_CAP_STREAMING)
      MSG("%s: Capable of streaming\n", m_dev_name.c_str());
  else {
      ERROR("%s: Not capable of streaming\n", m_dev_name.c_str());
  }


  ret = ioctl(m_fd, VIDIOC_G_FMT, &src.fmt);
  if (ret < 0) {
      ERROR("VIDIOC_G_FMT failed: %s (%d)", strerror(errno), ret);
      return false;
  }*/
	memset(&fmt, 0, sizeof fmt);
  src.fmt.type = src.type;
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
	rqbufs.type = src.type;
	rqbufs.memory = src.memory;

	ret = ioctl(m_fd, VIDIOC_REQBUFS, &rqbufs);
	if (ret < 0) {
		ERROR( "%s: vpe i/p: REQBUFS failed: %s\n", m_dev_name.c_str(), strerror(errno));
    return false;
  }
	src.num_buffers = rqbufs.count;
  src.base_addr = (unsigned int **) calloc(src.num_buffers, sizeof(unsigned int));
  for (i = 0; i < src.num_buffers; i++) {
    memset(&v4l2buf, 0, sizeof(v4l2buf));
    v4l2buf.type = src.type;
    v4l2buf.memory = src.memory;
    v4l2buf.m.planes	= buf_planes;
    v4l2buf.length	= src.coplanar ? 2 : 1;
    v4l2buf.index = i;

    ret = ioctl(m_fd, VIDIOC_QUERYBUF, &v4l2buf);
    if (ret) {
        ERROR("VIDIOC_QUERYBUF failed: %s (%d)", strerror(errno), ret);
        return ret;
    }
    src.base_addr[i] = (unsigned int *) mmap(NULL, v4l2buf.m.planes[0].length, PROT_READ | PROT_WRITE,
           MAP_SHARED, m_fd, v4l2buf.m.planes[0].m.mem_offset);


    if (MAP_FAILED == src.base_addr[i]) {
      while(i>=0){
        /* Unmap all previous buffers in case of failure*/
        i--;
        munmap(src.base_addr[i], src.size);
        src.base_addr[i] = NULL;
      }
      ERROR("Cant mmap buffers Y");
      return false;
    }
  }

	return true;

}

bool VPEObj::vpe_output_init()
{
	int ret;
	struct v4l2_format fmt;
	struct v4l2_requestbuffers rqbufs;
  struct v4l2_plane		buf_planes[2];
  struct v4l2_buffer v4l2buf;

	bzero(&fmt, sizeof fmt);
	fmt.type = dst.type;
	fmt.fmt.pix_mp.width = dst.width;
	fmt.fmt.pix_mp.height = dst.height;
	fmt.fmt.pix_mp.pixelformat = dst.fourcc;
	fmt.fmt.pix_mp.field = dst.field;
	fmt.fmt.pix_mp.colorspace = dst.colorspace;
	fmt.fmt.pix_mp.num_planes = dst.coplanar ? 2 : 1;

	ret = ioctl(m_fd, VIDIOC_S_FMT, &fmt);
	if (ret < 0) {
		ERROR( "%s: vpe o/p: S_FMT failed: %s\n", m_dev_name.c_str(), strerror(errno));
    return false;
  }
  src.size = fmt.fmt.pix_mp.plane_fmt[0].sizeimage;
  src.size_uv = fmt.fmt.pix_mp.plane_fmt[1].sizeimage;
  MSG("size %d", fmt.fmt.pix_mp.plane_fmt[0].sizeimage);
  MSG("size_uv %d", fmt.fmt.pix_mp.plane_fmt[1].sizeimage);

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
	rqbufs.type = dst.type;
	rqbufs.memory = dst.memory;

	ret = ioctl(m_fd, VIDIOC_REQBUFS, &rqbufs);
	if (ret < 0) {
		ERROR( "%s: vpe o/p: REQBUFS failed: %s\n", m_dev_name.c_str(), strerror(errno));
    return false;
  }

	dst.num_buffers = rqbufs.count;
  dst.base_addr = (unsigned int **) calloc(dst.num_buffers, sizeof(unsigned int));
  for (int i = 0; i < dst.num_buffers; i++) {
      memset(&v4l2buf, 0, sizeof(v4l2buf));
      v4l2buf.type = dst.type;
      v4l2buf.memory = dst.memory;
      v4l2buf.m.planes	= buf_planes;
      v4l2buf.length	= dst.coplanar ? 2 : 1;
      v4l2buf.index = i;

      ret = ioctl(m_fd, VIDIOC_QUERYBUF, &v4l2buf);
      dst.base_addr[i] = (unsigned int *) mmap(NULL, v4l2buf.m.planes[0].length, PROT_READ | PROT_WRITE,
             MAP_SHARED, m_fd, v4l2buf.m.planes[0].m.mem_offset);

      if (ret) {
          ERROR("VIDIOC_QUERYBUF failed: %s (%d)", strerror(errno), ret);
          return ret;
      }
  }

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
	disp->multiplanar = false;
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

int VPEObj::input_qbuf(int index)
{
	int ret;
	struct v4l2_buffer buf;
	struct v4l2_plane planes[2];

	MSG("vpe: src QBUF (%d):%s field", src.field,
		src.field==V4L2_FIELD_TOP?"top":"bottom");

	memset(&buf, 0, sizeof buf);
	memset(&planes, 0, sizeof planes);

	planes[0].length = planes[0].bytesused = src.size;
	if(src.coplanar)
		planes[1].length = planes[1].bytesused = src.size_uv;

	planes[0].data_offset = planes[1].data_offset = 0;

	buf.type = src.type;
	buf.memory = src.memory;
	buf.index = index;
	buf.m.planes = &planes[0];
	buf.field = src.field;
	if (src.coplanar)
		buf.length = 2;
	else
		buf.length = 1;

	// buf.m.planes[0].m.fd = vpe->input_buf_dmafd[index];
	// if(vpe->src.coplanar)
	// 	buf.m.planes[1].m.fd = vpe->input_buf_dmafd_uv[index];

	ret = ioctl(m_fd, VIDIOC_QBUF, &buf);
	if (ret < 0) {
		ERROR( "vpe i/p: QBUF failed: %s, index = %d\n", strerror(errno), index);
      return false;
  }
	return true;
}

bool VPEObj::output_qbuf(int index)
{
	int ret;
	struct v4l2_buffer buf;
	struct v4l2_plane planes[2];

	memset(&buf, 0, sizeof buf);
	memset(&planes, 0, sizeof planes);

	buf.type = dst.type;
	buf.memory = dst.memory;
	buf.index = index;
	buf.m.planes = &planes[0];
	if(dst.coplanar)
		buf.length = 2;
	else
		buf.length = 1;

  gettimeofday(&buf.timestamp, NULL);

	ret = ioctl(m_fd, VIDIOC_QBUF, &buf);
	if (ret < 0) {
		ERROR( "vpe o/p: QBUF failed: %s, index = %d\n",
			strerror(errno), index);
      return false;
  }
	return true;
}


int VPEObj::input_dqbuf()
{
	int ret;
	struct v4l2_buffer buf;
	struct v4l2_plane planes[2];

	memset(&buf, 0, sizeof buf);
	memset(&planes, 0, sizeof planes);

	buf.type = src.type;
	buf.memory = src.memory;
  buf.m.planes = &planes[0];
  if (src.coplanar)
		buf.length = 2;
	else
		buf.length = 1;
	ret = ioctl(m_fd, VIDIOC_DQBUF, &buf);
  if (ret < 0) {
    return -1;
  }
	return buf.index;
}


int VPEObj::output_dqbuf()
{
	int ret;
	struct v4l2_buffer buf;
	struct v4l2_plane planes[2];

	memset(&buf, 0, sizeof buf);
	memset(&planes, 0, sizeof planes);
	buf.type = dst.type;
	buf.memory = dst.memory;
        buf.m.planes = &planes[0];
	if(dst.coplanar)
		buf.length = 2;
	else
		buf.length = 1;
	ret = ioctl(m_fd, VIDIOC_DQBUF, &buf);
	if (ret < 0) {
		ERROR("vpe o/p: DQBUF failed: %s\n", strerror(errno));
    return -1;
  }

	return buf.index;
}


/*
* Enable streaming for V4L2 capture device
*/
bool VPEObj::stream_on(int layer){
  enum v4l2_buf_type type;
  int ret;

  if (layer == 1) {
    type = (v4l2_buf_type) dst.type;
    MSG("Streaming VPE Output");
  }
  else if (layer == 0) {
    type = (v4l2_buf_type) src.type;
    MSG("Streaming VPE Intput");
  }

  ret = ioctl(m_fd, VIDIOC_STREAMON, &type);

  if (ret) {
      ERROR("VIDIOC_STREAMON failed: %s (%d)", strerror(errno), ret);
      return false;
  }

  return true;
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
