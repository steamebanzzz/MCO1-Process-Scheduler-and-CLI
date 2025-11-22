#pragma once
#include <string>
#include <ctime>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <thread>
#include <random>
#include <cstdint>
#include "memory_manager.h"

using namespace std;

enum InstructionType {
    PRINT,
    DECLARE,
    ADD,
    SUBTRACT,
    SLEEP,
    FOR_LOOP,
    READ,
    WRITE
};

struct Instruction {
    InstructionType type;
    std::string var1, var2, var3;
    uint16_t value1 = 0, value2 = 0, repeats = 0;
    std::string message;
    std::vector<Instruction> subInstructions;
    uint32_t memAddress = 0; 
};

class process_console {
private:
    int processID;
    string name;
    string timestamp;
    int instructionLine;
    int instructionTotal;
    int coreID;
    bool isActive;

public:
    enum Status { RUNNING, WAITING, TERMINATED };
    Status status;

    vector<int> allocatedFrames;

    MemoryManager* memoryManagerPtr = nullptr;

    // Constructor
    process_console(const string& name, int instructionTotal);

    // Core simulation
    void runProcess(int coreID, int quantum_cycles, int delaysPerExec);

    // Getters
    string getName() const;
    string getTimestamp() const;
    int getInstructionLine() const;
    int getInstructionTotal() const;
    Status getStatus() const;
    int getCoreID() const;
    int getProcessID() const;
    bool getIsActive() const;

    // Setters
    void setInstructionLine(int instructionLine);
    void setInstructionTotal(int instructionTotal);
    void setProcessID(int id);
    void setIsActive(bool active);

    // Process Memory and Instructions
    std::unordered_map<std::string, uint16_t> variables;
    std::vector<Instruction> instructions;
    size_t instructionPointer = 0;
    uint8_t sleepTicks = 0;
    std::vector<std::string> logs;
    bool finished = false;

    void executeInstruction(process_console* proc, const Instruction& instr);
    void generateRandomInstructions(process_console* proc, int instructionCount);

private:
    static string getCurrentTime();
};
