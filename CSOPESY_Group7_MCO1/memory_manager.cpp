#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <cstring>
#include "memory_manager.h"
#include "process_console.h"

std::unordered_map<uint32_t, uint16_t> memoryWords; 
uint32_t totalWords = 0;

static std::unordered_map<uint64_t, std::vector<uint8_t>> backingStoreMap; // frameIndex -> data

// Per frame byte storage and metadata
static std::<std::vector<uint8_t>> frameData; // frameIndex -> data
static std::vector<int> frameOwnerPID;        // frameIndex -> owning process ID
static std::vector<int> frameOwnerVPage;      // frameIndex -> owning process virtual page

// Paging statistics
static uint64_t pagedInCount = 0;
static uint64_t pagedOutCount = 0;

// Simple round-robin victim pointer
static int nextVictim = 0;

MemoryManager::MemoryManager(int maxMem, int memPerFrame)
    : totalMemory(maxMem), memPerFrame(memPerFrame) {
    if (memPerFrame <= 0) memPerFrame = 1;
    frameCount = static_cast<int>(totalMemory / memPerFrame);
    if (frameCount <= 0) frameCount = 1;

    frames.resize(frameCount, nullptr);  // all frames free initially

    frameData.assign(frameCount, std::vector<uint8_t>(memPerFrame, 0));
    frameOwnerPID.assign(frameCount, -1);
    frameOwnerVPage.assign(frameCount, -1);

    totalWords = static_cast<uint32_t>(totalMemory / 2);

    // Clear backing store file
    std::ofstream ofs(backingStoreFile, std::ios::trunc);
    if (!ofs) std::cerr << "Error creating backing store file.\n";
}

// TESTING PURPOSES
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

// Reserve virtual memory pages for proc (demand paging, do not allocate physical frames)
bool MemoryManager::allocateMemory(process_console* proc, int memoryRequired) {
    if (!proc) return false;
    if (memoryRequired <= 0) return false;

    int requiredPages = (memoryRequired + memPerFrame - 1) / memPerFrame;
    std::vector<int> pageTable(requiredPages, -1); // -1 -> not resident
    processFrames[proc->getProcessID()] = std::move(pageTable);

    // initialize backing store pages (zero-filled)
    for (int p = 0; p < requiredPages; ++p) {
        uint64_t key = (static_cast<uint64_t>(proc->getProcessID()) << 32) | static_cast<uint32_t>(p);
        if (backingStoreMap.find(key) == backingStoreMap.end()) {
            backingStoreMap[key] = std::vector<uint8_t>(memPerFrame, 0);
        }
    }
    return true;
}

void MemoryManager::deallocateMemory(process_console* proc) {
    if (!proc) return;
    int pid = proc->getProcessID();
    auto it = processFrames.find(pid);
    if (it == processFrames.end()) return;

    // For each virtual page, if resident free the frame and persist it to backing store
    auto &pt = it->second;
    for (int v = 0; v < (int)pt.size(); ++v) {
        int frameIdx = pt[v];
        if (frameIdx >= 0 && frameIdx < frameCount) {
            // persist page to backing store
            uint64_t key = (static_cast<uint64_t>(pid) << 32) | static_cast<uint32_t>(v);
            backingStoreMap[key] = frameData[frameIdx];

            std::ofstream ofs(backingStoreFile, std::ios::app);
            if (ofs) {
                ofs << "PID " << pid << " PAGE " << v << " : ";
                for (uint8_t b : frameData[frameIdx]) {
                    ofs << std::hex << std::setw(2) << std::setfill('0') << (int)b;
                }
                ofs << "\n";
            }

            // free frame
            frameOwnerPID[frameIdx] = -1;
            frameOwnerVPage[frameIdx] = -1;
            frames[frameIdx] = nullptr;
            std::fill(frameData[frameIdx].begin(), frameData[frameIdx].end(), 0);
        }
    }

    processFrames.erase(it);
}

// find a free physical frame; returns -1 if none
static int findFreeFrameInternal() {
    for (int i = 0; i < (int)frameOwnerPID.size(); ++i) {
        if (frameOwnerPID[i] == -1) return i;
    }
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
    // fallback
    nextVictim = 0;
    return 0;
}

