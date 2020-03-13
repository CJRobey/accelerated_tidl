/******************************************************************************
 * Copyright (c) 2018, Texas Instruments Incorporated - http://www.ti.com/
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
#include <signal.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cassert>
#include <string>
#include <functional>
#include <algorithm>
#include <time.h>
#include <unistd.h>

#include <queue>
#include <vector>
#include <cstdio>
#include <string>
#include <chrono>

#include "capturevpedisplay.h"
#include "executor.h"
#include "execution_object.h"
#include "execution_object_pipeline.h"
#include "configuration.h"
#include "../common/object_classes.h"
#include "../common/utils.h"
#include "../common/video_utils.h"
#include "save_utils.h"

using namespace std;
using namespace tidl;
using namespace cv;
using namespace chrono;


#define NUM_VIDEO_FRAMES  100
#define SSD_DEFAULT_CONFIG    "jdetnet_voc"
#define SSD_DEFAULT_INPUT_FRAMES (1)
#define SSD_DEFAULT_OBJECT_CLASSES_LIST_FILE "./configs/jdetnet_voc_objects.json"
#define SSD_DEFAULT_OUTPUT_PROB_THRESHOLD  25

#define SEG_NUM_VIDEO_FRAMES  300
#define SEG_DEFAULT_CONFIG    "jseg21_tiscapes"
#define SEG_DEFAULT_INPUT     "../test/testvecs/input/000100_1024x512_bgr.y"
#define SEG_DEFAULT_INPUT_FRAMES  (9)
#define SEG_DEFAULT_OBJECT_CLASSES_LIST_FILE "./configs/jseg21_objects.json"

#define MAX_NUM_ROI 4

#ifdef TWO_ROIs
#define RES_X 400
#define RES_Y 300
#define NUM_ROI_X 2
#define NUM_ROI_Y 1
#define X_OFFSET 0
#define X_STEP   176
#define Y_OFFSET 52
#define Y_STEP   224
#else
#define RES_X 480
#define RES_Y 480
#define NUM_ROI_X 1
#define NUM_ROI_Y 1
#define X_OFFSET 10
#define X_STEP   460
#define Y_OFFSET 10
#define Y_STEP   460
#endif
int IMAGE_CLASSES_NUM = 0;
#define MAX_CLASSES 10
#define MAX_SELECTED_ITEMS 10
std::string labels_classes[MAX_CLASSES];
int selected_items_size = 0;
int selected_items[MAX_SELECTED_ITEMS];

#define NUM_ROI (NUM_ROI_X * NUM_ROI_Y)


/* Enable this macro to record individual output files and */
/* resized, cropped network input files                    */
//#define DEBUG_FILES

std::unique_ptr<ObjectClasses> object_classes;
uint32_t orig_width;
uint32_t orig_height;
uint32_t num_frames_file;

static int tf_postprocess(uchar *in, int out_size, int size, int roi_idx,
                          int frame_idx, int f_id);
static int ShowRegion(int roi_history[]);
bool tf_expected_id(int id);
// from most recent to oldest at top indices
static int selclass_history[MAX_NUM_ROI][3];

bool RunConfiguration(const cmdline_opts_t& opts);
Executor* CreateExecutor(DeviceType dt, uint32_t num, const Configuration& c,
                         int layers_group_id);
Executor* CreateExecutor(DeviceType dt, uint32_t num, const Configuration& c);
bool ReadFrameInput(ExecutionObjectPipeline& eop, uint32_t frame_idx,
               const Configuration& c, const cmdline_opts_t& opts,
               CamDisp &cap);
bool ReadFrameIO(ExecutionObjectPipeline& eop, uint32_t frame_idx,
              const Configuration& c, const cmdline_opts_t& opts,
              CamDisp &cap);
bool WriteFrameOutputSSD(const ExecutionObjectPipeline& eop,
                      const Configuration& c, const cmdline_opts_t& opts,
                      const CamDisp& cam, float fps);
// Create frame overlayed with pixel-level segmentation
bool WriteFrameOutputSEG(const ExecutionObjectPipeline &eop,
                      const Configuration& c,
                      const cmdline_opts_t& opts, const CamDisp& cam, float fps);
void WriteFrameOutputCLASS(const ExecutionObjectPipeline* eop, CamDisp &cap,
                  const Configuration& c, uint32_t frame_idx, float fps, uint32_t num_eops,
                  uint32_t num_eves, uint32_t num_dsps);
void OverlayFPS(Mat fps_screen, const Configuration& c, float fps, double scale);

static void DisplayHelp();
void CreateMask(uchar *classes, uchar *ma, uchar *mb, uchar *mg, uchar* mr,
                int channel_size);

/***************************************************************/
/* Slider to control detection confidence level                */
/***************************************************************/
// static void on_trackbar( int slider_id, void *inst )
// {
//   //This function is invoked on every slider move.
//   //No action required, since prob_slider is automatically updated.
//   //But, for any additional operation on slider move, this is the place to insert code.
// }


