/******************************************************************************
 * Copyright (c) 2019-2020, Texas Instruments Incorporated - http://www.ti.com/
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions are met:
 *       * Redistributions of source code must retain the above copyright
 *         notice, this list of conditions and the following disclaimer.
 *       * Redistributions in binary form must reproduce the above copyright
 *         notice, this list of conditions and the following disclaimer in the
 *         documentation and/or other materials provided with the distribution.
 *       * Neither the name of Texas Instruments Incorporated nor the
 *         names of its contributors may be used to endorse or promote products
 *         derived from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *   THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

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
#include "error.h"


#define V4L2_CID_TRANS_NUM_BUFS         (V4L2_CID_PRIVATE_BASE)

/*
* Initialize the app resources with default parameters
*/
void VPEObj::default_parameters(void) {

    m_fd = 0;
    m_dev_name = "/dev/video0";
    m_deinterlace = 0;
    m_field = V4L2_FIELD_ANY;
    m_num_buffers = 3;

    src.fourcc = V4L2_PIX_FMT_YUYV;
    src.width = CAP_WIDTH;
    src.height = CAP_HEIGHT;
    src.bytes_pp = 2;
    src.size = src.width * src.height * src.bytes_pp;
    src.v4l2bufs = NULL;
    // OUTPUT is the input buffer of the VPE
    src.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    src.coplanar = false;
    src.colorspace = V4L2_COLORSPACE_SMPTE170M;
    src.memory = V4L2_MEMORY_DMABUF;

    dst.fourcc = V4L2_PIX_FMT_BGR24;
    dst.width = TIDL_MODEL_WIDTH;
    dst.height = TIDL_MODEL_HEIGHT;
    dst.bytes_pp = 3;
    dst.size = dst.width * dst.height * dst.bytes_pp;
    dst.v4l2bufs = NULL;
    // CAPTURE is the output buffer of the VPE
    dst.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    dst.coplanar = false;
    dst.colorspace = V4L2_COLORSPACE_SRGB;
    dst.memory = V4L2_MEMORY_MMAP;
    return;
}


bool VPEObj::open_fd() {

  /* Open the capture device */
  m_fd = open(m_dev_name.c_str(), O_RDWR);

  if (m_fd < 0) {
      ERROR("Cannot open %s device\n\n", m_dev_name.c_str());
      return false;
  }

  MSG("\n%s: Opened Channel with fd = %d\n", m_dev_name.c_str(), m_fd);

  if (m_fd == 0) {
    MSG("WARNING: Capture device opened fd 0. There may be an issue with stdin.");
    sleep(1.5);
  }
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
		ERROR("%s: vpe: S_CTRL failed with error %s\n", m_dev_name.c_str(), strerror(errno));
    return false;
  }
	return true;
}