// handle a page fault: virtual page vpage of process proc -> bring into a physical frame.
int MemoryManager::handlePageFault(process_console* proc, int vpage) {
    if (!proc) return -1;
    int pid = proc->getProcessID();

    auto ptabIt = processFrames.find(pid);
    if (ptabIt == processFrames.end()) return -1;
    auto &pt = ptabIt->second;
    if (vpage < 0 || vpage >= (int)pt.size()) return -1;

    // If already resident, return it.
    if (pt[vpage] != -1) return pt[vpage];

    int freeFrame = findFreeFrameInternal();
    if (freeFrame == -1) {
        // no free frame: evict a victim
        int victim = chooseVictimFrameInternal();
        int victimPid = frameOwnerPID[victim];
        int victimVPage = frameOwnerVPage[victim];

        // persist victim page to backing store
        if (victimPid != -1 && victimVPage != -1) {
            uint64_t vkey = (static_cast<uint64_t>(victimPid) << 32) | static_cast<uint32_t>(victimVPage);
            backingStoreMap[vkey] = frameData[victim];
            pagedOutCount++;

            // clear victim mapping in its page table
            auto vit = processFrames.find(victimPid);
            if (vit != processFrames.end()) {
                if (victimVPage >= 0 && victimVPage < (int)vit->second.size()) {
                    vit->second[victimVPage] = -1;
                }
            }
        }
        freeFrame = victim;
    }

    // load page bytes from backing store (if present) or zero page
    uint64_t key = (static_cast<uint64_t>(pid) << 32) | static_cast<uint32_t>(vpage);
    auto bsIt = backingStoreMap.find(key);
    if (bsIt != backingStoreMap.end()) {
        frameData[freeFrame] = bsIt->second;
    } else {
        frameData[freeFrame].assign(memPerFrame, 0);
    }

    // install mapping
    frameOwnerPID[freeFrame] = pid;
    frameOwnerVPage[freeFrame] = vpage;
    frames[freeFrame] = proc;
    pt[vpage] = freeFrame;

    pagedInCount++;
    return freeFrame;
}

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
    if (it != backingStoreMap.end()) {
        frameData[frameIndex] = it->second;
    } else {
        frameData[frameIndex].assign(memPerFrame, 0);
    }
}

// wordAddress is a 16-bit-word index (each word = 2 bytes). Returns false on error/violation.
bool MemoryManager::writeUint16(process_console* proc, uint32_t wordAddress, uint16_t value) {
    if (!proc) return false;

    if (wordAddress >= totalWords) {
        // Access violation: outside global memory
        std::ostringstream oss;
        oss << "Access violation on WRITE at address 0x" << std::hex << wordAddress;
        proc->logs.push_back(oss.str());
        proc->status = process_console::TERMINATED;
        proc->isActive = false;
        return false;
    }

    // Convert to process-virtual page + offset (words-per-page)
    uint32_t wordsPerFrame = static_cast<uint32_t>(memPerFrame / 2);
    if (wordsPerFrame == 0) wordsPerFrame = 1;
    int vpage = static_cast<int>(wordAddress / wordsPerFrame);
    uint32_t offsetWord = wordAddress % wordsPerFrame;
    uint32_t offsetByte = static_cast<uint32_t>(offsetWord * 2);

    int pid = proc->getProcessID();
    auto ptabIt = processFrames.find(pid);
    if (ptabIt == processFrames.end() || vpage < 0 || vpage >= (int)ptabIt->second.size()) {
        std::ostringstream oss;
        oss << "Access violation on WRITE at address 0x" << std::hex << wordAddress;
        proc->logs.push_back(oss.str());
        proc->status = process_console::TERMINATED;
        proc->isActive = false;
        return false;
    }

    int frameIdx = ptabIt->second[vpage];
    if (frameIdx == -1) {
        frameIdx = handlePageFault(proc, vpage);
        if (frameIdx == -1) {
            std::ostringstream oss;
            oss << "Page fault failed on WRITE at address 0x" << std::hex << wordAddress;
            proc->logs.push_back(oss.str());
            proc->status = process_console::TERMINATED;
            proc->isActive = false;
            return false;
        }
    }

    uint8_t lo = static_cast<uint8_t>(value & 0xff);
    uint8_t hi = static_cast<uint8_t>((value >> 8) & 0xff);

    // If both bytes fit in this frame
    if (offsetByte + 1 < (uint32_t)memPerFrame) {
        frameData[frameIdx][offsetByte] = lo;
        frameData[frameIdx][offsetByte + 1] = hi;
    } else {
        // crosses page boundary: write low byte here, high byte to next virtual word
        frameData[frameIdx][offsetByte] = lo;
        int nextVPage = vpage + 1;
        if (nextVPage >= (int)ptabIt->second.size()) {
            std::ostringstream oss;
            oss << "Access violation on WRITE crossing boundary at 0x" << std::hex << wordAddress;
            proc->logs.push_back(oss.str());
            proc->status = process_console::TERMINATED;
            proc->isActive = false;
            return false;
        }
        int nextFrame = ptabIt->second[nextVPage];
        if (nextFrame == -1) {
            nextFrame = handlePageFault(proc, nextVPage);
            if (nextFrame == -1) {
                std::ostringstream oss;
                oss << "Page fault failed on WRITE boundary at 0x" << std::hex << wordAddress;
                proc->logs.push_back(oss.str());
                proc->status = process_console::TERMINATED;
                proc->isActive = false;
                return false;
            }
        }
        // high byte at byte offset 0 of next frame
        frameData[nextFrame][0] = hi;
    }

    memoryWords[wordAddress] = value;
    return true;
}