Rect rect_crop[NUM_ROI];

// TODO : Get rid of this
void setup_rect_crop(){
  for (int y = 0; y < NUM_ROI_Y; y ++) {
     for (int x = 0; x < NUM_ROI_X; x ++) {
        rect_crop[y * NUM_ROI_X + x] = Rect(X_OFFSET + x * X_STEP,
                                           Y_OFFSET + y * Y_STEP, X_STEP, Y_STEP);
        std::cout << "Rect[" << X_OFFSET + x * X_STEP << ", "
                  << Y_OFFSET + y * Y_STEP << "]" << std::endl;
     }
  }
}
static int get_classindex(std::string str2find)
{
  if(selected_items_size >= MAX_SELECTED_ITEMS)
  {
     std::cout << "Max number of selected classes is reached! (" << selected_items_size << ")!" << std::endl;
     return -1;
  }
  for (int i = 0; i < IMAGE_CLASSES_NUM; i ++)
  {
    if(labels_classes[i].compare(str2find) == 0)
    {
      selected_items[selected_items_size ++] = i;
      return i;
    }
  }
  std::cout << "Not found: " << str2find << std::endl << std::flush;
  return -1;
}


int populate_selected_items (const char *filename)
{
  ifstream file(filename);
  if(file.is_open())
  {
    string inputLine;

    while (getline(file, inputLine) )                 //while the end of file is NOT reached
    {
      int res = get_classindex(inputLine);
      std::cout << "Searching for " << inputLine  << std::endl;
      if(res >= 0) {
        std::cout << "Found: " << res << std::endl;
      } else {
        std::cout << "Not Found: " << res << std::endl;
      }
    }
    file.close();
  }
#if 0
  std::cout << "==Total of " << selected_items_size << " items!" << std::endl;
  for (int i = 0; i < selected_items_size; i ++)
    std::cout << i << ") " << selected_items[i] << std::endl;
#endif
  return selected_items_size;
}

void populate_labels (const char *filename)
{
  ifstream file(filename);
  if(file.is_open())
  {
    string inputLine;

    while (getline(file, inputLine) )                 //while the end of file is NOT reached
    {
      labels_classes[IMAGE_CLASSES_NUM ++] = string(inputLine);
    }
    file.close();
  }
#if 1
  std::cout << "==Total of " << IMAGE_CLASSES_NUM << " items!" << std::endl;
  for (int i = 0; i < IMAGE_CLASSES_NUM; i ++)
    std::cout << i << ") " << labels_classes[i] << std::endl;
#endif
}

int main(int argc, char *argv[])
{
    // Catch ctrl-c to ensure a clean exit
    signal(SIGABRT, exit);
    signal(SIGTERM, exit);
    setup_rect_crop();

    // If there are no devices capable of offloading TIDL on the SoC, exit
    uint32_t num_eves = Executor::GetNumDevices(DeviceType::EVE);
    uint32_t num_dsps = Executor::GetNumDevices(DeviceType::DSP);
    if (num_eves == 0 && num_dsps == 0)
    {
        cout << "ssd_multibox requires EVE or DSP for execution." << endl;
        return EXIT_SUCCESS;
    }

    // Process arguments
    cmdline_opts_t opts;
    if (! ProcessArgs(argc, argv, opts))
    {
        DisplayHelp();
        exit(EXIT_SUCCESS);
    }

    MSG("net_type %s\nconfig %s\nobject_classes_list_file %s\nnum_eves %d\n" \
        "num_dsps %d\noutput_prob_threshold %d", opts.net_type.c_str(), opts.config.c_str(),
        opts.object_classes_list_file.c_str(), opts.num_eves, opts.num_dsps,
        opts.output_prob_threshold);
    // choose the defaults based on the kind of network of any field that was
    // not populated
    if (opts.net_type == "ssd") {

      if (opts.config == "") opts.config = SSD_DEFAULT_CONFIG;
      if (opts.object_classes_list_file == "") opts.object_classes_list_file =
        SSD_DEFAULT_OBJECT_CLASSES_LIST_FILE;
      if (!opts.num_eves) opts.num_eves = num_eves > 0 ? 1 : 0;
      if (!opts.num_dsps) opts.num_dsps = num_dsps > 0 ? 1 : 0;
      if (!opts.output_prob_threshold) opts.output_prob_threshold =
        SSD_DEFAULT_OUTPUT_PROB_THRESHOLD;
    }
    else if (opts.net_type == "seg") {
      if (opts.config == "") opts.config = SEG_DEFAULT_CONFIG;
      if (opts.object_classes_list_file == "") opts.object_classes_list_file =
        SEG_DEFAULT_OBJECT_CLASSES_LIST_FILE;
    }
    MSG("net_type %s\nconfig %s\nobject_classes_list_file %s\nnum_eves %d\n" \
        "num_dsps %d\noutput_prob_threshold %d",opts.net_type.c_str(), opts.config.c_str(),
        opts.object_classes_list_file.c_str(), opts.num_eves, opts.num_dsps,
        opts.output_prob_threshold);

    assert(opts.num_dsps != 0 || opts.num_eves != 0);
    if (opts.num_frames == 0)
        opts.num_frames = (opts.is_camera_input || opts.is_video_input) ?
                          NUM_VIDEO_FRAMES :
                          ((opts.input_file == SEG_DEFAULT_INPUT) ?
                           SEG_DEFAULT_INPUT_FRAMES : 1);
    cout << "Input: " << opts.input_file << endl;

    if (opts.net_type == "seg" || opts.net_type == "ssd") {
            // Get object classes list
      object_classes = std::unique_ptr<ObjectClasses>(
                               new ObjectClasses(opts.object_classes_list_file));
      if (object_classes->GetNumClasses() == 0)
      {
          cout << "No object classes defined for this config." << endl;
          return EXIT_FAILURE;
      }
    }
    else {
      populate_labels(opts.object_classes_list_file.c_str());
      populate_selected_items(opts.object_classes_list_file.c_str());
    }

    // Run network
    bool status = RunConfiguration(opts);
    if (!status)
    {
        cout << opts.net_type << " FAILED" << endl;
        return EXIT_FAILURE;
    }

    cout << opts.net_type << " PASSED" << endl;
    return EXIT_SUCCESS;
}

