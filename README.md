# MO1 - OS Emulator: Process Scheduler and CLI

Course: CSOPESY
Major Output 1  
Prepared by: Gregory Cu  

Developers: 
- Sofia Ashley M. Aguete  
- Chrisane Ianna B. Gaspar  
- Evan Andrew J. Pinca  
- Adler Clarence E. Strebel  

## Project Overview
A command-line OS emulator that simulates a CPU process scheduler. The emulator supports FCFS and Round-Robin scheduling, pre-generated process instructions (PRINT, DECLARE, ADD, SUBTRACT, SLEEP, FOR), and CPU utilization reporting.

## Attributes
- Main menu commands: `initialize`, `exit`
- Screen commands:
    - `scheduler-start`, `scheduler-stop`, `report-util`
    - `screen -s [process name]` - Create a new process
    - `screen -r [process_name]` — Check on status of current process
    - `screen -ls` - List running/ready/sleeping/finished processes + CPU summary
    - `process-smi` - status/logs, `exit` - back to main menu
- Process instruction support:
    - `PRINT(msg)` — prints text to the process (defaults to "Hello world from <process_name>!")
    - `DECLARE(var, value)` — declares a uint16 variable.
    - `ADD(var1, var2/value, var3/value)` — var1 = var2 + var3
    - `SUBTRACT(var1, var2/value, var3/value)` — var1 = var2 - var3
    - `SLEEP(X)` — sleep for X CPU ticks
    - `FOR([instructions], repeats)` — repeat a block of instructions; nesting supported up to 3

## Configuration (`config.txt`)

```
- num-cpu 16 [integer]
- scheduler "rr" [rr or fcfs]
- quantum-cycles 5 [positive integer]
- batch-process-freq 1 [>=1]
- min-ins 5000
- max-ins 5000
- delay-per-exec 1
```

## Build & Run 
1. Open the project in Visual Studio Community
2. Check your C++ environment
3. Go to the top menu → Build → Build Solution.
4. Run the emulator: Click the green "Start" button.
5. The command-line emulator will open in a terminal window.
