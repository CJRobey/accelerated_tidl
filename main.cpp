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

using namespace std;
using namespace tidl;
using namespace cv;


#define NUM_VIDEO_FRAMES  100
#define DEFAULT_CONFIG    "jdetnet_voc"
#define DEFAULT_INPUT     "../test/testvecs/input/horse_768x320.y"
#define DEFAULT_INPUT_FRAMES (1)
#define DEFAULT_OBJECT_CLASSES_LIST_FILE "./jdetnet_voc_objects.json"
#define DEFAULT_OUTPUT_PROB_THRESHOLD  25

/* Enable this macro to record individual output files and */
/* resized, cropped network input files                    */
//#define DEBUG_FILES

std::unique_ptr<ObjectClasses> object_classes;
uint32_t orig_width;
uint32_t orig_height;
uint32_t num_frames_file;

bool RunConfiguration(const cmdline_opts_t& opts);
Executor* CreateExecutor(DeviceType dt, uint32_t num, const Configuration& c,
                         int layers_group_id);
bool ReadFrame(ExecutionObjectPipeline& eop, uint32_t frame_idx,
               const Configuration& c, const cmdline_opts_t& opts,
               CamDisp &cap, ifstream &ifs);
bool WriteFrameOutput(const ExecutionObjectPipeline& eop,
                      const Configuration& c, const cmdline_opts_t& opts,
                      float confidence_value);
static void DisplayHelp();


int main(int argc, char *argv[])
{
    // Catch ctrl-c to ensure a clean exit
    signal(SIGABRT, exit);
    signal(SIGTERM, exit);

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
    opts.config = DEFAULT_CONFIG;
    opts.object_classes_list_file = DEFAULT_OBJECT_CLASSES_LIST_FILE;
    opts.num_eves = num_eves > 0 ? 1 : 0;
    opts.num_dsps = num_dsps > 0 ? 1 : 0;
    opts.input_file = DEFAULT_INPUT;
    opts.output_prob_threshold = DEFAULT_OUTPUT_PROB_THRESHOLD;
    if (! ProcessArgs(argc, argv, opts))
    {
        DisplayHelp();
        exit(EXIT_SUCCESS);
    }
    assert(opts.num_dsps != 0 || opts.num_eves != 0);
    if (opts.num_frames == 0)
        opts.num_frames = (opts.is_camera_input || opts.is_video_input) ?
                          NUM_VIDEO_FRAMES :
                          ((opts.input_file == DEFAULT_INPUT) ?
                           DEFAULT_INPUT_FRAMES : 1);
    cout << "Input: " << opts.input_file << endl;

    // Get object classes list
    object_classes = std::unique_ptr<ObjectClasses>(
                             new ObjectClasses(opts.object_classes_list_file));
    if (object_classes->GetNumClasses() == 0)
    {
        cout << "No object classes defined for this config." << endl;
        return EXIT_FAILURE;
    }

    // Run network
    bool status = RunConfiguration(opts);
    if (!status)
    {
        cout << "ssd_multibox FAILED" << endl;
        return EXIT_FAILURE;
    }

    cout << "ssd_multibox PASSED" << endl;
    return EXIT_SUCCESS;
}

bool RunConfiguration(const cmdline_opts_t& opts)
{
    // Read the TI DL configuration file
    Configuration c;
    std::string config_file = "../test/testvecs/config/infer/tidl_config_"
                              + opts.config + ".txt";
    bool status = c.ReadFromFile(config_file);
    if (!status)
    {
        cerr << "Error in configuration file: " << config_file << endl;
        return false;
    }
    c.enableApiTrace = opts.verbose;
    if (opts.num_eves == 0 || opts.num_dsps == 0)
        c.runFullNet = true;

    CamDisp cam(800, 600, c.inWidth, c.inHeight);
    cam.init_capture_pipeline();

    // setup preprocessed input
    ifstream ifs;
    if (opts.is_preprocessed_input)
    {
        ifs.open(opts.input_file, ios::binary | ios::ate);
        if (! ifs.good())
        {
            cerr << "Cannot open " << opts.input_file << endl;
            return false;
        }
        num_frames_file = ((int) ifs.tellg()) /
                          (c.inWidth * c.inHeight * c.inNumChannels);
    }

    try
    {
        // Create Executors with the approriate core type, number of cores
        // and configuration specified
        // EVE will run layersGroupId 1 in the network, while
        // DSP will run layersGroupId 2 in the network
        Executor* e_eve = CreateExecutor(DeviceType::EVE, opts.num_eves, c, 1);
        Executor* e_dsp = CreateExecutor(DeviceType::DSP, opts.num_dsps, c, 2);
        vector<ExecutionObjectPipeline *> eops;

        if (e_eve != nullptr && e_dsp != nullptr)
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
            //    it can also overlap host ReadFrame() with device processing:
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
        else
        {
            // Construct ExecutionObjectPipeline that utilizes a
            // ExecutionObject to process a single frame, each ExecutionObject
            // processes the full network
            //
            // Use duplicate EOPs to do double buffering on frame input/output
            //    because each EOP has its own set of input/output buffers,
            //    so that host ReadFrame() can overlap device processing
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
        uint32_t num_eops = eops.size();

        // Allocate input/output memory for each EOP
        AllocateMemory(eops);

        chrono::time_point<chrono::steady_clock> tloop0, tloop1;
        tloop0 = chrono::steady_clock::now();

        // Process frames with available eops in a pipelined manner
        // additional num_eops iterations to flush pipeline (epilogue)
        for (uint32_t frame_idx = 0;
             frame_idx < opts.num_frames + num_eops; frame_idx++)
        {
            ExecutionObjectPipeline* eop = eops[frame_idx % num_eops];

            // Wait for previous frame on the same eop to finish processing
            if (eop->ProcessFrameWait())
                printf("eop finished\n");

            // Read a frame and start processing it with current eo
            if (ReadFrame(*eop, frame_idx, c, opts, cam, ifs))
                eop->ProcessFrameStartAsync();
        }

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

// Create an Executor with the specified type and number of EOs
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

bool ReadFrame(ExecutionObjectPipeline& eop, uint32_t frame_idx,
               const Configuration& c, const cmdline_opts_t& opts,
               CamDisp &cap, ifstream &ifs)
{
    if ((uint32_t)frame_idx >= opts.num_frames)
        return false;

    eop.SetFrameIndex(frame_idx);

    char*  frame_buffer = eop.GetInputBufferPtr();
    assert (frame_buffer != nullptr);
    int channel_size = c.inWidth * c.inHeight;
    char *bgr_frames = (char *) cap.grab_image();
    memcpy(frame_buffer,                bgr_frames, channel_size);
    memcpy(frame_buffer+1*channel_size, bgr_frames+channel_size, channel_size);
    memcpy(frame_buffer+2*channel_size, bgr_frames+(2*channel_size), channel_size);
    return true;
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