bool VPEObj::vpe_input_init()
{
	int ret;
	struct v4l2_requestbuffers rqbufs;
  struct v4l2_selection selection;


	if (!set_ctrl()) return false;


  selection.r.left = 0;
  selection.r.top = 0;
  selection.r.width = dst.width;
  selection.r.height = dst.height;
  selection.target = V4L2_SEL_TGT_CROP_ACTIVE;
  selection.type = src.type;

  ret = ioctl(m_fd, VIDIOC_S_SELECTION, &selection);
  if (ret < 0) {
    ERROR( "%s: vpe i/p: S_SELECTION failed: %s\n", m_dev_name.c_str(), strerror(errno));
    return false;
  }
  ret = ioctl(m_fd, VIDIOC_G_SELECTION, &selection);
  if (ret < 0) {
    ERROR( "%s: vpe i/p: G_SELECTION failed: %s\n", m_dev_name.c_str(), strerror(errno));
    return false;
  }

  DBG("Cropping params of VPE set to\nw: %d\nh: %d\norigin: (%d,%d)",
    selection.r.width, selection.r.height, selection.r.left, selection.r.top);


  MSG("\n%s: Opened Channel\n", m_dev_name.c_str());
  if (src.type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
    src.fmt.type = src.type;
    src.fmt.fmt.pix_mp.width = src.width;
  	src.fmt.fmt.pix_mp.height = src.height;
  	src.fmt.fmt.pix_mp.pixelformat = src.fourcc;
  	src.fmt.fmt.pix_mp.colorspace = src.colorspace;
    src.fmt.fmt.pix_mp.field = m_field;
    src.fmt.fmt.pix_mp.num_planes = src.coplanar ? 2 : 1;
    src.fmt.fmt.pix_mp.plane_fmt[0].bytesperline = int(src.size/src.height);
    src.fmt.fmt.pix_mp.plane_fmt[0].sizeimage = src.size;
  }
  else {
    src.fmt.type = src.type;
    src.fmt.fmt.pix.width = src.width;
  	src.fmt.fmt.pix.height = src.height;
  	src.fmt.fmt.pix.pixelformat = src.fourcc;
  	src.fmt.fmt.pix.colorspace = src.colorspace;
    src.fmt.fmt.pix.field = m_field;
    src.fmt.fmt.pix.bytesperline = int(src.size/src.height);
    src.fmt.fmt.pix.sizeimage = src.size;
    }

	ret = ioctl(m_fd, VIDIOC_S_FMT, &src.fmt);
	if (ret < 0) {
		ERROR( "%s: vpe i/p: S_FMT failed: %s\n", m_dev_name.c_str(), strerror(errno));
    return false;
  }
  else if (src.type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
    src.size = src.fmt.fmt.pix_mp.plane_fmt[0].sizeimage;
    src.size_uv = src.fmt.fmt.pix_mp.plane_fmt[1].sizeimage;
  }

	ret = ioctl(m_fd, VIDIOC_G_FMT, &src.fmt);
	if (ret < 0) {
		ERROR( "%s: vpe i/p: G_FMT_2 failed: %s\n", m_dev_name.c_str(), strerror(errno));
    return false;
  }
  if (src.type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
	MSG("%s: vpe i/p: G_FMT: width = %u, height = %u, 4cc = %.4s\n",
			m_dev_name.c_str(), src.fmt.fmt.pix_mp.width, src.fmt.fmt.pix_mp.height,
			(char*)&src.fmt.fmt.pix_mp.pixelformat);
    }
  else {
    MSG("%s: vpe i/p: G_FMT: width = %u, height = %u, 4cc = %.4s\n",
        m_dev_name.c_str(), src.fmt.fmt.pix.width, src.fmt.fmt.pix.height,
        (char*)&src.fmt.fmt.pix.pixelformat);
  }

	memset(&rqbufs, 0, sizeof(rqbufs));
	rqbufs.count = m_num_buffers;
	rqbufs.type = src.type;
	rqbufs.memory = src.memory;

	ret = ioctl(m_fd, VIDIOC_REQBUFS, &rqbufs);
	if (ret < 0) {
		ERROR( "%s: vpe i/p: REQBUFS failed: %s\n", m_dev_name.c_str(), strerror(errno));
    return false;
  }
	m_num_buffers = rqbufs.count;

	return true;

}

