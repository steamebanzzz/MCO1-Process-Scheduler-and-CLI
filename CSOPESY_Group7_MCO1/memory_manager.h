#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>

class process_console;

class MemoryManager {
public:
    MemoryManager(int totalMemoryBytes, int memPerFrameBytes);

    // Memory allocation
    bool allocateMemory(process_console* proc, int memoryRequiredBytes);
    void deallocateMemory(process_console* proc);

    // Page fault handling
    int handlePageFault(process_console* proc, int vpage);

    // Read/write memory
    bool readUint16(process_console* proc, uint32_t address, uint16_t& value);
    bool writeUint16(process_console* proc, uint32_t address, uint16_t value);

    // Backing store
    void saveFrameToBackingStore(int frameIndex);
    void loadFrameFromBackingStore(int frameIndex);

    // Utilities
    int getTotalMemory() const;
    int getMemPerFrame() const;
    int getFrameCount() const;
    int getUsedFrameCount() const;
    int getUsedMemory() const;
    uint64_t getPagedInCount() const;
    uint64_t getPagedOutCount() const;

    std::vector<int> getFramesForProcess(int pid) const;
    void printFrameTable() const;
    void debugPrintFrames() const;

private:
    int totalMemory;
    int memPerFrame;
    int frameCount;

    std::vector<process_console*> frames;
    std::vector<std::vector<uint8_t>> frameData;
    std::vector<int> frameOwnerPID;
    std::vector<int> frameOwnerVPage;

    uint64_t pagedInCount;
    uint64_t pagedOutCount;
    int nextVictim;

    std::unordered_map<int, std::vector<int>> processFrames;
    static std::unordered_map<uint64_t, std::vector<uint8_t>> backingStoreMap;
    std::string backingStoreFile = "backing_store.txt";

    // Internal helpers
    int findFreeFrame();
    int chooseVictimFrame();
    static std::string to_hex(uint32_t value);
};
