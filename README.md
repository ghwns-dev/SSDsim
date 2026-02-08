#2025/08/19 
#Author : 

The SSDsim (v1.9) is a simple simulator for modeling the general SSD architecture, with relatively small size.
(8KB of total NAND Flash, max logical address is 1024, physical page addrses is 8192)
Despite of the small size of the model, the basic operations of SSD controller such as READ, ERASE, PROGRAM and Address Mapping, Garbage Collection
are managed in the simulator. The SSDsim shows how many cycles are needed for total host reqeust.
So one can realize his/her own idea for boosting the performance in terms of the lateny.

Garbage Collecton has been managed since SSDsim v1.7, and timing parameter calculation has been modified.
SSDsim v1.7 revision uses the free block pointer for reducing the latency for finding the free block, and number of free pagse and invalid pages
stored in the struct "block_t". This policy makes the SSDsim runs within the 5% cycles compared to the SSDsim v1.6 with the same running config. 
(max buffer size 32, iteration count 8192 )
Now the address has been scaled, I recommend that you run the simulator with max buffer size 256, iteration count 131072.
You can simply run the process by executing the shell script file ./run.sh
The most critical issue of this revison 1.81 is there are some cases that at the end of the execution, number of the valid flash pages exceeds the max logical address.
It should be handled before the revision 1.9.
Further sophisticated Address Mapping algorithm, addition of other components reducing the latency would be done within some next Release.
  
The main function generates the random host-side command for reqeusting the data processing, READ, WRITE.
FTL (microprocessor) tranlates them into a proper form and stores them in the Queue located in DRAM.
Flash Memory is initialized with filled the data thus the host READ command would hardly fail.

The software tree is as below.

SSDsim_v1.9/
├── Makefile
├── README.md
├── src/
│   ├── run.cc
│   ├── microprocessor.h
│   ├── microprocessor.cc
│   ├── dramcontroller.h
│   ├── dramcontroller.cc
│   ├── flashcontroller.h
│   ├── flashcontroller.cc
│
├── src/include/
│   ├── defs.h
│   ├── defs.cc
│   ├── random.h
│   ├── random.cc
│
└── build/SSDsim

Further release shall obey this structure, and it is expected that there would be no dramatic change for the structrue.

Build and execution procedure is as below.

$make clean
$make
$./build/SSDsim -max_buffer_size=256 -iteration_cnt=131072 (recommended execution command)

(or you can simply execute "run.sh")

Execution with buffer size 256 and iteration count 131072 will fully utilize the 64KB Flash Memory (heuristic)
And the case would show how Garbage Collection operates.

Executing the SSDsim with config file is not yet supported, but soon it would be accessible.
(e.g. ./build/SSDsim ./config/SSD_model_name.ini)


Any modification is up to you and welcomed, enjoy your work.