bool RunConfiguration(const cmdline_opts_t& opts)
{
    // int prob_slider     = opts.output_prob_threshold;
    // Read the TI DL configuration file
    Configuration c;
    std::string config_file;
    // TODO : clean up
    if (opts.net_type == "class")
      config_file = opts.config;
    else {
      config_file = "../test/testvecs/config/infer/tidl_config_"
                                + opts.config + ".txt";
    }

    bool status = c.ReadFromFile(config_file);
    if (!status)
    {
        cerr << "Error in configuration file: " << config_file << endl;
        return false;
    }
    c.enableApiTrace = opts.verbose;
    if (opts.num_eves == 0 || opts.num_dsps == 0)
        c.runFullNet = true;

    /* alpha_value of the second plane. 0 makes it clear and 255 makes it opaque
     * cam_w, cam_h should be just over the model
     */
    CamDisp cam;
    int cap_w, cap_h, alpha_value;

    // optsarg is const, quick_display should be set to false if it was
    // triggered by the user when not using the segmentation demo.
    bool quick_display = opts.quick_display;
    if (opts.net_type == "seg") {
      cap_w = 1024;
      cap_h = 576;
      alpha_value = 150;
    }
    else if (opts.net_type == "ssd") {
      cap_w = 800;
      cap_h = 448;
      alpha_value = 255;
      quick_display = false;
    }
    else if (opts.net_type == "class") {
      cap_w = 640;
      cap_h = 480;
      alpha_value = 255;
      quick_display = false;
    }

    // The quick display setting looks better with a darker second layer
    if (quick_display) alpha_value = 215;
    bool usb_capture = true;

    if ((opts.input_file != "") && opts.input_file.length() == 1) {
      string device_name = "/dev/video" + opts.input_file;
      cam = CamDisp(cap_w, cap_h, c.inWidth, c.inHeight, alpha_value,
        device_name, usb_capture, opts.net_type, quick_display);
    }
    else {
      cam = CamDisp(cap_w, cap_h, c.inWidth, c.inHeight, alpha_value,
        "/dev/video1", usb_capture, opts.net_type, quick_display);
    }
    cam.init_capture_pipeline();

    try
    {
        // Create Executors with the approriate core type, number of cores
        // and configuration specified
        // EVE will run layersGroupId 1 in the network, while
        // DSP will run layersGroupId 2 in the network
        Executor *e_dsp, *e_eve;
        MSG("Beginning to create executors for net_type %s", opts.net_type.c_str());
        if (opts.net_type == "ssd" || opts.net_type == "class") {
          e_dsp = CreateExecutor(DeviceType::DSP, opts.num_dsps, c, 2);
          e_eve = CreateExecutor(DeviceType::EVE, opts.num_eves, c, 1);
        }
        else {
          e_eve = CreateExecutor(DeviceType::EVE, opts.num_eves, c);
          e_dsp = CreateExecutor(DeviceType::DSP, opts.num_dsps, c);
        }
        vector<ExecutionObjectPipeline *> eops;
        if (e_eve != nullptr && e_dsp != nullptr && (opts.net_type != "seg"))
        {
            // Construct ExecutionObjectPipeline that utilizes multiple
            // ExecutionObjects to process a single frame, each ExecutionObject
            // processes one layerGroup of the network
            //
            // Pipeline depth can enable more optimized pipeline execution:
            // Given one EVE and one DSP as an example, with different
            //     pipeline_depth, we have different execution behavior:
            // If pipeline_depth is set to 1,
            //    we create one EOP: eop0 (eve0, dsp0)
            //    pipeline execution of multiple frames over time is as follows:
            //    --------------------- time ------------------->
            //    eop0: [eve0...][dsp0]
            //    eop0:                [eve0...][dsp0]
            //    eop0:                               [eve0...][dsp0]
            //    eop0:                                              [eve0...][dsp0]
            // If pipeline_depth is set to 2,
            //    we create two EOPs: eop0 (eve0, dsp0), eop1(eve0, dsp0)
            //    pipeline execution of multiple frames over time is as follows:
            //    --------------------- time ------------------->
            //    eop0: [eve0...][dsp0]
            //    eop1:          [eve0...][dsp0]
            //    eop0:                   [eve0...][dsp0]
            //    eop1:                            [eve0...][dsp0]
            // Additional benefit of setting pipeline_depth to 2 is that
            //    it can also overlap host ReadFrameInput() with device processing:
            //    --------------------- time ------------------->
            //    eop0: [RF][eve0...][dsp0]
            //    eop1:     [RF]     [eve0...][dsp0]
            //    eop0:                    [RF][eve0...][dsp0]
            //    eop1:                             [RF][eve0...][dsp0]
            uint32_t pipeline_depth = 2;  // 2 EOs in EOP -> depth 2
            for (uint32_t j = 0; j < pipeline_depth; j++)
                for (uint32_t i = 0; i < max(opts.num_eves, opts.num_dsps); i++)
                    eops.push_back(new ExecutionObjectPipeline(
                      {(*e_eve)[i%opts.num_eves], (*e_dsp)[i%opts.num_dsps]}));
        }
        else if (opts.net_type == "ssd")
        {
            // Construct ExecutionObjectPipeline that utilizes a
            // ExecutionObject to process a single frame, each ExecutionObject
            // processes the full network
            //
            // Use duplicate EOPs to do double buffering on frame input/output
            //    because each EOP has its own set of input/output buffers,
            //    so that host ReadFrameInput() can overlap device processing
            // Use one EO as an example, with different buffer_factor,
            //    we have different execution behavior:
            // If buffer_factor is set to 1 -> single buffering
            //    we create one EOP: eop0 (eo0)
            //    pipeline execution of multiple frames over time is as follows:
            //    --------------------- time ------------------->
            //    eop0: [RF][eo0.....][WF]
            //    eop0:                   [RF][eo0.....][WF]
            //    eop0:                                     [RF][eo0.....][WF]
            // If buffer_factor is set to 2 -> double buffering
            //    we create two EOPs: eop0 (eo0), eop1(eo0)
            //    pipeline execution of multiple frames over time is as follows:
            //    --------------------- time ------------------->
            //    eop0: [RF][eo0.....][WF]
            //    eop1:     [RF]      [eo0.....][WF]
            //    eop0:                   [RF]  [eo0.....][WF]
            //    eop1:                             [RF]  [eo0.....][WF]
            uint32_t buffer_factor = 2;  // set to 1 for single buffering
            for (uint32_t j = 0; j < buffer_factor; j++)
            {
                for (uint32_t i = 0; i < opts.num_eves; i++)
                    eops.push_back(new ExecutionObjectPipeline({(*e_eve)[i]}));
                for (uint32_t i = 0; i < opts.num_dsps; i++)
                    eops.push_back(new ExecutionObjectPipeline({(*e_dsp)[i]}));
            }
        }
        else if (opts.net_type == "seg") {
          // Get ExecutionObjects from Executors
          vector<ExecutionObject*> eos;
          for (uint32_t i = 0; i < opts.num_eves; i++) eos.push_back((*e_eve)[i]);
          for (uint32_t i = 0; i < opts.num_dsps; i++) eos.push_back((*e_dsp)[i]);
          MSG("eos created of size %d", eos.size());
          uint32_t num_eos = eos.size();

          // Use duplicate EOPs to do double buffering on frame input/output
          //    because each EOP has its own set of input/output buffers,
          //    so that host ReadFrameInput() can be overlapped with device processing
          // Use one EO as an example, with different buffer_factor,
          //    we have different execution behavior:
          // If buffer_factor is set to 1 -> single buffering
          //    we create one EOP: eop0 (eo0)
          //    pipeline execution of multiple frames over time is as follows:
          //    --------------------- time ------------------->
          //    eop0: [RF][eo0.....][WF]
          //    eop0:                   [RF][eo0.....][WF]
          //    eop0:                                     [RF][eo0.....][WF]
          // If buffer_factor is set to 2 -> double buffering
          //    we create two EOPs: eop0 (eo0), eop1(eo0)
          //    pipeline execution of multiple frames over time is as follows:
          //    --------------------- time ------------------->
          //    eop0: [RF][eo0.....][WF]
          //    eop1:     [RF]      [eo0.....][WF]
          //    eop0:                   [RF]  [eo0.....][WF]
          //    eop1:                             [RF]  [eo0.....][WF]
          uint32_t buffer_factor = 2;  // set to 1 for single buffering
          for (uint32_t j = 0; j < buffer_factor; j++)
              for (uint32_t i = 0; i < num_eos; i++)
                  eops.push_back(new ExecutionObjectPipeline({eos[i]}));
          MSG("eops of size %d created", eops.size());
        }
        else {
          ERROR("NETWORK NOT INITIALIZED");
          sleep(2);
          return false;
        }
        uint32_t num_eops = eops.size();

        // Allocate input/output memory for each EOP
        AllocateMemory(eops);
        chrono::time_point<chrono::steady_clock> tloop0, tloop1;
        tloop0 = chrono::steady_clock::now();

        // Process frames with available eops in a pipelined manner
        // additional num_eops iterations to flush pipeline (epilogue)
        high_resolution_clock::time_point wrStart;
        // going to keep a running average of the FPS
        int ave = 20;
        float fps_bank[ave];
        float fps = 0;
        for (uint32_t frame_idx = 0;
             frame_idx < (int) opts.num_frames + num_eops; frame_idx++)
        {
            ExecutionObjectPipeline* eop = eops[frame_idx % num_eops];
            // Wait for previous frame on the same eop to finish processing
            if (eop->ProcessFrameWait()) {
              auto fpsCount = duration_cast<milliseconds>(high_resolution_clock::now() - wrStart);
              fps_bank[(frame_idx-num_eops)%ave] = (1000.00/(float)fpsCount.count());

              // Take the average fps
              if ((frame_idx-num_eops) >= (unsigned int) ave) {
                fps = 0;
                for (int f=0; f<ave; f++)
                  fps += fps_bank[f]/ave;
              }

              wrStart = high_resolution_clock::now();
              if (opts.net_type == "ssd")
                WriteFrameOutputSSD(*eop, c, opts, cam, fps);
              else if ((opts.net_type == "seg") && (!quick_display)) {
                WriteFrameOutputSEG(*eop, c, opts, cam, fps);
              }
              else if (opts.net_type == "class") {
                WriteFrameOutputCLASS(eop, cam, c, frame_idx, fps, num_eops, opts.num_eves, opts.num_dsps);
              }

              cam.disp_frame();

              if (opts.verbose) {
                auto wrStop = high_resolution_clock::now();
                auto wrDuration = duration_cast<milliseconds>(wrStop - wrStart);
                DBG("Overlay write time: %d ms", (int) wrDuration.count());
              }
            }
            // Read a frame and start processing it with current eo
            auto rdStart = high_resolution_clock::now();
           if (opts.net_type != "seg" || !quick_display) {
              ReadFrameInput(*eop, frame_idx, c, opts, cam);
           }
           else {
              ReadFrameIO(*eop, frame_idx, c, opts, cam);
           }
          auto rdStop = high_resolution_clock::now();
          auto rdDuration = duration_cast<milliseconds>(rdStop - rdStart);
          cout << "One buffer read time:" << rdDuration.count() << " ms" << endl;
          eop->ProcessFrameStartAsync();
        }
        cam.turn_off();
        tloop1 = chrono::steady_clock::now();
        chrono::duration<float> elapsed = tloop1 - tloop0;
        cout << "Loop total time (including read/write/opencv/print/etc): "
                  << setw(6) << setprecision(4)
                  << (elapsed.count() * 1000) << "ms" << endl;

        FreeMemory(eops);
        for (auto eop : eops)  delete eop;
        delete e_eve;
        delete e_dsp;
    }
    catch (tidl::Exception &e)
    {
        cerr << e.what() << endl;
        status = false;
    }

    return status;
}