bool VPEObj::vpe_output_init(int *export_fds)
{
	int ret;
	struct v4l2_format fmt;
	struct v4l2_requestbuffers rqbufs;
  // struct v4l2_plane		buf_planes[m_num_buffers][2];
	bzero(&fmt, sizeof fmt);
	fmt.type = dst.type;
	fmt.fmt.pix_mp.width = dst.width;
	fmt.fmt.pix_mp.height = dst.height;
	fmt.fmt.pix_mp.pixelformat = dst.fourcc;
	fmt.fmt.pix_mp.field = m_field;
	fmt.fmt.pix_mp.colorspace = dst.colorspace;
	fmt.fmt.pix_mp.num_planes = dst.coplanar ? 2 : 1;
  DBG("dst.fourcc = 0x%x", dst.fourcc);
  DBG("YUYV is 0x%x", V4L2_PIX_FMT_YUYV);
  DBG("AR24 is 0x%x", FOURCC_STR("AR24"));
  DBG("AB12 is 0x%x", FOURCC_STR("AB12"));
  DBG("dst.fourcc = 0x%x", fmt.fmt.pix_mp.pixelformat);

	ret = ioctl(m_fd, VIDIOC_S_FMT, &fmt);
	if (ret < 0) {
		ERROR( "%s: vpe o/p: S_FMT failed: %s\n", m_dev_name.c_str(), strerror(errno));
    return false;
  }
  DBG("dst.fourcc = 0x%x", fmt.fmt.pix_mp.pixelformat);
  dst.size = fmt.fmt.pix_mp.plane_fmt[0].sizeimage;
  // sleep(10);

  DBG("dst.size was set at %d", dst.size);
  sleep(1);
  dst.size_uv = fmt.fmt.pix_mp.plane_fmt[1].sizeimage;

	ret = ioctl(m_fd, VIDIOC_G_FMT, &fmt);
	if (ret < 0) {
		ERROR( "%s: vpe o/p: G_FMT_2 failed: %s\n", m_dev_name.c_str(), strerror(errno));
    return false;
  }

	MSG("%s: vpe o/p: G_FMT: width = %u, height = %u, 4cc = %.4s\n",
			 m_dev_name.c_str(), fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height,
			(char*)&fmt.fmt.pix_mp.pixelformat);

	memset(&rqbufs, 0, sizeof(rqbufs));
	rqbufs.count = m_num_buffers;
	rqbufs.type = dst.type;
	rqbufs.memory = dst.memory;

	ret = ioctl(m_fd, VIDIOC_REQBUFS, &rqbufs);
	if (ret < 0) {
		ERROR( "%s: vpe o/p: REQBUFS failed: %s\n", m_dev_name.c_str(), strerror(errno));
    return false;
  }

	m_num_buffers = rqbufs.count;

  dst.v4l2bufs = (struct v4l2_buffer **) malloc(rqbufs.count * sizeof(unsigned int));
  dst.v4l2planes = (struct v4l2_plane **) malloc(rqbufs.count * sizeof(unsigned int));
  for (int i = 0; i < m_num_buffers; i++) {
    dst.v4l2bufs[i] = (struct v4l2_buffer *) malloc(sizeof(struct v4l2_buffer));
    dst.v4l2planes[i] = (struct v4l2_plane *) calloc(2, sizeof(struct v4l2_plane));

    memset(&dst.v4l2planes[i][0], 0, sizeof(*dst.v4l2planes[i]));
    memset(dst.v4l2bufs[i], 0, sizeof(*dst.v4l2bufs[i]));

    dst.v4l2bufs[i]->type = dst.type;
    dst.v4l2bufs[i]->memory = dst.memory;
    dst.v4l2bufs[i]->length	= dst.coplanar ? 2 : 1;
    dst.v4l2bufs[i]->index = i;
    dst.v4l2bufs[i]->m.planes = &dst.v4l2planes[i][0];

    ret = ioctl(m_fd, VIDIOC_QUERYBUF, dst.v4l2bufs[i]);
    if (ret) {
        ERROR("VIDIOC_QUERYBUF failed: %s (%d)", strerror(errno), ret);
        return ret;
    }

    if (dst.memory == V4L2_MEMORY_MMAP) {
      dst.base_addr = (unsigned int **) calloc(m_num_buffers, sizeof(unsigned int));
      dst.base_addr[i] = (unsigned int *) mmap(NULL,
      dst.v4l2bufs[i]->m.planes[0].length, PROT_READ | PROT_WRITE, MAP_SHARED,
        m_fd, dst.v4l2bufs[i]->m.planes[0].m.mem_offset);
    }
    if (dst.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
    	dst.v4l2planes[i][0].length = dst.v4l2planes[i][0].bytesused = dst.size;
    	if (dst.coplanar)
    		dst.v4l2planes[i][1].length = dst.v4l2planes[i][1].bytesused = dst.size_uv;

    	dst.v4l2planes[i][0].data_offset = dst.v4l2planes[i][1].data_offset = 0;
      memset(&dst.v4l2planes[i][1], 0, sizeof(v4l2_plane));
      dst.v4l2bufs[i]->m.planes = &dst.v4l2planes[i][0];
      if (dst.memory == V4L2_MEMORY_DMABUF)
  	   dst.v4l2bufs[i]->m.planes[0].m.fd = export_fds[i];

     dst.v4l2bufs[i]->field = V4L2_FIELD_TOP;
     dst.v4l2bufs[i]->length = 1;
    }
    else if (dst.memory == V4L2_MEMORY_DMABUF) {
      dst.v4l2bufs[i]->m.fd = export_fds[i];
    }
  }

  MSG("\n##################");
  for (int j=0; j<m_num_buffers; j++)
    print_v4l2buffer(dst.v4l2bufs[j]);


	MSG("%s: vpe o/p: allocated buffers = %d\n", m_dev_name.c_str(), rqbufs.count);

	return true;
}

bool VPEObj::input_qbuf(int fd, int index){
  struct v4l2_buffer buf;
	struct v4l2_plane planes[2];

	memset(&buf, 0, sizeof buf);
	memset(&planes, 0, sizeof planes);

	buf.type = src.type;
	buf.memory = src.memory;
	buf.index = index;

	buf.field = m_field;
	if (src.coplanar)
		buf.length = 2;
	else
		buf.length = 1;

  if (src.type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
  	planes[0].length = planes[0].bytesused = src.size;
  	if (src.coplanar)
  		planes[1].length = planes[1].bytesused = src.size_uv;

  	planes[0].data_offset = planes[1].data_offset = 0;
    buf.m.planes = &planes[0];
	  buf.m.planes[0].m.fd = fd;
  }
  else {
    buf.m.fd = fd;
  }
  //gettimeofday(&buf.timestamp, NULL);

  int ret = ioctl(m_fd, VIDIOC_QBUF, &buf);
  if (ret) {
      ERROR("VIDIOC_QBUF failed: %s (%d)", strerror(errno), ret);
      return false;
  }
  return true;
}

