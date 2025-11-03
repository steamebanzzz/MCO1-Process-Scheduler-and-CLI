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

/*
* This function reads the config.txt file and initializes parameters
*/
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
            iss >> quoted(value);  // Use std::quoted to handle quotes
            scheduler = value;  // Assign the stripped value
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
    //cout << "Configuration successfully loaded.\n";
    configFile.close();
    return;
}

void ConsoleManager::testConfig() {
    // test if the config file was read successfully, print all values
    cout << "num-cpu: " << num_cpu << endl;
    cout << "scheduler: " << scheduler << endl;
    cout << "quantum-cycles: " << quantum_cycles << endl;
    cout << "batch-process-freq: " << batch_process_freq << endl;
    cout << "min-ins: " << min_ins << endl;
    cout << "max-ins: " << max_ins << endl;
    cout << "delay-per-exec: " << delay_per_exec << endl;
}

/*
* This function adds a new console to the list of consoles
*
* @param name - the name of the console
*/
void ConsoleManager::addConsole(const string& name, bool fromScreenCommand = false) {
    lock_guard<mutex> lock(processMutex);

    // Check if the console name already exists
    if (consoles.find(name) != consoles.end()) {
        cout << "Console \"" << name << "\" already exists." << endl;
        return;
    }

    // Generate unique process ID
    static int nextId = 1;
    int processId = nextId++;

    // Random number of instructions between min_ins and max_ins
    random_device rd;
    knuth_b knuth_gen(rd());
    uniform_int_distribution<> dist(min_ins, max_ins);
    int maxInstructions = dist(knuth_gen);

    // Create console
    process_console* newConsole = new process_console(name, maxInstructions);
    newConsole->setProcessID(processId);
    newConsole->setInstructionLine(0);

    // Add to waiting queue and map
    waitingQueue.push(newConsole);
    consoles[name] = newConsole;

    if (fromScreenCommand) {
        displayConsole(name);
    }
}


/*
* This function displays the information of the specified console
*
* @param name - the name of the console
*/
void ConsoleManager::displayConsole(const string& name) const {
    // Check if the console name exists in the map
    auto it = consoles.find(name);
    if (it != consoles.end()) {
        process_console* console = it->second;  
        system("cls");

        // Display console information
        cout << "Process: \"" << console->getName() << "\"" << endl;
        cout << "ID: " << console->getProcessID() << endl;  
        // cout << "Created At: " << console->getTimestamp() << endl;
        cout << "Current Line of Instruction: " << console->getInstructionLine() << endl;
        cout << "Lines of Code: " << console->getInstructionTotal() << endl;
    }
    else {
        // If console does not exist, display a message
        cout << "Console \"" << name << "\" does not exist." << endl;
    }
}

/*
* This function displays the current general CPU info
*/
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

/*
* This function lists the status of all the consoles in the console screen
*/
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
        // get all running consoles
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
        // get all finished consoles
        if (console->getStatus() == process_console::TERMINATED) {
            hasFinished = true;
            cout << console->getName() + "\t" + console->getTimestamp() + "\tFinished\t" + to_string(console->getInstructionLine()) + "/" + to_string(console->getInstructionTotal()) + "\n";
        }
    }
    cout << "\n";
    if (!hasFinished) cout << "No terminated consoles.\n";
}