// SSD: Create an Executor with the specified type and number of EOs
Executor* CreateExecutor(DeviceType dt, uint32_t num, const Configuration& c,
                         int layers_group_id)
{
    if (num == 0) return nullptr;

    DeviceIds ids;
    for (uint32_t i = 0; i < num; i++)
        ids.insert(static_cast<DeviceId>(i));

    Executor* e = new Executor(dt, ids, c, layers_group_id);
    assert(e != nullptr);
    return e;
}

// SEG: Create an Executor with the specified type and number of EOs
Executor* CreateExecutor(DeviceType dt, uint32_t num, const Configuration& c)
{
    if (num == 0) return nullptr;

    DeviceIds ids;
    for (uint32_t i = 0; i < num; i++)
        ids.insert(static_cast<DeviceId>(i));

    return new Executor(dt, ids, c);
}


/* This function will read the captured frame into the input buffer of TIDL.
 * However, the output buffer is left alone for TIDL to allocate and manage the
 * memory.
 */
bool ReadFrameInput(ExecutionObjectPipeline& eop, uint32_t frame_idx,
               const Configuration& c, const cmdline_opts_t& opts,
               CamDisp &cap)
{
    if ((uint32_t)frame_idx >= opts.num_frames)
        return false;

    eop.SetFrameIndex(frame_idx);
    char *in_ptr = (char *) cap.grab_image();
    int channel_size = c.inWidth*c.inHeight;

    /* More efficient method after testing */
    Mat pic(cvSize(c.inWidth, c.inHeight), CV_8UC4, in_ptr);
    Mat channels[4];
    split(pic, channels);
    char*  frame_buffer = eop.GetInputBufferPtr();

    auto cpyStart = high_resolution_clock::now();
    memcpy(frame_buffer, channels[0].ptr(), channel_size);
    memcpy(frame_buffer+channel_size, channels[1].ptr(), channel_size);
    memcpy(frame_buffer+(2*channel_size), channels[2].ptr(), channel_size);
    auto cpyStop = high_resolution_clock::now();
    auto cpyDuration = duration_cast<milliseconds>(cpyStop - cpyStart);
    DBG("VPE -> TIDL memcpy time: %d ms", (int) cpyDuration.count());
    assert (frame_buffer != nullptr);
    return true;
}

