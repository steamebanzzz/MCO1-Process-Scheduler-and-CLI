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
unsigned long long cpuActiveTicks = 0;
unsigned long long cpuIdleTicks = 0;
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

    memManager = MemoryManager(max_overall_mem, mem_per_frame);
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

    // --- Allocate memory for the process ---
    uniform_int_distribution<> memDist(min_mem_per_proc, max_mem_per_proc);
    int memRequired = memDist(knuth_gen);

    bool success = memManager.allocateMemory(newConsole, memRequired);
    if (!success) {
        cout << "Memory allocation failed for process " << name << endl;
        delete newConsole;   // cleanup
        return;
    }

    waitingQueue.push(newConsole);
    consoles[name] = newConsole;

    /* COMMENT OUT FOR NOW
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

    // Validate memory size
    if ((memsize & (memsize - 1)) != 0) {  // not a power of 2
        cout << "Invalid memory allocation: must be a power of 2.\n";
        return;
    }

    if (memsize < 64 || memsize > 65536) {  // bounds [2^6, 2^16]
        cout << "Invalid memory allocation: must be between 64 and 65536 bytes.\n";
        return;
    }

    // Check if process already exists
    if (consoleExists(name)) {
        cout << "Process with name \"" << name << "\" already exists.\n";
        return;
    }

    // Create process with default instruction count
    int instructionCount = 10;  // or any heuristic
    process_console* newProc = new process_console(name, instructionCount);

    // Attach memory manager
    newProc->memoryManagerPtr = &memManager;

    // Allocate memory frames
    if (!memManager.allocateMemory(newProc, memsize)) {
        cout << "Failed to allocate memory for process \"" << name << "\".\n";
        delete newProc;
        return;
    }

    // Add process to consoles
    consoles[name] = newProc;

    cout << "Process \"" << name << "\" created successfully with " << memsize << " bytes.\n";

    // Optionally: generate random instructions
    generateRandomInstructions(newProc, instructionCount);
}

void ConsoleManager::handleScreenC(const vector<string>& tokens, const string& rawInput) {
    if (tokens.size() < 4) {
        cout << "Usage: screen -c <process_name> <memory_size> \"<instructions>\"\n";
        return;
    }

    string name = tokens[2];
    int memsize_words = stoi(tokens[3]); // memory size in words
    if (memsize_words <= 0) {
        cout << "Invalid memory size\n";
        return;
    }

    // Convert words to bytes for MemoryManager
    int memsize_bytes = memsize_words * 2;

    // Extract instruction string from raw input
    size_t instrPos = rawInput.find(tokens[3]);
    string instructionStr = rawInput.substr(instrPos + tokens[3].length());
    instructionStr.erase(0, instructionStr.find_first_not_of(" \""));
    instructionStr.erase(instructionStr.find_last_not_of("\"") + 1);

    cout << "[Parsed] screen -c\n";
    cout << "  Name: " << name << "\n";
    cout << "  Memory: " << memsize_words << " words (" << memsize_bytes << " bytes)\n";
    cout << "  Instructions: " << instructionStr << "\n";

    // Parse instructions
    vector<Instruction> procInstructions;
    istringstream ss(instructionStr);
    string instrLine;
    while (getline(ss, instrLine, ';')) {
        if (instrLine.empty()) continue;

        instrLine.erase(0, instrLine.find_first_not_of(" "));
        instrLine.erase(instrLine.find_last_not_of(" ") + 1);

        Instruction instr;
        string cmd;
        size_t parenPos = instrLine.find('(');
        size_t spacePos = instrLine.find(' ');

        if (parenPos != string::npos)
            cmd = instrLine.substr(0, parenPos);
        else if (spacePos != string::npos)
            cmd = instrLine.substr(0, spacePos);
        else
            cmd = instrLine;

        for (auto& c : cmd) c = toupper(c);

        if (cmd == "DECLARE") {
            instr.type = DECLARE;
            istringstream s(instrLine.substr(spacePos + 1));
            string var;
            int val;
            s >> var >> val;
            instr.var1 = var;
            instr.value1 = static_cast<uint16_t>(val);
        }
        else if (cmd == "PRINT") {
            instr.type = PRINT;
            size_t start = instrLine.find('(');
            size_t end = instrLine.rfind(')');
            if (start != string::npos && end != string::npos && end > start + 1) {
                string rawMsg = instrLine.substr(start + 1, end - start - 1);
                rawMsg.erase(0, rawMsg.find_first_not_of(" "));
                rawMsg.erase(rawMsg.find_last_not_of(" ") + 1);
                instr.message = rawMsg;
            }
        }
        else if (cmd == "ADD" || cmd == "SUBTRACT" || cmd == "READ" || cmd == "WRITE" || cmd == "SLEEP") {
            // Existing parsing logic for these instructions...
        }
        else {
            cout << "Unknown instruction: " << instrLine << "\n";
            continue;
        }

        procInstructions.push_back(instr);
    }

    // Create the process
    addConsole(name);
    process_console* proc = consoles[name];
    proc->memoryManagerPtr = &memManager;

    // --- FIXED PART: allocate memory in bytes, not words ---
    if (!memManager.allocateMemory(proc, memsize_bytes)) {
        cout << "Memory allocation failed for process " << name << ".\n";
        return;
    }

    // Assign instructions
    proc->instructions = procInstructions;
    proc->setInstructionTotal(procInstructions.size());

    cout << "Loaded " << procInstructions.size() << " instructions for process " << name << ".\n";
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

    // If process has memory violation
    if (proc->getHasViolation()) {
        std::cout << "Process " << name
            << " shutdown due to memory access violation error that occured at "
            << proc->getViolationTime() << ". 0x"
            << std::hex << std::uppercase << setfill('0') << std::setw(8)
            << proc->getViolationAddress() << std::dec
            << " invalid." << "\n";
        return;
    }

    // If process has already terminated
    if (proc->getStatus() == process_console::TERMINATED) {
        cout << "Process " << name << " not found. \n";
        return;
    }

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
            string output;
            string expr = instr.message; 

            istringstream ss(expr);
            string segment;
            while (getline(ss, segment, '+')) {
 
                segment.erase(0, segment.find_first_not_of(" \""));
                segment.erase(segment.find_last_not_of(" \"") + 1);

                if (vars.count(segment))
                    output += to_string(vars[segment]);
                else
                    output += segment;
            }

            proc->logs.push_back(output);
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
    std::lock_guard<std::mutex> lock(processMutex);

    std::cout << "\n============= process-smi =============\n";

    // CPU / scheduler summary
    std::cout << "CPU cores       : " << coreCount << "\n";
    std::cout << "Available cores : " << availableCores << "\n";
    std::cout << "Scheduler       : " << scheduler << "\n";
    std::cout << "Quantum-cycles  : " << quantum_cycles << "\n";

    // Memory config
    int totalMem = memManager.getTotalMemory();
    int frameSize = memManager.getMemPerFrame();
    int totalFrames = memManager.getFrameCount();

    std::cout << "\nMemory configuration:\n";
    std::cout << "  Total memory : " << totalMem << " bytes\n";
    std::cout << "  Mem per frame: " << frameSize << " bytes\n";
    std::cout << "  Frames total : " << totalFrames << "\n";

    if (!hasConsoles()) {
        std::cout << "\nNo processes.\n";
        std::cout << "=======================================\n";
        return;
    }

    std::cout << "\nProcesses:\n";
    std::cout << "NAME\tPID\tSTATE\tCORE\tPC/INS\tMEMORY\n";

    for (auto& entry : consoles) {
        process_console* proc = entry.second;
        if (!proc) continue;

        std::string stateStr;
        switch (proc->getStatus()) {
        case process_console::RUNNING:    stateStr = "RUNNING";   break;
        case process_console::WAITING:    stateStr = "WAITING";   break;
        case process_console::TERMINATED: stateStr = "FINISHED";  break;
        default:                          stateStr = "UNKNOWN";   break;
        }

        // Memory used by this process
        std::vector<int> framesForProc = memManager.getFramesForProcess(proc->getProcessID());
        int memUsedBytes = 0;
        int usedFrames = 0;
        for (int f : framesForProc) {
            if (f >= 0) {
                memUsedBytes += frameSize;
                usedFrames++;
            }
        }

        std::cout << proc->getName()
            << "\t" << proc->getProcessID()
            << "\t" << stateStr
            << "\t" << proc->getCoreID()
            << "\t" << proc->getInstructionLine()
            << "/" << proc->getInstructionTotal()
            << "\t" << memUsedBytes << " B ("
            << usedFrames << " frames)\n";
    }

    std::cout << "=======================================\n";
}

void ConsoleManager::handleVMStat() {
    int totalMem = memManager.getTotalMemory();
    int usedMem = memManager.getUsedMemory();
    int freeMem = totalMem - usedMem;

    int totalFrames = memManager.getFrameCount();
    int usedFrames = memManager.getUsedFrameCount();
    int freeFrames = totalFrames - usedFrames;

    std::cout << "\n========== vmstat ==========\n";
    std::cout << "Memory (bytes)\n";
    std::cout << "  total : " << totalMem << "\n";
    std::cout << "  used  : " << usedMem << "\n";
    std::cout << "  free  : " << freeMem << "\n\n";

    std::cout << "Frames\n";
    std::cout << "  frame size   : " << memManager.getMemPerFrame() << "\n";
    std::cout << "  total frames : " << totalFrames << "\n";
    std::cout << "  used frames  : " << usedFrames << "\n";
    std::cout << "  free frames  : " << freeFrames << "\n\n";

    std::cout << "CPU ticks\n";
    std::cout << "  active : " << activeCpuTicks << "\n";
    std::cout << "  idle   : " << idleCpuTicks << "\n";
    std::cout << "  total  : " << (activeCpuTicks + idleCpuTicks) << "\n\n";

    std::cout << "Paging\n";
    std::cout << "  paged in  : " << memManager.getPagedInCount() << "\n";
    std::cout << "  paged out : " << memManager.getPagedOutCount() << "\n";
    std::cout << "============================\n";
}

void ConsoleManager::handleSchedulerStart() {
    cout << "[Parsed] scheduler-start\n";

    if (!schedulerRunning) {
        startScheduler();
        schedulerRunning = true;
    }

    schedulerPaused = false;
    schedulerCV.notify_one();
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

        // vmstat tick count
        int activeCores = 0;
        for (bool used : cpuCores) {
            if (used) ++activeCores;
        }

        activeCpuTicks += activeCores;
        idleCpuTicks += (coreCount - activeCores);
        totalCpuTicks += coreCount;
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

        // vmstat tick count
        int activeCores = 0;
        for (bool used : cpuCores) {
            if (used) ++activeCores;
        }

        activeCpuTicks += activeCores;
        idleCpuTicks += (coreCount - activeCores);
        totalCpuTicks += coreCount;
    }
}
