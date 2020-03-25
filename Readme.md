## Usage

In the TI psdk, git clone into /usr/share/ti/tidl/examples/

Then `cd accelerated_tidl`

Build the binary: <br/>
`make accelerated_tidl;` <br/>

Stop the GUI, so that the native display can be used by the application: <br/>
`/etc/init.d/weston stop`<br/>

Usage: ./accelerated_tidl

Will run partitioned network to perform multi-objects or segmentation detection and classification, or just classification.  

First part of network (layersGroupId 1) runs on EVE, while the second part (layersGroupId 2) runs on DSP.

Use -c to run a different network than the default for each net_type

Optional arguments: <br/>

 `-t <net_type>        Valid net types: ssd, seg, class`<br/>
 `-c <config>          Valid configs for ssd: jdetnet_voc, jdetnet`<br/>
 `-d <number>          Number of dsp cores to use`<br/>
 `-e <number>          Number of eve cores to use`<br/>
 `-i <number>          Use /dev/video<number> as input`<br/>
 `-l <objects_list>    Path to the object classes list file`<br/>
 `-f <number>          Number of frames to process`<br/>
 `-p <number>          Output probability threshold in percentage - default is 25 percent or higher`<br/>
 `-q                   Only for segmentation demo - executes a quicker, and less computationally expensive display routine`<br/>
` -v                   Verbose output during execution`<br/>
` -h                   Help`<br/>


### Examples

Segmentation Network on the Beaglebone AI or AM5729 IDK: <br/>
`./accelerated_tidl -e 4 -d 0 -g 1 -i 1 -v -f 500 -c jseg21 -l configs/jseg21_objects.json -t seg` <br/>

SSD Network on the Beaglebone AI or AM5729 IDK: <br/>
`./accelerated_tidl -e 4 -d 1 -g 2 -i 1 -v -f 500 -c jdetnet -l configs/jdetnet_objects.json -p 15 -t ssd` <br/>

Classification Network on the Beaglebone AI or AM5729 IDK: <br/>
`./accelerated_tidl -e 4 -d 1 -g 2 -i 1 -v -f 500 -c stream_config_toysdogs.txt -l configs/toydogsnet.txt -t class` <br/>

### Resetting CMEM

If you hit the error: 
`accelerated_tidl: inc/executor.h:172: T* tidl::malloc_ddr(size_t) [with T = char; size_t = unsigned int]: Assertion 'val != nullptr' failed.` <br/>

Then you need to execute the following commands: <br/>
`pkill ti-mctd` <br/>
`rm /dev/shm/HeapManager 2> /dev/null` (it may not exist) <br/>
`ti-mctd` <br/>

Sometimes crash-endings of TIDL will cause CMEM to become corrupted. The previous steps simply reset it.
