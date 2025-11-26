#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <climits>
#include "os_core.h"
#include "process_console.h"
#include "memory_manager.h"

using namespace std;

int num_cpu;
string scheduler;
int quantum_cycles;
int batch_process_freq;
int min_ins;
int max_ins;
int delay_per_exec;
int max_overall_mem = 0;
int mem_per_frame = 0;
int min_mem_per_proc = 0;
int max_mem_per_proc = 0;
const uint64_t MAX_VALUE = 4294967296;

bool scheduler_test_run = false;

vector<string> tokenize(const string& input) {
    vector<string> tokens;
    string token;
    bool inQuotes = false;

    for (char c : input) {
        if (c == '"') {
            inQuotes = !inQuotes;
            continue;
        }
        if (c == ' ' && !inQuotes) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        }
        else {
            token.push_back(c);
        }
    }

    if (!token.empty()) tokens.push_back(token);

    return tokens;
}

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
        else if (key == "max-overall-mem") {
            iss >> max_overall_mem;
            if (max_overall_mem < 1 || max_overall_mem > MAX_VALUE) {
                cerr << "Error: Invalid max-overall_mem value: " << max_overall_mem << ". Must be >0.\n";
                return;
            }
        }
        else if (key == "mem-per-frame") {
            iss >> mem_per_frame;
            if (mem_per_frame < 1 || mem_per_frame > MAX_VALUE) {
                cerr << "Error: Invalid mem-per_frame value: " << mem_per_frame << ". Must be >0.\n";
                return;
            }
        }
        else if (key == "min-mem-per-proc") {
            iss >> min_mem_per_proc;
            if (min_mem_per_proc < 1 || min_mem_per_proc > MAX_VALUE) {
                cerr << "Error: Invalid min-mem_per-proc value: " << min_mem_per_proc << ". Must be >0.\n";
                return;
            }
        }
        else if (key == "max-mem-per-proc") {
            iss >> max_mem_per_proc;
            if (max_mem_per_proc < 1 || max_mem_per_proc > MAX_VALUE) {
                cerr << "Error: Invalid max_mem_per_proc value: " << max_mem_per_proc << ". Must be >0.\n";
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
    cout << "max-overall-mem: " << max_overall_mem << endl;
    cout << "mem-per-frame: " << mem_per_frame << endl;
    cout << "min-mem-per-frame: " << min_mem_per_proc << endl;
    cout << "max-mem-per-frame: " << max_mem_per_proc << endl;
    cout << "-------------------------\n";
}

/*void ConsoleManager::testMemoryAllocation() {
    cout << "\n[Memory Allocation Test]\n";

    // Example: Allocate a process of 512 bytes
    process_console* dummyProc = new process_console("dummy", 10);
    bool success = memManager.allocateMemory(dummyProc, 512);

    if (success) {
        cout << "Memory allocated successfully for process 'dummy' (512 bytes)\n";
    }
    else {
        cout << "Memory allocation failed for process 'dummy' (512 bytes)\n";
    }

    memManager.printFrameTable(); // Optional: print frame allocation table
}*/


void ConsoleManager::addConsole(const string& name, bool fromScreenCommand) {
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

    /* COMMENT OUR FOR NOW
    if (fromScreenCommand) {
        displayConsole(name);
    }
    */
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

/*void ConsoleManager::parseCommand(const std::string& input) {
    vector<string> tokens = tokenize(input);
    if (tokens.empty()) return;

    const string& cmd = tokens[0];

    if (cmd == "screen") {
        if (tokens.size() < 2) {
            cout << "Error: Missing mode (-s, -c, -r).\n";
            return;
        }

        string mode = tokens[1];

        if (mode == "-s") {
            handleScreenS(tokens);
        }
        else if (mode == "-c") {
            handleScreenC(tokens, input);
        }
        else if (mode == "-r") {
            handleScreenR(tokens);
        }
        else {
            cout << "Error: Unknown screen option.\n";
        }
    }
    else if (cmd == "process-smi") {
        handleProcessSMI();
    }
    else if (cmd == "vmstat") {
        handleVMStat();
    }
    else if (cmd == "scheduler-start") {
        handleSchedulerStart();
    }
    else {
        cout << "Error: Unknown command.\n";
    }
} */

void ConsoleManager::handleScreenS(const vector<string>& tokens) {
    if (tokens.size() < 4) {
        cout << "Usage: screen -s <process_name> <memory_size>\n";
        return;
    }

    string name = tokens[2];
    int memsize = stoi(tokens[3]);

    cout << "[Parsed] screen -s\n";
    cout << "  Name: " << name << "\n";
    cout << "  Memory: " << memsize << "\n";

    // TODO: call to process creation
}

void ConsoleManager::handleScreenC(const vector<string>& tokens, const std::string& rawInput) {
    if (tokens.size() < 4) {
        cout << "Usage: screen -c <name> <memory> \"<instructions>\"\n";
        return;
    }

    string name = tokens[2];
    int memsize = stoi(tokens[3]);

    // Extract instruction string between quotes
    size_t firstQuote = rawInput.find('"');
    size_t lastQuote = rawInput.rfind('"');
    string instructionStr = "";
    if (firstQuote != string::npos && lastQuote != string::npos && lastQuote > firstQuote) {
        instructionStr = rawInput.substr(firstQuote + 1, lastQuote - firstQuote - 1);
    }

    cout << "[Parsed] screen -c\n";
    cout << "  Name: " << name << "\n";
    cout << "  Memory: " << memsize << "\n";
    cout << "  Instructions: " << instructionStr << "\n";

    // Create process
    addConsole(name, false); // false = don't enter loop
    process_console* proc = consoles[name];
    if (!proc) {
        cout << "Failed to create process.\n";
        return;
    }

    // Attach memory
    if (!memManager.allocateMemory(proc, memsize)) {
        cout << "Failed to allocate memory for process \"" << name << "\".\n";
        return;
    }
    proc->memoryManagerPtr = &memManager;

    // Parse instructions separated by ;
    stringstream ss(instructionStr);
    string instrToken;

    while (getline(ss, instrToken, ';')) {
        if (instrToken.empty()) continue;

        // Trim leading/trailing whitespace
        instrToken.erase(0, instrToken.find_first_not_of(" \t\n\r"));
        instrToken.erase(instrToken.find_last_not_of(" \t\n\r") + 1);

        Instruction instr;

        if (instrToken.rfind("PRINT", 0) == 0) {
            instr.type = PRINT;

            // Extract everything inside PRINT(...)
            size_t start = instrToken.find("(");
            size_t end = instrToken.rfind(")");

            if (start == std::string::npos || end == std::string::npos || end <= start) {
                throw std::runtime_error("Syntax error: PRINT requires parentheses");
            }

            // Extract inside the parentheses
            std::string content = instrToken.substr(start + 1, end - start - 1);

            // Trim whitespace
            content.erase(0, content.find_first_not_of(" \t"));
            content.erase(content.find_last_not_of(" \t") + 1);

            instr.message = content;
        }
        else if (instrToken.find("DECLARE") == 0) {
            instr.type = DECLARE;
            char varName[50] = {};
            int value = 0;
            if (sscanf_s(instrToken.c_str(), "DECLARE %49s %d",
                varName, (unsigned)_countof(varName), &value) == 2) {
                instr.var1 = varName;
                instr.value1 = static_cast<uint16_t>(value);
            }
            else {
                cout << "Error parsing DECLARE: " << instrToken << "\n";
                continue;
            }
        }
        else if (instrToken.find("ADD") == 0) {
            instr.type = ADD;
            char v1[50], v2[50], v3[50];
            int scanned = sscanf_s(instrToken.c_str(), "ADD %49s %49s %49s",
                v1, (unsigned)_countof(v1),
                v2, (unsigned)_countof(v2),
                v3, (unsigned)_countof(v3));
            if (scanned != 3) { cout << "Error parsing ADD: " << instrToken << "\n"; continue; }
            instr.var1 = v1; instr.var2 = v2; instr.var3 = v3;
        }
        else if (instrToken.find("SUBTRACT") == 0) {
            instr.type = SUBTRACT;
            char v1[50], v2[50], v3[50];
            int scanned = sscanf_s(instrToken.c_str(), "SUBTRACT %49s %49s %49s",
                v1, (unsigned)_countof(v1),
                v2, (unsigned)_countof(v2),
                v3, (unsigned)_countof(v3));
            if (scanned != 3) { cout << "Error parsing SUBTRACT: " << instrToken << "\n"; continue; }
            instr.var1 = v1; instr.var2 = v2; instr.var3 = v3;
        }
        else if (instrToken.find("SLEEP") == 0) {
            instr.type = SLEEP;
            int val;
            if (sscanf_s(instrToken.c_str(), "SLEEP %d", &val) != 1) {
                cout << "Error parsing SLEEP: " << instrToken << "\n"; continue;
            }
            instr.value1 = val;
        }
        else if (instrToken.find("FOR_LOOP") == 0) {
            instr.type = FOR_LOOP;
            int repeats;
            char innerInstr[200];
            int scanned = sscanf_s(instrToken.c_str(), "FOR_LOOP(%d) %[^\n]", &repeats, innerInstr, (unsigned)_countof(innerInstr));
            if (scanned < 1) { cout << "Error parsing FOR_LOOP: " << instrToken << "\n"; continue; }
            instr.repeats = repeats;

            // Split inner instructions by ; if any
            string innerStr = innerInstr;
            stringstream innerSS(innerStr);
            string innerToken;
            while (getline(innerSS, innerToken, ';')) {
                innerToken.erase(0, innerToken.find_first_not_of(" \t\n\r"));
                innerToken.erase(innerToken.find_last_not_of(" \t\n\r") + 1);

                if (innerToken.empty()) continue;

                Instruction subInstr;
                if (innerToken.find("PRINT") == 0) {
                    subInstr.type = PRINT;
                    subInstr.message = innerToken.substr(5);
                    subInstr.message.erase(0, subInstr.message.find_first_not_of(" \t"));
                }
                // You can add more inner instruction parsing here if needed
                instr.subInstructions.push_back(subInstr);
            }
        }
        else if (instrToken.find("READ") == 0) {
            instr.type = READ;
            char v[50];
            int addr;
            if (sscanf_s(instrToken.c_str(), "READ %49s %d", v, (unsigned)_countof(v), &addr) != 2) {
                cout << "Error parsing READ: " << instrToken << "\n"; continue;
            }
            instr.var1 = v;
            instr.memAddress = addr;
        }
        else if (instrToken.find("WRITE") == 0) {
            instr.type = WRITE;
            char v[50];
            int addr;
            if (sscanf_s(instrToken.c_str(), "WRITE %49s %d", v, (unsigned)_countof(v), &addr) != 2) {
                cout << "Error parsing WRITE: " << instrToken << "\n"; continue;
            }
            instr.var1 = v;
            instr.memAddress = addr;
        }
        else {
            cout << "Unknown instruction: " << instrToken << "\n";
            continue;
        }

        proc->instructions.push_back(instr);
    }

    cout << "Process \"" << name << "\" created successfully with "
        << proc->instructions.size() << " instructions.\n";
}

void ConsoleManager::handleScreenR(const vector<string>& tokens) {
    if (tokens.size() < 3) {
        cout << "Usage: screen -r <process_name>\n";
        return;
    }

    string name = tokens[2];

    cout << "[Parsed] screen -r\n";
    cout << "  Requesting status of: " << name << "\n";

    // Lookup process
    auto it = consoles.find(name);
    if (it == consoles.end()) {
        cout << "Process " << name << " not found.\n";
        return;
    }

    process_console* proc = it->second;

    if (!proc || proc->instructions.empty()) {
        cout << "Process " << name << " has no instructions.\n";
        return;
    }

    // Symbol table for variables
    map<string, uint16_t> vars;

    for (const auto& instr : proc->instructions) {
        switch (instr.type) {
        case DECLARE:
            if (vars.size() >= 32) {
                cout << "Symbol table full, ignoring variable: " << instr.var1 << "\n";
                break;
            }
            vars[instr.var1] = instr.value1;
            break;

        case ADD:
            if (vars.count(instr.var2) && vars.count(instr.var3))
                vars[instr.var1] = vars[instr.var2] + vars[instr.var3];
            else
                cout << "ADD error: variable not found\n";
            break;

        case SUBTRACT:
            if (vars.count(instr.var2) && vars.count(instr.var3))
                vars[instr.var1] = vars[instr.var2] - vars[instr.var3];
            else
                cout << "SUBTRACT error: variable not found\n";
            break;

        case PRINT: {
            string output = instr.message;

            // Replace variables in the message if " + varName" exists
            size_t pos = 0;
            while ((pos = output.find(" + ")) != string::npos) {
                string left = output.substr(0, pos);
                string right = output.substr(pos + 3);

                // Trim quotes from right if present
                right.erase(remove(right.begin(), right.end(), '"'), right.end());
                if (vars.count(right))
                    output = left + to_string(vars[right]);
                else
                    output = left + right;
            }

            // Remove quotes around message if present
            if (!output.empty() && output.front() == '"') output.erase(0, 1);
            if (!output.empty() && output.back() == '"') output.pop_back();

            cout << output << "\n";
            break;
        }

        case READ: {
            uint16_t value = 0;
            // Use MemoryManager's readUint16
            if (!proc->memoryManagerPtr->readUint16(proc, instr.memAddress, value)) {
                std::cout << "Process " << name
                    << " shut down due to memory access violation at 0x"
                    << std::hex << instr.memAddress << std::dec << "\n";
                return;
            }
            vars[instr.var1] = value;
            break;
        }

        case WRITE: {
            uint16_t value = 0;
            // Take the variable's value if it exists
            if (vars.count(instr.var1))
                value = vars[instr.var1];

            // Use MemoryManager's writeUint16
            if (!proc->memoryManagerPtr->writeUint16(proc, instr.memAddress, value)) {
                std::cout << "Process " << name
                    << " shut down due to memory access violation at 0x"
                    << std::hex << instr.memAddress << std::dec << "\n";
                return;
            }
            break;
        }
        case SLEEP:
            // std::this_thread::sleep_for?
            break;

        case FOR_LOOP:
            for (int i = 0; i < instr.repeats; i++) {
                for (const auto& subInstr : instr.subInstructions) {
                    // Recursive execution for sub-instructions
                    // You can call a helper function here if desired
                }
            }
            break;

        default:
            cout << "Unknown instruction type\n";
            break;
        }
    }
}

void ConsoleManager::handleProcessSMI() {
    cout << "[Parsed] process-smi\n";
}

void ConsoleManager::handleVMStat() {
    cout << "[Parsed] vmstat\n";
}

void ConsoleManager::handleSchedulerStart() {
    cout << "[Parsed] scheduler-start\n";
    // TODO: start actual scheduler thread
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

        for (size_t i = 0; i < cpuCores.size(); ++i) {
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

        for (size_t i = 0; i < cpuCores.size(); ++i) {
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


