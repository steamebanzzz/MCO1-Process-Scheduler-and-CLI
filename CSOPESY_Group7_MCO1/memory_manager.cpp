#include <sstream>
#include "memory_manager.h"
#include "process_console.h"

std::unordered_map<uint32_t, uint16_t> memoryWords; 
uint32_t totalWords = 0;

MemoryManager::MemoryManager(int maxMem, int memPerFrame)
    : totalMemory(maxMem), memPerFrame(memPerFrame) {
    frameCount = totalMemory / memPerFrame;
    frames.resize(frameCount, nullptr);  // all frames free initially

    totalWords = static_cast<uint32_t>(totalMemory / 2);

    // Clear backing store file
    std::ofstream ofs(backingStoreFile, std::ios::trunc);
    if (!ofs) std::cerr << "Error creating backing store file.\n";
}

// TESTING PURPOSES
void MemoryManager::printFrameTable() {
    cout << "\n[Frame Allocation Table]\n";
    for (int i = 0; i < frames.size(); i++) {
        if (frames[i] == nullptr)
            cout << "Frame " << i << ": Free\n";
        else
            cout << "Frame " << i << ": Process PID " << frames[i]->getProcessID() << "\n";
    }
    cout << endl;
}

bool MemoryManager::allocateMemory(process_console* proc, int memoryRequired) {
    int requiredFrames = (memoryRequired + memPerFrame - 1) / memPerFrame;
    std::vector<int> allocated;

    for (int i = 0; i < frames.size() && allocated.size() < requiredFrames; ++i) {
        if (!frames[i]) {
            frames[i] = proc;
            allocated.push_back(i);
        }
    }

    if (allocated.size() < requiredFrames) {
        // Not enough free frames → trigger page replacement
        for (int i = 0; i < requiredFrames - allocated.size(); ++i) {
            auto it = std::find_if(frames.begin(), frames.end(), [](process_console* p) { return p != nullptr; });
            int victimFrame = std::distance(frames.begin(), it);
            if (victimFrame >= frameCount) break;

            saveFrameToBackingStore(victimFrame);
            frames[victimFrame] = proc;  // allocate to new process
            allocated.push_back(victimFrame);

            // Remove victim frame from old process
            for (auto& kv : processFrames) {
                auto& vec = kv.second;
                auto it = std::find(vec.begin(), vec.end(), victimFrame);
                if (it != vec.end()) {
                    vec.erase(it);
                    break;
                }
            }
        }
    }

    if (allocated.empty()) return false;

    processFrames[proc->getProcessID()] = allocated;
    return true;
}

void MemoryManager::deallocateMemory(process_console* proc) {
    int pid = proc->getProcessID();
    if (processFrames.find(pid) != processFrames.end()) {
        for (int frame : processFrames[pid]) {
            frames[frame] = proc;
        }
        processFrames.erase(pid);
    }
}

int MemoryManager::handlePageFault(process_console* proc, int frameIndex) {
    // For simplicity, just load frame from backing store
    loadFrameFromBackingStore(frameIndex);
    return frameIndex;
}

void MemoryManager::saveFrameToBackingStore(int frameIndex) {
    std::ofstream ofs(backingStoreFile, std::ios::app);
    ofs << "Frame " << frameIndex << " saved to backing store\n";
}

void MemoryManager::loadFrameFromBackingStore(int frameIndex) {
    std::ifstream ifs(backingStoreFile);
    std::string line;
    while (getline(ifs, line)) {
        if (line.find("Frame " + std::to_string(frameIndex)) != std::string::npos) {
            // Found frame in backing store
            break;
        }
    }
}

bool MemoryManager::writeUint16(process_console* proc, uint32_t wordAddress, uint16_t value) {
    if (wordAddress >= totalWords) {
        // Access violation
        if (proc) {
            proc->logs.push_back("Access violation on WRITE at address 0x" +
                (std::stringstream() << std::hex << wordAddress, std::string())); // workaround below
            // Because stream concat above is clumsy, we'll do the string in a simpler way:
            std::ostringstream oss;
            oss << "Access violation on WRITE at address 0x" << std::hex << wordAddress;
            proc->logs.push_back(oss.str());
            proc->status = process_console::TERMINATED;
        }
        return false;
    }
    memoryWords[wordAddress] = value;
    return true;
}

bool MemoryManager::readUint16(process_console* proc, uint32_t wordAddress, uint16_t& outValue) {
    if (wordAddress >= totalWords) {
        if (proc) {
            std::ostringstream oss;
            oss << "Access violation on READ at address 0x" << std::hex << wordAddress;
            proc->logs.push_back(oss.str());
            proc->status = process_console::TERMINATED;
        }
        return false;
    }
    auto it = memoryWords.find(wordAddress);
    if (it == memoryWords.end()) {
        outValue = 0; // uninitialized -> 0 as spec says
    }
    else {
        outValue = it->second;
    }
    return true;
}

