#include <string>
#include <ctime>
#include <iostream>
#include <fstream>
#include <random>
#include <thread>
#include "process_console.h"

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

std::string process_console::getCurrentTime() {
    std::time_t now = std::time(0);
    std::tm localTime;
#if defined(_MSC_VER)
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    char buffer[50];
    std::strftime(buffer, sizeof(buffer), "(%m/%d/%Y %I:%M:%S%p)", &localTime);
    return buffer;
}

void process_console::executeInstruction(process_console* proc, const Instruction& instr) {
    switch (instr.type) {
    case PRINT: {
        // DEFAULT MESSAGE
        std::string msg = instr.message.empty()
            ? "Hello world from " + proc->getName() + "!"
            : instr.message;
        proc->logs.push_back(msg);
        std::cout << msg << std::endl;
        break;
    }

    case DECLARE: {
        proc->variables[instr.var1] = instr.value1;
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

    default:
        proc->logs.push_back("Unknown instruction encountered.");
        break;
    }
}


