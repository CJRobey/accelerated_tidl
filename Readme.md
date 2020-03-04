##TODO
* Solve issues with overlay plane not updating
* Give parameters to display init that allow a setting of the alpha-blending value
* Take the output of the VPE into the DSS for rescaling consistency
* Integrate a text plane for classification
* Integrate a semantic segmentation example (should be pretty easy with current configuration)

###Stretch
* EXPBUF MMAP allocation on the capture side to VPE (not sure if this actually works). This would remove the need to memcpy USB capture data to the VPE and would keep V4L2 calls consistent between the usage of VIP and USB.
* Implement better GUI functions

