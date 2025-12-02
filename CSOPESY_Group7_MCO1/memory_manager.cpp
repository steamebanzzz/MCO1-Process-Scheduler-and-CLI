#include "memory_manager.h"
#include "process_console.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <sstream>

std::unordered_map<uint64_t, std::vector<uint8_t>> MemoryManager::backingStoreMap;

MemoryManager::MemoryManager(int totalMemoryBytes, int memPerFrameBytes)
    : totalMemory(totalMemoryBytes), memPerFrame(memPerFrameBytes),
    pagedInCount(0), pagedOutCount(0), nextVictim(0)
{
    if (memPerFrame <= 0) memPerFrame = 1;

    frameCount = std::max(1, totalMemory / memPerFrame);

    frames.assign(frameCount, nullptr);
    frameData.assign(frameCount, std::vector<uint8_t>(memPerFrame, 0));
    frameOwnerPID.assign(frameCount, -1);
    frameOwnerVPage.assign(frameCount, -1);

    // Clear backing store
    std::ofstream ofs(backingStoreFile, std::ios::trunc);
    if (!ofs) std::cerr << "Error creating backing store file.\n";
}

// ---------------------------
// Allocation / Deallocation
// ---------------------------
bool MemoryManager::allocateMemory(process_console* proc, int memoryRequiredBytes) {
    if (!proc) return false;

    int framesNeeded = (memoryRequiredBytes + memPerFrame - 1) / memPerFrame;
    framesNeeded = std::max(1, framesNeeded);

    int freeFrames = 0;
    for (int i = 0; i < frameCount; ++i)
        if (frameOwnerPID[i] == -1) freeFrames++;

    // if (freeFrames < framesNeeded) {
    //    std::cout << "Memory allocation failed: only " << freeFrames
    //        << " free frame(s) for process " << proc->getProcessID() << "\n";
    //    return false;
    // }

    std::vector<int> allocatedFrames;
    for (int v = 0; v < framesNeeded; ++v) {
        int frameIdx = findFreeFrame();
        if (frameIdx == -1) {
            // No free frame → evict one
            frameIdx = chooseVictimFrame();
            int victimPid = frameOwnerPID[frameIdx];
            int victimVPage = frameOwnerVPage[frameIdx];

            if (victimPid != -1 && victimVPage != -1) {
                // Save victim to backing store
                uint64_t key = (static_cast<uint64_t>(victimPid) << 32) | static_cast<uint32_t>(victimVPage);
                backingStoreMap[key] = frameData[frameIdx];
                pagedOutCount++;

                auto vit = processFrames.find(victimPid);
                if (vit != processFrames.end() && victimVPage < (int)vit->second.size()) {
                    vit->second[victimVPage] = -1;
                }
            }
        }

        frameOwnerPID[frameIdx] = proc->getProcessID();
        frameOwnerVPage[frameIdx] = v;
        frames[frameIdx] = proc;
        std::fill(frameData[frameIdx].begin(), frameData[frameIdx].end(), 0);
        allocatedFrames.push_back(frameIdx);
        pagedInCount++;
    }


    proc->allocatedFrames = allocatedFrames;
    processFrames[proc->getProcessID()] = allocatedFrames;

    //std::cout << "Process " << proc->getProcessID() << " allocated frames: ";
    //for (int f : allocatedFrames) std::cout << f << " ";
    //std::cout << "\n";

    return true;
}

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

            frameOwnerPID[frameIdx] = -1;
            frameOwnerVPage[frameIdx] = -1;
            frames[frameIdx] = nullptr;
            std::fill(frameData[frameIdx].begin(), frameData[frameIdx].end(), 0);
        }
    }
    processFrames.erase(it);
}

