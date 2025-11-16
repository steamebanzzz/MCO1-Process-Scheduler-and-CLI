#pragma once
#include <map>
#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <string>
#include <vector>
#include <condition_variable>
#include "process_console.h"
#include "memory_manager.h"

using namespace std;

class ConsoleManager {
private:
    std::mutex processMutex;
    std::condition_variable schedulerCV;
    map<string, process_console*> consoles;
    bool reportingMode = false;
    bool currentConsole = false;
    bool schedulerRunning = false;
    bool schedulerPaused = false;
    int coreCount;
    int availableCores;
    vector<bool> cpuCores;
    queue<process_console*> waitingQueue;
    map<string, thread> runningProcesses;
    MemoryManager memManager;

public:
    int max_overall_mem;
    int mem_per_frame;
    int min_mem_per_proc;
    int max_mem_per_proc;
    ConsoleManager(int maxMem = 4096, int frameSize = 256)
        : memManager(maxMem, frameSize) {}
    void initialize();
    void addConsole(const string& name, bool fromScreenCommand = false);
    void readConfig(const string& filename);
    void testConfig();
    void testMemoryAllocation();
    void displayConsole(const string& name) const;
    void displayCPUInfo();
    void listConsoles();
    void reportUtil();

    void parseCommand(const std::string& input);
    void handleScreenS(const vector<string>& tokens);
    void handleScreenC(const vector<string>& tokens, const std::string& rawInput);
    void handleScreenR(const vector<string>& tokens);
    void handleProcessSMI();
    void handleVMStat();
    void handleSchedulerStart();

    void startScheduler();
    bool consoleExists(const string& name) const;
    bool hasConsoles() const;
    process_console::Status getConsoleStatus(const string& name) const;
    void loopConsole(const string& name);
    void schedulerTest(bool set_scheduler);
    void schedulerFCFS();
    void schedulerRR();
};
