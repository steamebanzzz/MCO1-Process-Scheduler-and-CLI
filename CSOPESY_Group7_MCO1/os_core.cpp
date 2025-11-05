#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <format>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <climits>
#include "os_core.h"
#include "process_console.h"

using namespace std;

int num_cpu;
string scheduler;
int quantum_cycles;
int batch_process_freq;
int min_ins;
int max_ins;
int delay_per_exec;
const uint64_t MAX_VALUE = 4294967296;

bool scheduler_test_run = false;

void ConsoleManager::initialize() {

    readConfig("config.txt");

    coreCount = num_cpu;
    availableCores = num_cpu;

    cpuCores = vector<bool>(num_cpu, false);
    startScheduler();
}

void ConsoleManager::readConfig(const string& filename) {
    ifstream configFile(filename);
    if (!configFile.is_open()) {
        cerr << "Error: Could not open config file.\n";
        return;
    }

    string line;
    while (getline(configFile, line)) {
        istringstream iss(line);
        string key;
        if (!(iss >> key)) continue;

        if (key == "num-cpu") {
            iss >> num_cpu;
            if (num_cpu < 1 || num_cpu > 128) {
                cerr << "Error: Invalid num-cpu value: " << num_cpu << ". Must be in range [1, 128].\n";
                return;
            }
        }
        else if (key == "scheduler") {
            string value;
            iss >> quoted(value);
            scheduler = value;
            if (scheduler != "fcfs" && scheduler != "rr") {
                cerr << "Error: Invalid scheduler value: '" << scheduler << "'. Must be 'fcfs' or 'rr'.\n";
                return;
            }
        }
        else if (key == "quantum-cycles") {
            iss >> quantum_cycles;
            if (quantum_cycles < 1 || quantum_cycles > MAX_VALUE) {
                cerr << "Error: Invalid quantum-cycles value: " << quantum_cycles << ". Must be in range [1, " << MAX_VALUE << "].\n";
                return;
            }
        }
        else if (key == "batch-process-freq") {
            iss >> batch_process_freq;
            if (batch_process_freq < 1 || batch_process_freq > MAX_VALUE) {
                cerr << "Error: Invalid batch-process-freq value: " << batch_process_freq << ". Must be in range [1, " << MAX_VALUE << "].\n";
                return;
            }
        }
        else if (key == "min-ins") {
            iss >> min_ins;
            if (min_ins < 1 || min_ins > MAX_VALUE) {
                cerr << "Error: Invalid min-ins value: " << min_ins << ". Must be in range [1, " << MAX_VALUE << "].\n";
                return;
            }
        }
        else if (key == "max-ins") {
            iss >> max_ins;
            if (max_ins < 1 || max_ins > MAX_VALUE) {
                cerr << "Error: Invalid max-ins value: " << max_ins << ". Must be in range [1, " << MAX_VALUE << "].\n";
                return;
            }
        }
        else if (key == "delay-per-exec") {
            iss >> delay_per_exec;
            if (delay_per_exec < 0 || delay_per_exec > MAX_VALUE) {
                cerr << "Error: Invalid delay-per-exec value: " << delay_per_exec << ". Must be in range [0, " << MAX_VALUE << "].\n";
                return;
            }
        }
        else {
            cerr << "Error: Unknown parameter in config file: " << key << endl;
            return;
        }
    }
    configFile.close();
    return;
}

// DEBUGGING PURPOSE
void ConsoleManager::testConfig() {
    cout << "num-cpu: " << num_cpu << endl;
    cout << "scheduler: " << scheduler << endl;
    cout << "quantum-cycles: " << quantum_cycles << endl;
    cout << "batch-process-freq: " << batch_process_freq << endl;
    cout << "min-ins: " << min_ins << endl;
    cout << "max-ins: " << max_ins << endl;
    cout << "delay-per-exec: " << delay_per_exec << endl;
}

void ConsoleManager::addConsole(const string& name, bool fromScreenCommand = false) {
    lock_guard<mutex> lock(processMutex);

    if (consoles.find(name) != consoles.end()) {
        cout << "Console \"" << name << "\" already exists." << endl;
        return;
    }

    static int nextId = 1;
    int processId = nextId++;

    random_device rd;
    knuth_b knuth_gen(rd());
    uniform_int_distribution<> dist(min_ins, max_ins);
    int maxInstructions = dist(knuth_gen);

    process_console* newConsole = new process_console(name, maxInstructions);
    newConsole->setProcessID(processId);
    newConsole->setInstructionLine(0);

    waitingQueue.push(newConsole);
    consoles[name] = newConsole;

    if (fromScreenCommand) {
        displayConsole(name);
    }
}


