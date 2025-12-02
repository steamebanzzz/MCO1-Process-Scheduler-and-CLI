#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <iomanip>

#include "memory_manager.h"
#include "process_console.h"

// Internal backing store map keyed by (pid<<32 | vpage)
static std::unordered_map<uint64_t, std::vector<uint8_t>> backingStoreMap;

// Per-frame byte storage and metadata
static std::vector<std::vector<uint8_t>> frameData;     // frameIndex -> bytes (memPerFrame)
static std::vector<int> frameOwnerPID;                  // frameIndex -> pid (-1 = free)
static std::vector<int> frameOwnerVPage;                // frameIndex -> vpage (-1 = none)

// Paging stats
static uint64_t pagedInCount = 0;
static uint64_t pagedOutCount = 0;

// Round-robin victim pointer
static int nextVictim = 0;

// Helper: convert uint32_t to hex string
static std::string to_hex(uint32_t value) {
    std::stringstream ss;
    ss << std::hex << value;
    return ss.str();
}

// =======================
// Helper functions
// =======================

// find a free physical frame; returns -1 if none
static int findFreeFrameInternal() {
    for (size_t i = 0; i < frameOwnerPID.size(); ++i)
        if (frameOwnerPID[i] == -1) return static_cast<int>(i);
    return -1;
}

// choose a victim frame to evict (round-robin)
static int chooseVictimFrameInternal() {
    int fc = static_cast<int>(frameOwnerPID.size());
    for (int scanned = 0; scanned < fc; ++scanned) {
        int idx = (nextVictim + scanned) % fc;
        if (frameOwnerPID[idx] != -1) {
            nextVictim = (idx + 1) % fc;
            return idx;
        }
    }
    nextVictim = 0;
    return 0;
}

// =======================
// MemoryManager
// =======================
MemoryManager::MemoryManager(int maxMem, int memPerFrame)
    : totalMemory(maxMem), memPerFrame(memPerFrame) {

    if (memPerFrame <= 0) memPerFrame = 1;
    frameCount = std::max(1, totalMemory / memPerFrame);

    // Initialize frame vectors
    frames.assign(frameCount, nullptr);
    frameData.assign(frameCount, std::vector<uint8_t>(memPerFrame, 0));
    frameOwnerPID.assign(frameCount, -1);
    frameOwnerVPage.assign(frameCount, -1);

    // Clear backing store file
    std::ofstream ofs(backingStoreFile, std::ios::trunc);
    if (!ofs) std::cerr << "Error creating backing store file.\n";

    // Reset stats
    pagedInCount = 0;
    pagedOutCount = 0;
    nextVictim = 0;
}

// Print frame allocation table
void MemoryManager::printFrameTable() {
    std::cout << "\n[Frame Allocation Table]\n";
    for (int i = 0; i < frameCount; ++i) {
        if (frameOwnerPID[i] == -1)
            std::cout << "Frame " << i << ": Free\n";
        else
            std::cout << "Frame " << i << ": PID " << frameOwnerPID[i]
            << " VPage " << frameOwnerVPage[i] << "\n";
    }
    std::cout << std::dec << std::endl;
}

// Allocate memory for a process
bool MemoryManager::allocateMemory(process_console* proc, int memoryRequired) {
    if (!proc || memoryRequired <= 0) return false;

    int requiredPages = (memoryRequired + memPerFrame - 1) / memPerFrame;
    std::vector<int> pageTable(requiredPages, -1); // -1 -> not resident
    processFrames[proc->getProcessID()] = std::move(pageTable);

    // Initialize backing store pages (zero-filled)
    for (int p = 0; p < requiredPages; ++p) {
        uint64_t key = (static_cast<uint64_t>(proc->getProcessID()) << 32) | static_cast<uint32_t>(p);
        if (backingStoreMap.find(key) == backingStoreMap.end()) {
            backingStoreMap[key] = std::vector<uint8_t>(memPerFrame, 0);
        }
    }
   
    return true;
}

// Deallocate memory for a process
void MemoryManager::deallocateMemory(process_console* proc) {
    if (!proc) return;
    int pid = proc->getProcessID();
    auto it = processFrames.find(pid);
    if (it == processFrames.end()) return;

    auto& pt = it->second;
    for (int v = 0; v < (int)pt.size(); ++v) {
        int frameIdx = pt[v];
        if (frameIdx >= 0 && frameIdx < frameCount) {
            uint64_t key = (static_cast<uint64_t>(pid) << 32) | static_cast<uint32_t>(v);
            backingStoreMap[key] = frameData[frameIdx];

            // Write to backing store file
            std::ofstream ofs(backingStoreFile, std::ios::app);
            if (ofs) {
                ofs << "PID " << pid << " PAGE " << v << " : ";
                for (uint8_t b : frameData[frameIdx]) {
                    ofs << std::hex << std::setw(2) << std::setfill('0') << (int)b;
                }
                ofs << "\n";
            }

            frameOwnerPID[frameIdx] = -1;
            frameOwnerVPage[frameIdx] = -1;
            frames[frameIdx] = nullptr;
            std::fill(frameData[frameIdx].begin(), frameData[frameIdx].end(), 0);
        }
    }

    processFrames.erase(it);
}

