## Usage

From the tidl psdk, in the folder /usr/share/ti/tidl/examples/accelerated_tidl/

/etc/init.d/weston stop
make accelerated_tidl;

Usage: ./accelerated_tidl

Will run partitioned network to perform multi-objects or segmentation detection and classification, or just classification.  

First part of network (layersGroupId 1) runs on EVE, while the second part (layersGroupId 2) runs on DSP.

Use -c to run a different network than the default for each net_type

Optional arguments:
 `-t <net_type>        Valid net types: ssd, seg, class`
 `-c <config>          Valid configs for ssd: jdetnet_voc, jdetnet`
 `-d <number>          Number of dsp cores to use`
 `-e <number>          Number of eve cores to use`
 `-i <number>          Use /dev/video<number> as input`
 `-l <objects_list>    Path to the object classes list file`
 `-f <number>          Number of frames to process`
 `-p <number>          Output probability threshold in percentage - default is 25 percent or higher`
 `-q                   Only for segmentation demo - executes a quicker, and less computationally expensive display routine`
` -v                   Verbose output during execution`
` -h                   Help`