/* This function will read the captured frame into the input buffer of TIDL. It
 * will also send the output of TIDL (still allocated/managed by TIDL) directly
 * to the display system. This is an optimized display overlay method for
 * something like a segmentation neural network.
 */
bool ReadFrameIO(ExecutionObjectPipeline& eop, uint32_t frame_idx,
               const Configuration& c, const cmdline_opts_t& opts,
               CamDisp &cap)
{
    if ((uint32_t)frame_idx >= opts.num_frames)
        return false;

    eop.SetFrameIndex(frame_idx);
    char *in_ptr = (char *) cap.grab_image();
    int channel_size = c.inWidth*c.inHeight;

    /* More efficient method after testing */
    Mat pic(cvSize(c.inWidth, c.inHeight), CV_8UC4, in_ptr);
    Mat channels[4];
    split(pic, channels);
    char*  frame_buffer = eop.GetInputBufferPtr();
    memcpy(frame_buffer, channels[0].ptr(), channel_size);
    memcpy(frame_buffer+channel_size, channels[1].ptr(), channel_size);
    memcpy(frame_buffer+(2*channel_size), channels[2].ptr(), channel_size);

    ArgInfo in = {ArgInfo(frame_buffer, channel_size*3)};
    ArgInfo out = {ArgInfo(cap.drm_device.plane_data_buffer[1][cap.frame_num]->buf_mem_addr[0], channel_size)};
    // ArgInfo out = {ArgInfo(eop.GetOutputBufferPtr(), channel_size)}
    eop.SetInputOutputBuffer(in, out);
    assert (frame_buffer != nullptr);
    return true;
}