bool VPEObj::output_qbuf(int index, int fd)
{
	int ret;
	struct v4l2_buffer *buf = NULL;
	// struct v4l2_plane planes[2];

	// memset(&buf, 0, sizeof buf);
	// memset(&planes, 0, sizeof planes);
  //
	// buf.type = dst.type;
	// buf.memory = dst.memory;
	// buf.index = index;
	// buf.m.planes = &planes[0];
	// if (dst.coplanar)
	// 	buf.length = 2;
	// else
	// 	buf.length = 1;
  //
  // if (dst.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
  // 	planes[0].length = planes[0].bytesused = dst.size;
  // 	if (dst.coplanar)
  // 		planes[1].length = planes[1].bytesused = dst.size_uv;
  //
  // 	planes[0].data_offset = planes[1].data_offset = 0;
  //   buf.m.planes = &planes[0];
	//   buf.m.planes[0].m.fd = fd;
  // }
  // else {
  //   buf.m.fd = fd;
  // }

  for (int i=0; i<m_num_buffers; i++) {
    // MSG("dst.v4l2bufs[i]->m.planes[0].m.fd = %d\nfd = %d", dst.v4l2bufs[i]->m.planes[0].m.fd, fd);
    if (dst.v4l2bufs[i]->m.planes[0].m.fd == fd)
      buf = dst.v4l2bufs[i];
  }
  if (!buf) {
    ERROR("While vpe output was queueing the buffer, no requested buffer" \
    " with fd = %d was found", fd);
    return false;
  }

	ret = ioctl(m_fd, VIDIOC_QBUF, buf);
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
    printf("vpe o/p: QBUF failed: %s\n", strerror(errno));
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

  if (dst.coplanar)
		buf.length = 2;
	else
		buf.length = 1;

	ret = ioctl(m_fd, VIDIOC_DQBUF, &buf);
	if (ret < 0) {
		ERROR("vpe o/p: DQBUF failed: %s\n", strerror(errno));
    return -1;
  }

  // print_v4l2buffer(dst.v4l2bufs[buf.index]);

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
    MSG("Streaming VPE Input");
  }

  ret = ioctl(m_fd, VIDIOC_STREAMON, &type);

  if (ret) {
      ERROR("VIDIOC_STREAMON failed: %s (%d)", strerror(errno), ret);
      return false;
  }

  return true;
}

/*
* Disable streaming for V4L2 capture device
*/
bool VPEObj::stream_off(int layer){
  enum v4l2_buf_type type;
  int ret;

  if (layer == 1) {
    type = (v4l2_buf_type) dst.type;
    MSG("Disable Streaming VPE Output");
  }
  else if (layer == 0) {
    type = (v4l2_buf_type) src.type;
    MSG("Disable Streaming VPE Input");
  }

  ret = ioctl(m_fd, VIDIOC_STREAMOFF, &type);

  if (ret) {
      ERROR("VIDIOC_STREAMOFF failed: %s (%d)", strerror(errno), ret);
      return false;
  }

  return true;
}

VPEObj::VPEObj(){
  default_parameters();
}


VPEObj::VPEObj(int src_w, int src_h, int src_bytes_per_pixel, int src_fourcc,
  int src_memory, int dst_w, int dst_h, int dst_bytes_per_pixel, int dst_fourcc,
  int dst_memory, int num_buffers)
{
  default_parameters();
  m_num_buffers = num_buffers;

  src.width = src_w;
  src.height = src_h;
  src.bytes_pp = src_bytes_per_pixel;
  src.size = src_w*src_h*src_bytes_per_pixel;
  src.fourcc = src_fourcc;
  src.memory = src_memory;

  dst.width = dst_w;
  dst.height = dst_h;
  dst.bytes_pp = dst_bytes_per_pixel;
  dst.size = dst_w*dst_h*dst_bytes_per_pixel;
  dst.fourcc = dst_fourcc;
  dst.memory = dst_memory;
}


VPEObj::~VPEObj(){
    free(src.v4l2bufs);
    free(dst.v4l2bufs);
    close(m_fd);
    return;
}