/*
* This function prints the status of all the consoles as a .txt file
*/
void ConsoleManager::reportUtil() {
    lock_guard<mutex> lock(processMutex);

    string fileName = "console_report.txt";
    ofstream outFile(fileName, ios::out | ios::trunc);

    if (!outFile.is_open()) {
        cerr << "Error: unable to open file for writing report details";
        return;
    }

    // Start writing to the file
    outFile << "Console Report\n";
    outFile << "-----------------------------------------\n";

    // Display CPU Info
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

/*
* This function checks if the specified console exists
*
* @param name - the name of the console
* @return true if the console exists, false otherwise
*/
bool ConsoleManager::consoleExists(const string& name) const {
    // Check if the console name exists in the list of consoles
    for (const auto& console : consoles) {
        // If console exists
        if (console.second->getName() == name) {
            return true;
        }
    }
    // If console does not exist
    return false;
}

/*
* This function checks if the list of consoles is empty or not
*
* @return true if the list of consoles is not empty, false otherwise
*/
bool ConsoleManager::hasConsoles() const {
    return !consoles.empty();
}

/*
* This function initializes and runs the console program inside the specified console
*
* @param name - the name of the console
*/
void ConsoleManager::loopConsole(const string& name) {
    // Find specified console in the list of consoles
    for (auto& console : consoles) {
        // If console exists
        if (console.second->getName() == name) {
            vector<string> buffer;
            string input;
            currentConsole = true;

            // Start console program
            do {
                buffer.clear();
                cout << "Console [" << console.second->getName() << "] Enter a command: ";

                // Read user input
                while (cin >> input) {
                    buffer.push_back(input);
                    if (cin.peek() == '\n') break;
                }

                if (buffer.empty()) continue;

                const string& command = buffer[0];

                if (command == "exit") {
                    return;  // Exit command
                }
                else if (command == "process-smi") {
                    process_console* proc = console.second;

                    // Generate instructions if none exist
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
                }
                else if (command == "finished") {
                    // Display finished processes
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
            return; // Exit console if not found
        }
    }
}

process_console::Status ConsoleManager::getConsoleStatus(const string& name) const {
    // Check if the console name exists in the map
    auto it = consoles.find(name);
    if (it != consoles.end()) {
        return it->second->getStatus(); // Return the status of the console
    }
    // If console does not exist, return a default status (or handle it as you prefer)
    return process_console::TERMINATED; // Assuming terminated is a safe fallback; you can change this
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
        this_thread::sleep_for(chrono::milliseconds(10));

        lock_guard<mutex> lock(processMutex);

        for (int i = 0; i < cpuCores.size(); ++i) {
            if (!cpuCores[i] && !waitingQueue.empty()) {
                process_console* nextProcess = waitingQueue.front();
                waitingQueue.pop();

                cpuCores[i] = true;
                availableCores--;

                runningProcesses[nextProcess->getName()] = thread([this, nextProcess, i]() {
                    nextProcess->runProcess(i, 0, delay_per_exec);
                    lock_guard<mutex> lock(processMutex);
                    cpuCores[i] = false;
                    availableCores++;
                    });

                runningProcesses[nextProcess->getName()].detach();
            }
        }


    }
}

void ConsoleManager::schedulerRR() {
    while (true) {
        this_thread::sleep_for(chrono::milliseconds(10));

        lock_guard<mutex> lock(processMutex);

        for (int i = 0; i < cpuCores.size(); ++i) {
            if (!cpuCores[i] && !waitingQueue.empty()) {
                process_console* nextProcess = waitingQueue.front();
                waitingQueue.pop();

                cpuCores[i] = true;
                availableCores--;

                runningProcesses[nextProcess->getName()] = thread([this, nextProcess, i]() {
                    nextProcess->runProcess(i, quantum_cycles, delay_per_exec);
                    lock_guard<mutex> lock(processMutex);

                    // If the process has not completed, requeue it
                    if (nextProcess->getIsActive() && nextProcess->getInstructionLine() < nextProcess->getInstructionTotal()) {
                        waitingQueue.push(nextProcess);
                    }
                    cpuCores[i] = false;
                    availableCores++;
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

void executeInstruction(process_console* proc, const Instruction& instr) {
    if (proc->finished) return;

    switch (instr.type) {
    case DECLARE:
        proc->variables[instr.var1] = instr.value1;
        break;

    case ADD: {
        uint16_t a = proc->variables.count(instr.var2) ? proc->variables[instr.var2] : instr.value1;
        uint16_t b = proc->variables.count(instr.var3) ? proc->variables[instr.var3] : instr.value2;
        proc->variables[instr.var1] = clampUint16(a + b);
        break;
    }

    case SUBTRACT: {
        uint16_t a = proc->variables.count(instr.var2) ? proc->variables[instr.var2] : instr.value1;
        uint16_t b = proc->variables.count(instr.var3) ? proc->variables[instr.var3] : instr.value2;
        proc->variables[instr.var1] = clampUint16(a - b);
        break;
    }

    case PRINT: {
        std::string msg = instr.message.empty()
            ? "Hello world from " + proc->getName() + "!"
            : instr.message;
        proc->logs.push_back(msg);
        break;
    }

    case SLEEP:
        proc->sleepTicks = instr.value1;
        break;

    case FOR_LOOP:
        for (uint16_t i = 0; i < instr.repeats; i++) {
            for (auto& sub : instr.subInstructions) {
                executeInstruction(proc, sub);
            }
        }
        break;
    }
}