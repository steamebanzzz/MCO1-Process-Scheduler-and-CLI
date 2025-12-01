#include <string>
#include <ctime>
#include <iostream>
#include <fstream>
#include <random>
#include <thread>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>
#include "process_console.h"
#include "memory_manager.h"

static int processCounter = 0;

process_console::process_console(const std::string& name, int instructionTotal)
    : name(name), processID(++processCounter), instructionLine(0), instructionTotal(instructionTotal),
    isActive(true), status(WAITING), coreID(-1), timestamp(getCurrentTime()) {
}

void process_console::runProcess(int coreID, int quantum_cycles, int delaysPerExec) {
    this->coreID = coreID;
    getCurrentTime();
    status = RUNNING;

    std::random_device rd;
    std::knuth_b knuth_gen(rd());
    std::uniform_int_distribution<> dist(10, 20);

    int executedInstructions = 0;

    while (isActive && instructionLine < instructionTotal) {
        if (delaysPerExec > 0) {
            for (int delay = 0; delay < delaysPerExec; ++delay) {

            }
        }

        if (quantum_cycles > 0 && executedInstructions >= quantum_cycles) {
            status = WAITING;
            break;
        }

        if (this->sleepTicks > 0) {
            this->sleepTicks--;
        }
        else if (this->instructionPointer < this->instructions.size()) {
            this->executeInstruction(this, this->instructions[this->instructionPointer]);
            this->instructionPointer++;

            if (this->instructionPointer >= this->instructions.size())
                this->finished = true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(dist(knuth_gen)));

        instructionLine++;
        executedInstructions++;
    }

    if (instructionLine == instructionTotal) {
        status = TERMINATED;
    }
}

std::string process_console::getName() const {
    return name;
}

std::string process_console::getTimestamp() const {
    return timestamp;
}

int process_console::getInstructionLine() const {
    return instructionLine;
}

bool process_console::getIsActive() const {
    return isActive;
}

int process_console::getInstructionTotal() const {
    return instructionTotal;
}

process_console::Status process_console::getStatus() const {
    return status;
}

int process_console::getCoreID() const {
    return coreID;
}

int process_console::getProcessID() const {
    return processID;
}

void process_console::setInstructionLine(int instructionLine) {
    this->instructionLine = instructionLine;
}

void process_console::setInstructionTotal(int instructionTotal) {
    this->instructionTotal = instructionTotal;
}

void process_console::setProcessID(int id) {
    processID = id;
}

void process_console::setIsActive(bool active) {
    isActive = active;
}

void process_console::setMemoryViolation(uint32_t address) {
    this->hasViolation = true;
    this->violationAddress = address;
    this->violationTime = getCurrentTime(HH_MM_SS_ONLY);
    this->isActive = false;
    this->status = TERMINATED;
}

bool process_console::getHasViolation() const {
    return hasViolation;
}

std::string process_console::getViolationTime() const {
    return violationTime;
}

uint32_t process_console::getViolationAddress() const {
    return violationAddress;
}

std::string process_console::getCurrentTime(TimeFormat format) {
    std::time_t now = std::time(0);
    std::tm localTime;
#if defined(_MSC_VER)
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    char buffer[50];
    const char* formatString;

    switch (format) {
    case HH_MM_SS_ONLY:
        formatString = "%H:%M:%S%p";
        break;
    case DEFAULT:
    default:
        formatString = "(%m/%d/%Y %I:%M:%S%p)";
        break;
    }

    std::strftime(buffer, sizeof(buffer), formatString, &localTime);
    return buffer;
}

static uint16_t clampUint16(int value) {
    if (value < 0) return 0;
    if (value > UINT16_MAX) return UINT16_MAX;
    return static_cast<uint16_t>(value);
}

void process_console::executeInstruction(process_console* proc, const Instruction& instr) {
    switch (instr.type) {
    case PRINT: {
        std::string msg;
        if (!instr.message.empty()) {
            // If message is exactly a variable name and exists, print its value
            if (proc->variables.count(instr.message)) {
                msg = instr.message + " = " + to_string(proc->variables[instr.message]);
            }
            else {
                msg = instr.message;
            }
        }
        else {
            msg = "Hello world from " + proc->getName() + "!";
        }
        proc->logs.push_back(msg);
        break;
    }
    case DECLARE: {
        if (proc->variables.size() >= 32) {
            proc->logs.push_back("DECLARE ignored: symbol table full (32 variables).");
        }
        else {
            // Clamp the value safely to 0–65535
            uint16_t v = clampUint16(static_cast<int>(instr.value1));
            proc->variables[instr.var1] = v;
        }
        break;
    }

    case ADD: {
        uint16_t val2 = proc->variables.count(instr.var2) ? proc->variables[instr.var2] : instr.value1;
        uint16_t val3 = proc->variables.count(instr.var3) ? proc->variables[instr.var3] : instr.value2;
        uint32_t result = static_cast<uint32_t>(val2) + static_cast<uint32_t>(val3);
        if (result > UINT16_MAX) result = UINT16_MAX;
        proc->variables[instr.var1] = static_cast<uint16_t>(result);
        break;
    }

    case SUBTRACT: {
        uint16_t val2 = proc->variables.count(instr.var2) ? proc->variables[instr.var2] : instr.value1;
        uint16_t val3 = proc->variables.count(instr.var3) ? proc->variables[instr.var3] : instr.value2;
        int32_t result = static_cast<int32_t>(val2) - static_cast<int32_t>(val3);
        if (result < 0) result = 0;
        proc->variables[instr.var1] = static_cast<uint16_t>(result);
        break;
    }

    case SLEEP: {
        proc->sleepTicks = static_cast<uint8_t>(instr.value1);
        break;
    }

    case FOR_LOOP: {
        for (int i = 0; i < instr.repeats; i++) {
            for (const auto& subInstr : instr.subInstructions) {
                proc->executeInstruction(proc, subInstr);
            }
        }
        break;
    }
    case READ: {
        // instr.var1 = variable to store result
        // instr.memAddress = word address (uint32_t)
        if (!proc->memoryManagerPtr) {
            proc->logs.push_back("READ failed: no memory manager attached.");
            break;
        }

        MemoryManager* mm = static_cast<MemoryManager*>(proc->memoryManagerPtr);
        uint16_t value = 0;

        // Attempt to read from memory
        if (mm->readUint16(proc, instr.memAddress, value)) {
            // On success, store value in the variable table
            proc->variables[instr.var1] = value;
        }
        // On failure, readUint16 already logs and may terminate the process
        break;
    }
    case WRITE: {
        if (!proc->memoryManagerPtr) {
            proc->logs.push_back("WRITE failed: no memory manager attached.");
            break;
        }

        MemoryManager* mm = static_cast<MemoryManager*>(proc->memoryManagerPtr);
        uint16_t valueToWrite = 0;

        // If variable exists, use its value; otherwise, use instr.value1
        if (!instr.var1.empty() && proc->variables.count(instr.var1)) {
            valueToWrite = proc->variables[instr.var1];
        }
        else {
            valueToWrite = clampUint16(static_cast<int>(instr.value1));
        }

        // Attempt to write to memory
        mm->writeUint16(proc, instr.memAddress, valueToWrite);
        break;
    }
    default:
        proc->logs.push_back("Unknown instruction encountered.");
        break;
    }
}