/* This function is ultimately just going to write a couple of bounding boxes
 * directly onto the second plane of the DSS's buffer. When the disp_obj()
 * function is called, the rectangles will be displayed.
 */
bool WriteFrameOutputSSD(const ExecutionObjectPipeline& eop,
                      const Configuration& c, const cmdline_opts_t& opts,
                      const CamDisp& cam, float fps)
{
    // Asseemble original frame
    int width  = c.inWidth;
    int height = c.inHeight;
    float confidence_value = 30;
    // int channel_size = width * height;
    Mat frame;

    /* omap_bo_map is grabbing the pointer to the memory allocated by plane #2
     * of the DSS. It then writes over that plane as it waits to be displayed.
     */

    // clear the old rectangles
    memset(cam.drm_device.plane_data_buffer[1][cam.frame_num]->buf_mem_addr[0], 0, height*width*4);

    /* Data is being read in as bgra - thus the user may control the alpha
     * values either from this write function or by passing in the alpha value
     * to the initializer of the CamDisp object. Value go from 0 (totally clear)
     * to 255 (opaque)
     */
    frame = Mat(height, width, CV_8UC4, cam.drm_device.plane_data_buffer[1][cam.frame_num]->buf_mem_addr[0]);

    // Draw boxes around classified objects
    float *out = (float *) eop.GetOutputBufferPtr();
    int num_floats = eop.GetOutputBufferSizeInBytes() / sizeof(float);
    for (int i = 0; i < num_floats / 7; i++)
    {
        int index = (int)    out[i * 7 + 0];
        if (index < 0)  break;

        float score =        out[i * 7 + 2];
        if (score * 100 < confidence_value)  continue;

        int   label = (int)  out[i * 7 + 1];
        int   xmin  = (int) (out[i * 7 + 3] * width);
        int   ymin  = (int) (out[i * 7 + 4] * height);
        int   xmax  = (int) (out[i * 7 + 5] * width);
        int   ymax  = (int) (out[i * 7 + 6] * height);

        const ObjectClass& object_class = object_classes->At(label);

        // for now, we really just want the pedestrian label
        if (object_class.label != "pedestrian")
          continue;

        int thickness = 1;
        double scale = 0.6;
        int baseline = 0;

        Size text_size = getTextSize(object_class.label, FONT_HERSHEY_DUPLEX, scale,
                                    thickness, &baseline);
        baseline += thickness;

        if (opts.verbose) {
            printf("%2d: (%d, %d) -> (%d, %d): %s, score=%f\n",
               i, xmin, ymin, xmax, ymax, object_class.label.c_str(), score);
        }

        int alpha = 255;
        if (xmin < 0)       xmin = 0;
        if (ymin < 0)       ymin = 0;
        if (xmax > width)   xmax = width;
        if (ymax > height)  ymax = height;
        cv::rectangle(frame, Point(xmin, ymin), Point(xmax, ymax),
                      Scalar(object_class.color.blue,
                             object_class.color.green,
                             object_class.color.red, alpha), 2);

       // place the name of the class at the botton of the box
       cv::rectangle(frame, Point(xmin,ymax) + Point(0, baseline),
             Point(xmin,ymax) + Point(text_size.width,
             -text_size.height) , Scalar(0,0,0,alpha), -1);
       cv::putText(frame, object_class.label, Point(xmin,ymax),
                   FONT_HERSHEY_DUPLEX, scale, Scalar(255,255,255,alpha), thickness);

        MSG("%s class blue %d, green %d, red %d", object_class.label.c_str(), object_class.color.blue,
            object_class.color.green, object_class.color.red);
    }
    OverlayFPS(frame, c, fps, 1);

    return true;
}

