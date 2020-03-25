## Usage

In the TI psdk, git clone into /usr/share/ti/tidl/examples/

Then `cd accelerated_tidl`


`/etc/init.d/weston stop`<br/>
`make accelerated_tidl;` <br/>

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
