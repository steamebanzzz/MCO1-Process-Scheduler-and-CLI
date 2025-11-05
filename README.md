# MO1 - OS Emulator: Process Scheduler and CLI

Course: CSOPESY
Major Output 1  
Prepared by: Gregory Cu  

Developers: 
[1] Aguete, Sofia Ashley M.
[2] Gaspar, Chrisane Ianna B.
[3] Pinca, Evan Andrew J.
[4] Strebel, Adler Clarence E. 

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