void ConsoleManager::displayConsole(const string& name) const {
    auto it = consoles.find(name);
    if (it != consoles.end()) {
        process_console* console = it->second;
        system("cls");

        cout << "Process: \"" << console->getName() << "\"" << endl;
        cout << "ID: " << console->getProcessID() << endl;
        cout << "Current Line of Instruction: " << console->getInstructionLine() << endl;
        cout << "Lines of Code: " << console->getInstructionTotal() << endl;
    }
    else {
        cout << "Console \"" << name << "\" does not exist." << endl;
    }
}

void ConsoleManager::displayCPUInfo() {
    int usedCores = coreCount - availableCores;

    float cpuUsage = 0.0;
    if (usedCores > 0) {
        cpuUsage = (usedCores / (float)coreCount) * 100;
    }

    cout << "CPU Cores: " << coreCount << endl;
    cout << "CPU Utilization: " << fixed << setprecision(2) << cpuUsage << "%" << endl;
    cout << "Cores used: " << usedCores << endl;
    cout << "Cores available: " << availableCores << endl;
}

void ConsoleManager::listConsoles() {
    lock_guard<mutex> lock(processMutex);

    displayCPUInfo();

    cout << "\n-----------------------------------------\n";

    if (!hasConsoles()) {
        cout << "No consoles to list.\n";
        return;
    }

    bool hasQueued = false;
    bool hasRunning = false;
    bool hasFinished = false;


    cout << "Running Processes:\n";
    for (const auto& consolePair : consoles) {
        process_console* console = consolePair.second;
        if (console->getStatus() == process_console::RUNNING) {
            hasRunning = true;
            cout << console->getName() + "\t" + console->getTimestamp() + "\tCore: " + to_string(console->getCoreID()) + "\t" + to_string(console->getInstructionLine()) + "/" + to_string(console->getInstructionTotal()) + "\n";
        }
    }
    cout << "\n";
    if (!hasRunning) cout << "No running consoles.\n";

    cout << "Finished Processes:\n";
    for (const auto& consolePair : consoles) {
        process_console* console = consolePair.second;
        if (console->getStatus() == process_console::TERMINATED) {
            hasFinished = true;
            cout << console->getName() + "\t" + console->getTimestamp() + "\tFinished\t" + to_string(console->getInstructionLine()) + "/" + to_string(console->getInstructionTotal()) + "\n";
        }
    }
    cout << "\n";
    if (!hasFinished) cout << "No terminated consoles.\n";
}

void ConsoleManager::reportUtil() {
    lock_guard<mutex> lock(processMutex);

    string fileName = "console_report.txt";
    ofstream outFile(fileName, ios::out | ios::trunc);

    if (!outFile.is_open()) {
        cerr << "Error: unable to open file for writing report details";
        return;
    }

    outFile << "Console Report\n";
    outFile << "-----------------------------------------\n";

    int usedCores = coreCount - availableCores;

    float cpuUsage = 0.0;
    if (usedCores > 0) {
        cpuUsage = (usedCores / (float)coreCount) * 100;
    }

    outFile << "CPU Cores: " << coreCount << endl;
    outFile << "CPU Utilization: " << fixed << setprecision(2) << cpuUsage << "%" << endl;
    outFile << "Cores used: " << usedCores << endl;
    outFile << "Cores available: " << availableCores << endl;

    if (!hasConsoles()) {
        outFile << "No consoles to list.\n";
        outFile.close();
        return;
    }

    bool hasQueued = false;
    bool hasRunning = false;
    bool hasFinished = false;

    outFile << "Running Processes:\n";
    for (const auto& consolePair : consoles) {
        process_console* console = consolePair.second;
        if (console->getStatus() == process_console::RUNNING) {
            hasRunning = true;
            outFile << console->getName() + "\t" + console->getTimestamp() + "\tCore: " + to_string(console->getCoreID()) + "\t" + to_string(console->getInstructionLine()) + "/" + to_string(console->getInstructionTotal()) + "\n";
        }
    }
    outFile << "\n";
    if (!hasRunning) outFile << "No running consoles.\n";

    outFile << "Finished Processes:\n";
    for (const auto& consolePair : consoles) {
        process_console* console = consolePair.second;
        if (console->getStatus() == process_console::TERMINATED) {
            hasFinished = true;
            outFile << console->getName() + "\t" + console->getTimestamp() + "\tFinished\t" + to_string(console->getInstructionLine()) + "/" + to_string(console->getInstructionTotal()) + "\n";
        }
    }
    outFile << "\n";
    if (!hasFinished) outFile << "No terminated consoles.\n";

    outFile.close();
    cout << "Report generated: " << fileName << "\n";
}