// Create Overlay mask for pixel-level segmentation
void CreateMask(uchar *classes, uchar *ma, uchar *mb, uchar *mg, uchar* mr,
                int channel_size)
{
    for (int i = 0; i < channel_size; i++)
    {
        const ObjectClass& object_class = object_classes->At(classes[i]);
        ma[i] = 255;
        mb[i] = object_class.color.blue;
        mg[i] = object_class.color.green;
        mr[i] = object_class.color.red;
    }
}

// Create frame overlayed with pixel-level segmentation
bool WriteFrameOutputSEG(const ExecutionObjectPipeline &eop,
                      const Configuration& c,
                      const cmdline_opts_t& opts, const CamDisp& cap, float fps)
{
    unsigned char *out = (unsigned char *) eop.GetOutputBufferPtr();
    int width          = c.inWidth;
    int height         = c.inHeight;
    int channel_size   = width * height;
    uint16_t *disp_data = (uint16_t *)
      cap.drm_device.plane_data_buffer[1][cap.disp_frame_num]->buf_mem_addr[0];

    // Color fmt is 0bXXXXRRRRGGGGBBBB
    for (int i = 0; i < channel_size; i++) {
      switch (out[i]) {
        case 0: // background class
          disp_data[i] = out[i];
          break;
        case 1: // road class
          disp_data[i] = 0x00F0;
          break;
        case 2: // pedestrian class
          disp_data[i] = 0x0F00;
          break;
        case 3: // road sign class
          disp_data[i] = 0x000F;
          break;
        case 4: // vehicle class
          disp_data[i] = 0x0FF0;
          break;
        default:
          disp_data[i] = 0x0000;
        }
    }
    Mat frame(c.inHeight, c.inWidth, CV_16UC1, disp_data);
    OverlayFPS(frame, c, fps, 1);
    return true;
}

void WriteFrameOutputCLASS(const ExecutionObjectPipeline* eop, CamDisp &cam,
                  const Configuration& c, uint32_t frame_idx, float fps, uint32_t num_eops,
                  uint32_t num_eves, uint32_t num_dsps)
{
  int width  = c.inWidth;
  int height = c.inHeight;

  /* omap_bo_map is grabbing the pointer to the memory allocated by plane #2
   * of the DSS. It then writes over that plane as it waits to be displayed.
   */
  /* clear the classes */
  memset(cam.drm_device.plane_data_buffer[1][cam.frame_num]->buf_mem_addr[0], 0, height*width*4);

  /* Data is being read in as bgra - thus the user may control the alpha
   * values either from this write function or by passing in the alpha value
   * to the initializer of the CamDisp object. Value go from 0 (totally clear)
   * to 255 (opaque)
   */
  Mat frame = Mat(height, width, CV_8UC4, cam.drm_device.plane_data_buffer[1][cam.frame_num]->buf_mem_addr[0]);

  int f_id = eop->GetFrameIndex();
  int curr_roi = f_id % NUM_ROI;
  int is_object = tf_postprocess((uchar*) eop->GetOutputBufferPtr(),
                                 eop->GetOutputBufferSizeInBytes(),
                               IMAGE_CLASSES_NUM, curr_roi, frame_idx, f_id);
  int alpha = 255;
  double scale = 0.6;

  selclass_history[curr_roi][2] = selclass_history[curr_roi][1];
  selclass_history[curr_roi][1] = selclass_history[curr_roi][0];
  selclass_history[curr_roi][0] = is_object;
  for (int r = 0; r < NUM_ROI; r++)
  {
    int rpt_id =  ShowRegion(selclass_history[r]);
    if(rpt_id >= 0)
    {
      int thickness = 1;
      int baseline = 0;

      Size text_size = getTextSize(labels_classes[rpt_id], FONT_HERSHEY_DUPLEX,
        scale, thickness, &baseline);
      baseline += thickness;
      // place the name of the class at the botton of the box
      cv::rectangle(frame, Point(0,c.inHeight),
             Point(text_size.width, c.inHeight-text_size.height-baseline),
             Scalar(0,0,0,alpha), -1);
      cv::putText(frame, labels_classes[rpt_id], Point(0,c.inHeight-baseline),
                  FONT_HERSHEY_DUPLEX, scale, Scalar(255,255,255,alpha),
                  thickness);
    }
  }
  OverlayFPS(frame, c, fps, 0.45);
}

