#pragma once
#include <map>
#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <string>
#include <vector>
#include "process_console.h"

using namespace std;

class ConsoleManager {
private:
    map<string, process_console*> consoles;
    bool reportingMode = false;
    bool currentConsole = false;
    bool schedulerRunning = false;
    int coreCount;
    int availableCores;
    vector<bool> cpuCores;
    queue<process_console*> waitingQueue;
    map<string, thread> runningProcesses;
    mutex processMutex;

public:
    void initialize();
    void addConsole(const string& name, bool fromScreenCommand);
    void readConfig(const string& filename);
    void testConfig();
    void displayConsole(const string& name) const;
    void displayCPUInfo();
    void listConsoles();
    void reportUtil();
    void startScheduler();
    bool consoleExists(const string& name) const;
    bool hasConsoles() const;
    process_console::Status getConsoleStatus(const string& name) const;
    void loopConsole(const string& name);
    void schedulerTest(bool set_scheduler);
    void schedulerFCFS();
    void schedulerRR();
};