void generateRandomInstructions(process_console* proc, int instructionCount) {
    static std::default_random_engine rng(std::random_device{}());
    static std::uniform_int_distribution<int> dist(0, 4);

    for (int i = 0; i < instructionCount; i++) {
        Instruction instr;
        int type = dist(rng);

        switch (type) {
        case 0:
            instr.type = PRINT;
            break;
        case 1:
            instr.type = DECLARE;
            instr.var1 = "x";
            instr.value1 = rand() % 100;
            break;
        case 2:
            instr.type = ADD;
            instr.var1 = "x";
            instr.value1 = rand() % 10;
            instr.value2 = rand() % 10;
            break;
        case 3:
            instr.type = SUBTRACT;
            instr.var1 = "x";
            instr.value1 = rand() % 10;
            instr.value2 = rand() % 10;
            break;
        case 4:
            instr.type = SLEEP;
            instr.value1 = 2;
            break;
        }

        proc->instructions.push_back(instr);
    }
}

void ConsoleManager::startScheduler() {
    if (scheduler == "fcfs") {
        thread schedulerThread(&ConsoleManager::schedulerFCFS, this);
        schedulerThread.detach();
    }
    else if (scheduler == "rr") {
        thread schedulerThread(&ConsoleManager::schedulerRR, this);
        schedulerThread.detach();
    }
}

bool ConsoleManager::consoleExists(const string& name) const {
    for (const auto& console : consoles) {
        if (console.second->getName() == name) {
            return true;
        }
    }
    return false;
}

bool ConsoleManager::hasConsoles() const {
    return !consoles.empty();
}

void ConsoleManager::loopConsole(const string& name) {
    for (auto& console : consoles) {
        if (console.second->getName() == name) {
            vector<string> buffer;
            string input;
            currentConsole = true;

            do {
                buffer.clear();
                cout << "Console [" << console.second->getName() << "] Enter a command: ";

                while (cin >> input) {
                    buffer.push_back(input);
                    if (cin.peek() == '\n') break;
                }

                if (buffer.empty()) continue;

                const string& command = buffer[0];

                if (command == "exit") {
                    return;
                }
                else if (command == "process-smi") {
                    process_console* proc = console.second;

                    schedulerPaused = true;

                    if (proc->instructions.empty()) {
                        generateRandomInstructions(proc, proc->getInstructionTotal());
                    }

                    cout << "Process: \"" << proc->getName() << "\"\n";
                    cout << "ID: " << proc->getProcessID() << "\n";
                    cout << "Current Line of Instruction: " << proc->getInstructionLine() << "\n";
                    cout << "Lines of Code: " << proc->getInstructionTotal() << "\n";

                    cout << "\n--- Logs ---\n";
                    if (proc->logs.empty())
                        cout << "(no logs yet)\n";
                    else
                        for (const auto& log : proc->logs)
                            cout << log << endl;

                    cout << "\n--- Variables ---\n";
                    if (proc->variables.empty())
                        cout << "(no variables)\n";
                    else
                        for (const auto& entry : proc->variables)
                            cout << entry.first << " = " << entry.second << endl;

                    cout << "\n--- Instructions ---\n";
                    if (proc->instructions.empty())
                        cout << "(no instructions)\n";
                    else {
                        int idx = 1;
                        for (const auto& instr : proc->instructions) {
                            cout << idx++ << ". ";
                            switch (instr.type) {
                            case DECLARE: cout << "DECLARE " << instr.var1 << " = " << instr.value1; break;
                            case ADD: cout << "ADD " << instr.var1; break;
                            case SUBTRACT: cout << "SUBTRACT " << instr.var1; break;
                            case PRINT: cout << "PRINT"; break;
                            case SLEEP: cout << "SLEEP " << instr.value1; break;
                            case FOR_LOOP: cout << "FOR_LOOP (" << instr.repeats << "x)"; break;
                            }
                            cout << endl;
                        }
                    }

                    if (proc->getStatus() == process_console::TERMINATED)
                        cout << "\nFinished!" << endl;

                    schedulerPaused = false;
                }
                else if (command == "finished") {
                    cout << "Finished Processes:\n";
                    bool hasFinished = false;
                    for (const auto& consolePair : consoles) {
                        process_console* proc = consolePair.second;
                        if (proc->getStatus() == process_console::TERMINATED) {
                            hasFinished = true;
                            cout << proc->getName() + "\t" +
                                proc->getTimestamp() + "\t" +
                                "Finished\t" +
                                to_string(proc->getInstructionLine()) + "/" +
                                to_string(proc->getInstructionTotal()) + "\n";
                        }
                    }
                    if (!hasFinished) cout << "No finished consoles.\n";
                }
                else {
                    cout << "Command [" << command << "] not recognized. Try again." << "\n";
                }

            } while (currentConsole);
            return;
        }
    }
}