// Handle a page fault
int MemoryManager::handlePageFault(process_console* proc, int vpage) {
    if (!proc) return -1;
    int pid = proc->getProcessID();

    auto ptabIt = processFrames.find(pid);
    if (ptabIt == processFrames.end()) return -1;
    auto& pt = ptabIt->second;
    if (vpage < 0 || vpage >= (int)pt.size()) return -1;

    if (pt[vpage] != -1) return pt[vpage];

    int freeFrame = findFreeFrameInternal();
    if (freeFrame == -1) {
        int victim = chooseVictimFrameInternal();
        int victimPid = frameOwnerPID[victim];
        int victimVPage = frameOwnerVPage[victim];

        if (victimPid != -1 && victimVPage != -1) {
            // Save victim page to backing store
            uint64_t vkey = (static_cast<uint64_t>(victimPid) << 32) | static_cast<uint32_t>(victimVPage);
            backingStoreMap[vkey] = frameData[victim];
            pagedOutCount++;

            auto vit = processFrames.find(victimPid);
            if (vit != processFrames.end() && victimVPage < (int)vit->second.size()) {
                vit->second[victimVPage] = -1;
            }
        }
        freeFrame = victim;
    }

    // Load page from backing store
    uint64_t key = (static_cast<uint64_t>(pid) << 32) | static_cast<uint32_t>(vpage);
    auto bsIt = backingStoreMap.find(key);
    if (bsIt != backingStoreMap.end()) frameData[freeFrame] = bsIt->second;
    else frameData[freeFrame].assign(memPerFrame, 0);

    frameOwnerPID[freeFrame] = pid;
    frameOwnerVPage[freeFrame] = vpage;
    frames[freeFrame] = proc;
    pt[vpage] = freeFrame;

    pagedInCount++;
    return freeFrame;
}

// Save frame to backing store
void MemoryManager::saveFrameToBackingStore(int frameIndex) {
    if (frameIndex < 0 || frameIndex >= frameCount) return;
    int pid = frameOwnerPID[frameIndex];
    int vpage = frameOwnerVPage[frameIndex];
    if (pid == -1 || vpage == -1) return;

    uint64_t key = (static_cast<uint64_t>(pid) << 32) | static_cast<uint32_t>(vpage);
    backingStoreMap[key] = frameData[frameIndex];

    std::ofstream ofs(backingStoreFile, std::ios::app);
    if (!ofs) return;
    ofs << "PID " << pid << " PAGE " << vpage << " : ";
    for (uint8_t b : frameData[frameIndex]) {
        ofs << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    }
    ofs << "\n";
}

// Load frame from backing store
void MemoryManager::loadFrameFromBackingStore(int frameIndex) {
    if (frameIndex < 0 || frameIndex >= frameCount) return;
    int pid = frameOwnerPID[frameIndex];
    int vpage = frameOwnerVPage[frameIndex];
    if (pid == -1 || vpage == -1) return;

    uint64_t key = (static_cast<uint64_t>(pid) << 32) | static_cast<uint32_t>(vpage);
    auto it = backingStoreMap.find(key);
    if (it != backingStoreMap.end()) frameData[frameIndex] = it->second;
    else frameData[frameIndex].assign(memPerFrame, 0);
}

// =======================
// Read/write integration
// =======================
bool MemoryManager::readUint16(process_console* proc, uint32_t address, uint16_t& value) {
    if (!proc) return false;
    int pageIndex = address / memPerFrame;
    int offset = address % memPerFrame;

    auto it = processFrames.find(proc->getProcessID());
    if (it == processFrames.end() || pageIndex >= (int)it->second.size()) {
        proc->setMemoryViolation(address);
        return false;
    }

    int frameIndex = it->second[pageIndex];
    if (frameIndex == -1) frameIndex = handlePageFault(proc, pageIndex);
    if (frameIndex == -1) {
        proc->setMemoryViolation(address);
        return false;
    }

    value = frameData[frameIndex][offset];
    return true;
}

bool MemoryManager::writeUint16(process_console* proc, uint32_t address, uint16_t value) {
    if (!proc) return false;
    int pageIndex = address / memPerFrame;
    int offset = address % memPerFrame;

    auto it = processFrames.find(proc->getProcessID());
    if (it == processFrames.end() || pageIndex >= (int)it->second.size()) {
        proc->setMemoryViolation(address);
        return false;
    }

    int frameIndex = it->second[pageIndex];
    if (frameIndex == -1) frameIndex = handlePageFault(proc, pageIndex);
    if (frameIndex == -1) {
        proc->setMemoryViolation(address);
        return false;
    }

    frameData[frameIndex][offset] = static_cast<uint8_t>(value & 0xFF);
    return true;
}

// =======================
// Utilities
// =======================
int MemoryManager::getTotalMemory() const { return totalMemory; }
int MemoryManager::getMemPerFrame() const { return memPerFrame; }
int MemoryManager::getFrameCount() const { return frameCount; }

int MemoryManager::getUsedFrameCount() const {
    int used = 0;
    for (size_t i = 0; i < frameOwnerPID.size(); ++i)
        if (frameOwnerPID[i] != -1) ++used;
    return used;
}

int MemoryManager::getUsedMemory() const {
    return getUsedFrameCount() * memPerFrame;
}
uint64_t MemoryManager::getPagedInCount() const { return pagedInCount; }
uint64_t MemoryManager::getPagedOutCount() const { return pagedOutCount; }
