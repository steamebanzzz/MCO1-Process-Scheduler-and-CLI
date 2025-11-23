#pragma once
#include <vector>
#include <map>
#include <fstream>
#include <string>
#include <iostream>
#include <algorithm>

class process_console;  // forward declaration

class MemoryManager {
private:
    int totalMemory;
    int memPerFrame;
    int frameCount;
    std::vector<process_console*> frames;                 // true = occupied, false = free
    std::map<int, std::vector<int>> processFrames; // PID -> list of allocated frame indices
    std::string backingStoreFile = "csopesy-backing-store.txt";

public:
    MemoryManager(int maxMem, int memPerFrame);
    bool allocateMemory(process_console* proc, int memoryRequired); // returns success/fail
    void deallocateMemory(process_console* proc);
    int handlePageFault(process_console* proc, int frameIndex);
    void saveFrameToBackingStore(int frameIndex);
    void loadFrameFromBackingStore(int frameIndex);
    void printFrameTable();
    bool writeUint16(class process_console* proc, uint32_t wordAddress, uint16_t value);
    bool readUint16(class process_console* proc, uint32_t wordAddress, uint16_t& outValue);
};