process_console::Status ConsoleManager::getConsoleStatus(const string& name) const {
    auto it = consoles.find(name);
    if (it != consoles.end()) {
        return it->second->getStatus();
    }
    return process_console::TERMINATED;
}

void ConsoleManager::schedulerTest(bool set_scheduler) {
    scheduler_test_run = set_scheduler;

    thread([this] {
        int cycles = 1;
        int i = 1;

        while (scheduler_test_run) {
            if (cycles % batch_process_freq == 0) {
                if (i < 10)
                    addConsole("process00" + to_string(i));
                else if (i < 100)
                    addConsole("process0" + to_string(i));
                else
                    addConsole("process" + to_string(i));

                i++;
            }
            this_thread::sleep_for(chrono::milliseconds(100));
            cycles++;
        }
        }).detach();
}

void ConsoleManager::schedulerFCFS() {
    while (true) {
        std::unique_lock<std::mutex> lock(processMutex);

        schedulerCV.wait_for(lock, std::chrono::milliseconds(2));

        if (schedulerPaused)
            continue;

        for (int i = 0; i < cpuCores.size(); ++i) {
            if (!cpuCores[i] && !waitingQueue.empty()) {
                process_console* nextProcess = waitingQueue.front();
                waitingQueue.pop();
                if (nextProcess->getStatus() == process_console::TERMINATED ||
                    nextProcess->getInstructionLine() >= nextProcess->getInstructionTotal()) {
                    continue; 
                }

                cpuCores[i] = true;
                availableCores--;

                runningProcesses[nextProcess->getName()] = std::thread([this, nextProcess, i]() {
                    nextProcess->runProcess(i, 0, delay_per_exec);

                    std::lock_guard<std::mutex> lock(processMutex);
                    cpuCores[i] = false;
                    availableCores++;

                    schedulerCV.notify_one();  
                    });

                runningProcesses[nextProcess->getName()].detach();
            }
        }
    }
}

void ConsoleManager::schedulerRR() {
    while (true) {
        std::unique_lock<std::mutex> lock(processMutex);

        schedulerCV.wait_for(lock, std::chrono::milliseconds(2));

        if (schedulerPaused)
            continue;

        for (int i = 0; i < cpuCores.size(); ++i) {
            if (!cpuCores[i] && !waitingQueue.empty()) {
                process_console* nextProcess = waitingQueue.front();
                waitingQueue.pop();
                if (nextProcess->getStatus() == process_console::TERMINATED ||
                    nextProcess->getInstructionLine() >= nextProcess->getInstructionTotal()) {
                    continue;
                }

                cpuCores[i] = true;
                availableCores--;

                runningProcesses[nextProcess->getName()] = std::thread([this, nextProcess, i]() {
                    nextProcess->runProcess(i, quantum_cycles, delay_per_exec);

                    std::lock_guard<std::mutex> lock(processMutex);

                    if (nextProcess->getIsActive() && nextProcess->getInstructionLine() < nextProcess->getInstructionTotal()) {
                        waitingQueue.push(nextProcess);
                    }

                    cpuCores[i] = false;
                    availableCores++;

                    schedulerCV.notify_one();  
                    });

                runningProcesses[nextProcess->getName()].detach();
            }
        }
    }
}

static uint16_t clampUint16(int value) {
    if (value < 0) return 0;
    if (value > UINT16_MAX) return UINT16_MAX;
    return static_cast<uint16_t>(value);
}