// ---------------------------
// Page Fault / Eviction
// ---------------------------
int MemoryManager::handlePageFault(process_console* proc, int vpage) {
    if (!proc) return -1;
    int pid = proc->getProcessID();

    auto ptabIt = processFrames.find(pid);
    if (ptabIt == processFrames.end()) return -1;
    auto& pt = ptabIt->second;

    if (vpage < 0 || vpage >= (int)pt.size()) return -1;

    if (pt[vpage] != -1) return pt[vpage];

    int freeFrame = findFreeFrame();
    if (freeFrame == -1) {
        int victim = chooseVictimFrame();
        int victimPid = frameOwnerPID[victim];
        int victimVPage = frameOwnerVPage[victim];

        if (victimPid != -1 && victimVPage != -1) {
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

// ---------------------------
// Read / Write
// ---------------------------
bool MemoryManager::readUint16(process_console* proc, uint32_t address, uint16_t& value) {
    int pageIndex = address / memPerFrame;
    int offset = address % memPerFrame;

    auto it = processFrames.find(proc->getProcessID());
    if (it == processFrames.end() || pageIndex < 0 || pageIndex >= (int)it->second.size()) {
        proc->setMemoryViolation(address);
        return false;
    }

    int frameIndex = it->second[pageIndex];
    if (frameIndex == -1) frameIndex = handlePageFault(proc, pageIndex);
    if (frameIndex < 0 || frameIndex >= frameCount || offset < 0 || offset >= memPerFrame) {
        proc->setMemoryViolation(address);
        return false;
    }

    value = frameData[frameIndex][offset];
    return true;
}

bool MemoryManager::writeUint16(process_console* proc, uint32_t address, uint16_t value) {
    int pageIndex = address / memPerFrame;
    int offset = address % memPerFrame;

    auto it = processFrames.find(proc->getProcessID());
    if (it == processFrames.end() || pageIndex < 0 || pageIndex >= (int)it->second.size()) {
        proc->setMemoryViolation(address);
        return false;
    }

    int frameIndex = it->second[pageIndex];
    if (frameIndex == -1) frameIndex = handlePageFault(proc, pageIndex);
    if (frameIndex < 0 || frameIndex >= frameCount || offset < 0 || offset >= memPerFrame) {
        proc->setMemoryViolation(address);
        return false;
    }

    frameData[frameIndex][offset] = static_cast<uint8_t>(value & 0xFF);
    return true;
}

// ---------------------------
// Backing store
// ---------------------------
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

// ---------------------------
// Utilities
// ---------------------------
int MemoryManager::getTotalMemory() const { return totalMemory; }
int MemoryManager::getMemPerFrame() const { return memPerFrame; }
int MemoryManager::getFrameCount() const { return frameCount; }

int MemoryManager::getUsedFrameCount() const {
    int used = 0;
    for (int pid : frameOwnerPID) if (pid != -1) used++;
    return used;
}

int MemoryManager::getUsedMemory() const { return getUsedFrameCount() * memPerFrame; }
uint64_t MemoryManager::getPagedInCount() const { return pagedInCount; }
uint64_t MemoryManager::getPagedOutCount() const { return pagedOutCount; }

std::vector<int> MemoryManager::getFramesForProcess(int pid) const {
    std::vector<int> allocated;
    for (size_t i = 0; i < frameOwnerPID.size(); ++i)
        if (frameOwnerPID[i] == pid) allocated.push_back(static_cast<int>(i));
    return allocated;
}

void MemoryManager::printFrameTable() const {
    std::cout << "\n[Frame Allocation Table]\n";
    for (int i = 0; i < frameCount; ++i) {
        if (frameOwnerPID[i] == -1) std::cout << "Frame " << i << ": Free\n";
        else std::cout << "Frame " << i << ": PID " << frameOwnerPID[i]
            << " VPage " << frameOwnerVPage[i] << "\n";
    }
}

void MemoryManager::debugPrintFrames() const {
    std::cout << "\n[Memory Frame Contents]\n";
    for (int i = 0; i < frameCount; ++i) {
        std::cout << "Frame " << i << " (PID " << frameOwnerPID[i]
            << ", VPage " << frameOwnerVPage[i] << "): ";
        for (uint8_t b : frameData[i])
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)b << " ";
        std::cout << std::dec << "\n";
    }
}

// ---------------------------
// Internal helpers
// ---------------------------
int MemoryManager::findFreeFrame() {
    for (size_t i = 0; i < frameOwnerPID.size(); ++i)
        if (frameOwnerPID[i] == -1) return static_cast<int>(i);
    return -1;
}

int MemoryManager::chooseVictimFrame() {
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

std::string MemoryManager::to_hex(uint32_t value) {
    std::stringstream ss;
    ss << std::hex << value;
    return ss.str();
}