int ShowRegion(int roi_history[])
{
  if((roi_history[0] >= 0) && (roi_history[0] == roi_history[1])) return roi_history[0];
  if((roi_history[0] >= 0) && (roi_history[0] == roi_history[2])) return roi_history[0];
  if((roi_history[1] >= 0) && (roi_history[1] == roi_history[2])) return roi_history[1];
  return -1;
}

// Function to filter all the reported decisions
bool tf_expected_id(int id)
{
   // Filter out unexpected IDs
   for (int i = 0; i < selected_items_size; i ++)
   {
       if(id == selected_items[i]) return true;
   }
   return false;
}
//Temporal averaging
int TOP_CANDIDATES = 3;
int tf_postprocess(uchar *in, int out_size, int size, int roi_idx,
                   int frame_idx, int f_id)
{
  //prob_i = exp(TIDL_Lib_output_i) / sum(exp(TIDL_Lib_output))
  // sort and get k largest values and corresponding indices
  const int k = TOP_CANDIDATES;
  int rpt_id = -1;
  // Tensorflow trained network outputs 1001 probabilities,
  // with 0-index being background, thus we need to subtract 1 when
  // reporting classified object from 1000 categories
  int background_offset = out_size == 1001 ? 1 : 0;

  typedef std::pair<uchar, int> val_index;
  auto cmp = [](val_index &left, val_index &right) { return left.first > right.first; };
  std::priority_queue<val_index, std::vector<val_index>, decltype(cmp)> queue(cmp);
  // initialize priority queue with smallest value on top
  for (int i = 0; i < k; i++) {
    queue.push(val_index(in[i], i));
  }
  // for rest input, if larger than current minimum, pop mininum, push new val
  for (int i = k; i < size; i++)
  {
    if (in[i] > queue.top().first)
    {
      queue.pop();
      queue.push(val_index(in[i], i));
    }
  }

  // output top k values in reverse order: largest val first
  std::vector<val_index> sorted;
  while (! queue.empty())
  {
    sorted.push_back(queue.top());
    queue.pop();
  }

  for (int i = 0; i < k; i++)
  {
      int id = sorted[i].second - background_offset;

      if (tf_expected_id(id))
      {
        std::cout << "Frame:" << frame_idx << "," << f_id << " ROI[" << roi_idx << "]: rank="
                  << k-i << ", outval=" << (float)sorted[i].first / 255 << ", "
                  << labels_classes[id] << std::endl;
        rpt_id = id;
      }
  }
  return rpt_id;
}


void OverlayFPS(Mat fps_screen, const Configuration& c, float fps, double scale) {

  // write the data in the bottom right corner of the screen
  int thickness = 1;
  int baseline = 0;

  char fps_string[20];
  sprintf(fps_string, "FPS: %.2f", fps);
  Size text_size = getTextSize(fps_string, FONT_HERSHEY_DUPLEX, scale,
                              thickness, &baseline);
  baseline += thickness;
  // place the name of the class at the botton of the box
  if (fps_screen.channels() == 4) {
    cv::putText(fps_screen, fps_string, Point(c.inWidth, c.inHeight) -
      Point(text_size.width, text_size.height), FONT_HERSHEY_DUPLEX, scale,
      Scalar(255,255,255,255), thickness);
  }
  else {
    cv::putText(fps_screen, fps_string, Point(c.inWidth, c.inHeight) -
      Point(text_size.width, text_size.height), FONT_HERSHEY_DUPLEX, scale,
      0xF000, thickness);
  }
}


void DisplayHelp()
{
    std::cout <<
    "Usage: ssd_multibox\n"
    "  Will run partitioned ssd_multibox network to perform "
    "multi-objects detection\n"
    "  and classification.  First part of network "
    "(layersGroupId 1) runs on EVE,\n"
    "  second part (layersGroupId 2) runs on DSP.\n"
    "  Use -c to run a different segmentation network.  Default is jdetnet_voc.\n"
    "Optional arguments:\n"
    " -c <config>          Valid configs: jdetnet_voc, jdetnet \n"
    " -d <number>          Number of dsp cores to use\n"
    " -e <number>          Number of eve cores to use\n"
    " -i <image>           Path to the image file as input\n"
    "                      Default are 9 frames in testvecs\n"
    " -i camera<number>    Use camera as input\n"
    "                      video input port: /dev/video<number>\n"
    " -i <name>.{mp4,mov,avi}  Use video file as input\n"
    " -l <objects_list>    Path to the object classes list file\n"
    " -f <number>          Number of frames to process\n"
    " -w <number>          Output image/video width\n"
    " -p <number>          Output probability threshold in percentage\n"
    "                      Default is 25 percent or higher\n"
    " -v                   Verbose output during execution\n"
    " -h                   Help\n";
}
