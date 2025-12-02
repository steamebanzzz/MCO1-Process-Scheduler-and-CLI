# MO2 - Multitasking OS

Course: CSOPESY  
MP Specs are prepared by: Mr. Gregory Cu  

Developers: 
- Sofia Ashley M. Aguete  
- Chrisane Ianna B. Gaspar  
- Evan Andrew J. Pinca  
- Adler Clarence E. Strebel  

## Project Overview
This project is an emulator of a multitasking operating system that ff:
- Process creation using "screen -s" and "screen -c"
- Instruction execution including DECLARE, ADD, PRINT, READ, and WRITE
- Demand paging memory manager
- Backing store operations using "csopesy-backing-store.txt"
- Memory visualization using the commands "process-smi" and "vmstat"
- CPU scheduling (FCFS or RR)
- All MO1 features carried over and expanded with memory management

## Entry Point

For MO2, be sure to pull or download the ```mco2_dev-branch``` branch for this machine project.

The main function of the program is located in:

Main.cpp

## Configuration (`config.txt`)

```
num-cpu 1
scheduler rr
quantum-cycles 10
batch-process-freq 1
min-ins 1000
max-ins 1000
delay-per-exec 50
max-overall-mem 4096
mem-per-frame 256
```

## Build & Run 
1. Open the project in Visual Studio Community
2. Check your C++ environment
3. Go to the top menu → Build → Build Solution.
4. Run the emulator: Click the green "Start" button.
5. The command-line emulator will open in a terminal window.

## Commands List
1. initialize  
   - Loads all system parameters from config.txt and prepares the scheduler, memory manager, and backing store.

2. screen -s <process_name> <memory_size>  
   - Creates a new process with the given name and memory allocation.

3. screen -c <process_name> <memory_size> "<instructions>"  
   - Creates a new process and immediately assigns it a custom set of instructions (1–50 instructions).

4. screen -r <process_name>  
   - Re-attaches the user to the process screen, showing its output or reporting if the process ended or crashed because of memory access violation.

5. scheduler-start  
   - Begins instruction execution for all ready processes according to the chosen CPU scheduling algorithm.

6. scheduler-stop  
   - Stops the scheduler and halts further CPU instruction execution.

7. scheduler-test  
   - Automatically generates processes at intervals defined by batch-process-freq and runs them under the scheduler for testing.

8. process-smi  
   - Displays a summarized view of memory usage, showing total memory, used memory, free memory, and all running processes with their memory consumption.

9. vmstat  
   - Shows a detailed breakdown of system activity including CPU ticks, memory allocation, active/inactive pages, and page-in/page-out counters.

10. exit  
   - Terminates the CLI emulator safely and shuts down all remaining processes.