bool MemoryManager::readUint16(process_console* proc, uint32_t wordAddress, uint16_t& outValue) {
    outValue = 0;
    if (!proc) return false;

    if (wordAddress >= totalWords) {
        std::ostringstream oss;
        oss << "Access violation on READ at address 0x" << std::hex << wordAddress;
        proc->logs.push_back(oss.str());
        proc->status = process_console::TERMINATED;
        proc->isActive = false;
        return false;
    }

    uint32_t wordsPerFrame = static_cast<uint32_t>(memPerFrame / 2);
    if (wordsPerFrame == 0) wordsPerFrame = 1;
    int vpage = static_cast<int>(wordAddress / wordsPerFrame);
    uint32_t offsetWord = wordAddress % wordsPerFrame;
    uint32_t offsetByte = static_cast<uint32_t>(offsetWord * 2);

    int pid = proc->getProcessID();
    auto ptabIt = processFrames.find(pid);
    if (ptabIt == processFrames.end() || vpage < 0 || vpage >= (int)ptabIt->second.size()) {
        std::ostringstream oss;
        oss << "Access violation on READ at address 0x" << std::hex << wordAddress;
        proc->logs.push_back(oss.str());
        proc->status = process_console::TERMINATED;
        proc->isActive = false;
        return false;
    }

    int frameIdx = ptabIt->second[vpage];
    if (frameIdx == -1) {
        frameIdx = handlePageFault(proc, vpage);
        if (frameIdx == -1) {
            std::ostringstream oss;
            oss << "Page fault failed on READ at address 0x" << std::hex << wordAddress;
            proc->logs.push_back(oss.str());
            proc->status = process_console::TERMINATED;
            proc->isActive = false;
            return false;
        }
    }

    // If both bytes present in this frame
    if (offsetByte + 1 < (uint32_t)memPerFrame) {
        uint8_t b0 = frameData[frameIdx][offsetByte];
        uint8_t b1 = frameData[frameIdx][offsetByte + 1];
        outValue = static_cast<uint16_t>(b0 | (b1 << 8));
    } else {
        // cross page boundary
        uint8_t b0 = frameData[frameIdx][offsetByte];
        int nextVPage = vpage + 1;
        if (nextVPage >= (int)ptabIt->second.size()) {
            std::ostringstream oss;
            oss << "Access violation on READ crossing boundary at 0x" << std::hex << wordAddress;
            proc->logs.push_back(oss.str());
            proc->status = process_console::TERMINATED;
            proc->isActive = false;
            return false;
        }
        int nextFrame = ptabIt->second[nextVPage];
        if (nextFrame == -1) {
            nextFrame = handlePageFault(proc, nextVPage);
            if (nextFrame == -1) {
                std::ostringstream oss;
                oss << "Page fault failed on READ boundary at 0x" << std::hex << wordAddress;
                proc->logs.push_back(oss.str());
                proc->status = process_console::TERMINATED;
                proc->isActive = false;
                return false;
            }
        }
        uint8_t b1 = frameData[nextFrame][0];
        outValue = static_cast<uint16_t>(b0 | (b1 << 8));
    }

    memoryWords[wordAddress] = outValue;
    return true;
}

uint64_t MemoryManager::getPagedInCount() const {
    return pagedInCount;
}

uint64_t MemoryManager::getPagedOutCount() const {
    return pagedOutCount;
}

