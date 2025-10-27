#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <algorithm>
#include <random>
#include <chrono>
#include <iomanip>
#include <thread>
#include <atomic>
#include <mutex>
#include <ctime>
#include <deque>
// #include <sec_api/time_s.h>

struct Config {
    int num_cpu = 1;
    std::string scheduler = "fcfs";
    int quantum_cycles = 1;
    int batch_process_freq = 0;
    int min_ins = 3;
    int max_ins = 8;
    int delay_per_exec = 0;
};

enum class InstructionType {
    PRINT, DECLARE, ADD, SUBTRACT, SLEEP, FOR_START, FOR_END
};

struct Instruction {
    InstructionType type;
    std::string var1, var2, var3;
    std::uint16_t value1 = 0, value2 = 0;
    std::string message;
    int sleep_ticks = 0;
    int for_repeats = 0;
    std::vector<Instruction> for_instructions;
};

static inline std::string trim(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

static bool parse_config_file(const std::string& filename, Config& cfg, std::string& error) {
    std::ifstream file(filename);
    if (!file.is_open()) { error = "Cannot open configuration file: " + filename; return false; }

    std::map<std::string, std::string> kv;
    std::string line; int line_num = 0;

    while (std::getline(file, line)) {
        ++line_num;
        std::string s = trim(line);
        if (s.empty() || s[0] == '#') continue;
        std::istringstream iss(s);
        std::string key, value;
        if (!(iss >> key >> value)) {
            error = "Invalid line " + std::to_string(line_num) + ": " + line;
            return false;
        }
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        if (!value.empty() && value.front() == '"' && value.back() == '"')
            value = value.substr(1, value.size() - 2);
        kv[key] = value;
    }

    try {
        if (kv.count("num-cpu")) cfg.num_cpu = std::stoi(kv["num-cpu"]);
        if (kv.count("scheduler")) cfg.scheduler = kv["scheduler"];
        if (kv.count("quantum-cycles")) cfg.quantum_cycles = std::stoi(kv["quantum-cycles"]);
        if (kv.count("batch-process-freq")) cfg.batch_process_freq = std::stoi(kv["batch-process-freq"]);
        if (kv.count("min-ins")) cfg.min_ins = std::stoi(kv["min-ins"]);
        if (kv.count("max-ins")) cfg.max_ins = std::stoi(kv["max-ins"]);
        if (kv.count("delay-per-exec")) cfg.delay_per_exec = std::stoi(kv["delay-per-exec"]);
    }
    catch (...) {
        error = "Error parsing numeric values in config.";
        return false;
    }

    if (cfg.num_cpu <= 0) { error = "num-cpu must be > 0"; return false; }
    if (cfg.min_ins < 0 || cfg.max_ins < cfg.min_ins) { error = "min-ins/max-ins invalid"; return false; }
    return true;
}

// ========================
// PROCESS
// ========================
struct Process {
    int id = 0;
    std::string name;
    bool finished = false;
    std::vector<Instruction> instructions;
    int current_instruction = 0;
    int total_instructions = 0;
    int executed = 0;
    enum State { READY, RUNNING, FINISHED, SLEEPING } state = READY;
    int priority = 0;   // this is for priority scheduling (not implemented)
    std::vector<std::string> logs;
    std::vector<std::string> ins_types = { "LOAD", "STORE", "ADD", "SUB", "MUL", "DIV", "JMP", "CMP" };

    std::map<std::string, std::uint16_t> variables;
    int sleep_remaining = 0;
    int delay_remaining = 0;

    struct LoopContext{
        int start;
        int repeats_itself;
        std::vector<Instruction> instructions;
        int index = 0;
    };
    std::vector<LoopContext> loop_stack;

    void print_smi_unsafe() const {
        std::cout << "Process name: " << name << "\n";
        std::cout << "ID: " << id << "\n";
        std::cout << "Logs:\n";
        for (const auto& l : logs) std::cout << "  " << l << "\n";
        if (finished) std::cout << "Finished!\n";
        else {
            std::cout << "Current instruction line: " << current_instruction << "\n";
            std::cout << "Lines of code: " << instructions.size() << "\n";
        }
    }

    std::uint16_t get_variable(const std::string& name) {
        if (variables.find(name) == variables.end()) {
            variables[name] = 0;
        }
        return variables[name];
    }

    void set_variable(const std::string& name, std::uint16_t value) {
        variables[name] = value;
    }

    std::uint16_t parse_operand(const std::string& operand) {
        if (std::isdigit(operand[0])) {
            return static_cast<std::uint16_t>(std::stoi(operand));
        }
        return get_variable(operand);
    }
};

// ========================
// EMULATOR
// ========================
class OSEmulator {
public:
    OSEmulator()
        : rng(static_cast<unsigned int>(std::chrono::high_resolution_clock::now().time_since_epoch().count())) {
    }

    ~OSEmulator() { stop_scheduler(); }

    std::deque<std::list<Process>::iterator> ready_queue;
    std::vector<std::list<Process>::iterator> running;  // one per core
    std::vector<int> quantum_left;                      // per-core quantum countdown

    void run() {
        std::cout << "CSOPESY OS Emulator (Phases 1–5)\nType 'help' for commands.\n";
        std::string line;
        while (true) {
            std::cout << "> ";
            if (!std::getline(std::cin, line)) break;
            if (line.empty()) continue;

            std::istringstream iss(line);
            std::string cmd; iss >> cmd;
            std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

            if (cmd == "exit") { stop_scheduler(); std::cout << "Exiting emulator...\n"; break; }
            if (cmd == "help") { print_help(); continue; }

            if (cmd == "initialize") { handle_initialize(iss); continue; }
            if (!initialized) { std::cout << "System not initialized. Run 'initialize' first.\n"; continue; }

            if (cmd == "screen") { handle_screen(iss); continue; }
            if (cmd == "scheduler-start") { start_scheduler(); continue; }
            if (cmd == "scheduler-stop") { stop_scheduler(); continue; }
            if (cmd == "tick") { handle_tick(iss); continue; }
            if (cmd == "report-util") { report_util(); continue; }

            std::cout << "Unknown command. Type 'help' for list.\n";
        }
    }

private:
    Config cfg;
    bool initialized = false;

    std::atomic<bool> scheduler_running{ false };
    std::thread scheduler_thread;

    std::list<Process> processes;
    int next_pid = 1;
    long long tick_count = 0;
    int batch_tick_counter = 0;
    int auto_process_counter = 1;

    std::recursive_mutex proc_mutex;
    std::mutex cout_mutex;
    std::mt19937 rng;

    // ============= HELPERS =============
    std::string timestamp() {
        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
    #if defined(_MSC_VER)
        localtime_s(&tm, &tt);
    #else
        std::tm* tmptr = std::localtime(&tt);
        if (tmptr) tm = *tmptr;
    #endif
        char buf[64];
        std::strftime(buf, sizeof(buf), "(%m/%d/%Y %I:%M:%S%p)", &tm);
        return std::string(buf);
    }

    void safe_cout(const std::string& s) {
        std::lock_guard<std::mutex> lk(cout_mutex);
        std::cout << s << std::flush;
    }

    // ============= COMMANDS =============
    void print_help() {
        std::cout << (initialized
            ? "Commands:\n  screen -s <name>\n  screen -ls\n  screen -r <name>\n  scheduler-start\n  scheduler-stop\n  tick <n>\n  report-util\n  exit\n"
            : "Commands before initialization:\n  initialize [file]\n  exit\n");
    }

    void handle_initialize(std::istringstream& iss) {
        std::string fname;
        if (!(iss >> fname)) fname = "config.txt";
        std::string err;
        if (!parse_config_file(fname, cfg, err)) {
            std::cout << "Error: " << err << "\n";
            return;
        }
        initialized = true;
        // Initialize running and quantum_left vectors based on num_cpu
        running = std::vector<std::list<Process>::iterator>(cfg.num_cpu, processes.end());
        quantum_left = std::vector<int>(cfg.num_cpu, 0);
        std::cout << "Initialized successfully from " << fname << "\n";
        std::cout << "num-cpu: " << cfg.num_cpu << " | scheduler: " << cfg.scheduler << "\n";
    }

    // ===== SCREEN COMMANDS =====
    void handle_screen(std::istringstream& iss) {
        std::string opt; iss >> opt;
        if (opt == "-s") { create_process_cmd(iss); return; }
        if (opt == "-ls") { list_processes(); return; }
        if (opt == "-r") { reattach_process_cmd(iss); return; }
        std::cout << "Invalid usage. Try: screen -s <name>, -ls, or -r <name>\n";
    }

    void create_process_cmd(std::istringstream& iss) {
        std::string name; iss >> name;
        if (name.empty()) { std::cout << "Usage: screen -s <name>\n"; return; }

        {
            std::lock_guard<std::recursive_mutex> lock(proc_mutex);
            if (find_process_by_name(name) != processes.end()) { std::cout << "Process name already exists.\n"; return; }
            add_process_locked(name);
        }

        std::cout << "Created process '" << name << "' successfully.\n";
        enter_process_screen_by_name(name);
    }

    void list_processes() {
        std::lock_guard<std::recursive_mutex> lock(proc_mutex);
        if (processes.empty()) { std::cout << "No processes available.\n"; return; }
        
        int cores_used = 0;
        for (const auto& core : running)
            if (core != processes.end()) cores_used++;
        
        std::cout << "CPU utilization: " << std::fixed << std::setprecision(2) 
                  << (100.0 * cores_used / cfg.num_cpu) << "%\n";
        std::cout << "Cores used: " << cores_used << "\n";
        std::cout << "Cores available: " << (cfg.num_cpu - cores_used) << "\n";
        std::cout << "\nRunning processes:\n";
        
        for (const auto& core : running) {
            if (core != processes.end()) {
                std::cout << core->name << "\t" << core->current_instruction 
                         << "/" << core->instructions.size() << "\t" 
                         << timestamp() << "\n";
            }
        }
        
        std::cout << "\nFinished processes:\n";
        for (const auto& p : processes) {
            if (p.finished) {
                std::cout << p.name << "\t" << p.instructions.size() 
                         << "/" << p.instructions.size() << "\t" 
                         << timestamp() << "\n";
            }
        }
    }

    void reattach_process_cmd(std::istringstream& iss) {
        std::string name; iss >> name;
        if (name.empty()) { std::cout << "Usage: screen -r <name>\n"; return; }
        std::lock_guard<std::recursive_mutex> lock(proc_mutex);
        auto it = find_process_by_name(name);
        if (it == processes.end()) { std::cout << "Process not found.\n"; return; }
        if (it->finished) { std::cout << "Cannot reattach — process has finished.\n"; return; }
        int pid = it->id;
        enter_process_screen(pid);
    }

    // INSTRUCTION EXECUTION
    std::vector<Instruction> generate_instructions(const std::string& name, int count) {
        std::vector<Instruction> instructions;
        std::uniform_int_distribution<int> type_dist(0, 6);
        std::uniform_int_distribution<int> value_dist(1, 100);
        std::uniform_int_distribution<int> sleep_dist(1, 10);
        std::uniform_int_distribution<int> for_dist(2, 5);

        // Always start with a PRINT instruction
        Instruction print_inst;
        print_inst.type = InstructionType::PRINT;
        print_inst.message = "Hello world from " + name + "!";
        instructions.push_back(print_inst);

        for (int i = 1; i < count; ++i) {
            Instruction inst;
            int type = type_dist(rng);
            
            switch (type) {
                case 0: // PRINT
                    inst.type = InstructionType::PRINT;
                    inst.message = "Hello world from " + name + "!";
                    break;
                case 1: // DECLARE
                    inst.type = InstructionType::DECLARE;
                    inst.var1 = "var" + std::to_string(i);
                    inst.value1 = value_dist(rng);
                    break;
                case 2: // ADD
                    inst.type = InstructionType::ADD;
                    inst.var1 = "result" + std::to_string(i);
                    inst.var2 = "var" + std::to_string(std::max(1, i-1));
                    inst.value2 = value_dist(rng);
                    break;
                case 3: // SUBTRACT
                    inst.type = InstructionType::SUBTRACT;
                    inst.var1 = "result" + std::to_string(i);
                    inst.var2 = "var" + std::to_string(std::max(1, i-1));
                    inst.value2 = value_dist(rng);
                    break;
                case 4: // SLEEP
                    inst.type = InstructionType::SLEEP;
                    inst.sleep_ticks = sleep_dist(rng);
                    break;
                default: // Default to PRINT
                    inst.type = InstructionType::PRINT;
                    inst.message = "Hello world from " + name + "!";
                    break;
            }
            instructions.push_back(inst);
        }
        return instructions;
    }

    // ===== PROCESS HANDLING =====
    void add_process_locked(const std::string& name) {
        std::uniform_int_distribution<int> ins(cfg.min_ins, cfg.max_ins);
        Process p;
        p.id = next_pid++;
        p.name = name;
        p.instructions = generate_instructions(name, ins(rng));
        p.current_instruction = 0;
        p.executed = 0;
        p.total_instructions = p.instructions.size();
        p.logs.push_back(timestamp() + " Process created");
        processes.push_back(std::move(p));

        auto it = processes.end();
        --it;
        ready_queue.push_back(it);
    }

    auto find_process_by_name(const std::string& name) -> std::list<Process>::iterator {
        return std::find_if(processes.begin(), processes.end(),
            [&](const Process& p) { return p.name == name; });
    }

    auto find_process_by_id(int pid) -> std::list<Process>::iterator {
        return std::find_if(processes.begin(), processes.end(),
            [&](const Process& p) { return p.id == pid; });
    }

    void enter_process_screen_by_name(const std::string& name) {
        std::lock_guard<std::recursive_mutex> lock(proc_mutex);
        auto it = find_process_by_name(name);
        if (it == processes.end()) { std::cout << "Process not found.\n"; return; }
        enter_process_screen(it->id);
    }

    void enter_process_screen(int pid) {
        std::string line;
        while (true) {
            std::cout << "[" << pid << "]> ";
            if (!std::getline(std::cin, line)) break;
            if (line.empty()) continue;

            std::istringstream iss(line);
            std::string cmd;
            iss >> cmd;
            std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

            if (cmd == "exit") {
                std::cout << "Returned to main menu.\n";
                return;
            }
            else if (cmd == "process-smi") {
                Process snapshot;
                {
                    std::lock_guard<std::recursive_mutex> lock(proc_mutex);
                    auto it = find_process_by_id(pid);
                    if (it == processes.end()) {
                        std::cout << "Process not found.\n";
                        continue;
                    }
                    snapshot = *it;
                }

                snapshot.print_smi_unsafe();

                if (snapshot.finished) {
                    std::cout << "Finished!\n";
                    return;
                }
            }
            else {
                std::cout << "Unknown process command. Type 'process-smi' or 'exit'.\n";
            }
        }
    }

    // ===== SCHEDULER =====
    void start_scheduler() {
        bool expected = false;
        if (!scheduler_running.compare_exchange_strong(expected, true)) {
            std::cout << "Scheduler already running.\n"; return;
        }
        safe_cout("Scheduler started (auto-ticking enabled).\n");
        scheduler_thread = std::thread([this]() {
            const std::chrono::milliseconds tick_delay(500);
            while (scheduler_running.load()) {
                std::this_thread::sleep_for(tick_delay);
                simulate_tick();
            }
            });
    }

    void stop_scheduler() {
        bool expected = true;
        if (!scheduler_running.compare_exchange_strong(expected, false)) return;
        if (scheduler_thread.joinable()) scheduler_thread.join();
        safe_cout("Scheduler stopped.\n");
    }

    void handle_tick(std::istringstream& iss) {
        int n = 1; iss >> n;
        if (n <= 0) n = 1;
        for (int i = 0; i < n; ++i) simulate_tick();
        std::ostringstream os; os << "Advanced " << n << " ticks (total=" << tick_count << ").\n";
        safe_cout(os.str());
    }

    void execute_instruction(Process& p, int core_id) {
        if (p.current_instruction >= p.instructions.size()){
            p.finished = true;
            p.state = Process::FINISHED;
            p.logs.push_back(timestamp() + " Process finished execution.");
            return;
        } 

        const Instruction& inst = p.instructions[p.current_instruction];

        switch (inst.type) {
            case InstructionType::PRINT:
                p.logs.push_back(timestamp() + " PRINT: " + inst.message);
                break;
            case InstructionType::DECLARE:
                p.set_variable(inst.var1, inst.value1);
                p.logs.push_back(timestamp() + " DECLARE: " + inst.var1 + " = " + std::to_string(inst.value1));
                break;
            case InstructionType::ADD: {
                std::uint16_t val2 = p.parse_operand(inst.var2);
                std::uint16_t result = val2 + inst.value2;
                p.set_variable(inst.var1, result);
                p.logs.push_back(timestamp() + " ADD: " + inst.var1 + " = " + std::to_string(result));
                break;
            }
            case InstructionType::SUBTRACT: {
                std::uint16_t val2 = p.parse_operand(inst.var2);
                std::uint16_t result = val2 - inst.value2;
                p.set_variable(inst.var1, result);
                p.logs.push_back(timestamp() + " SUBTRACT: " + inst.var1 + " = " + std::to_string(result));
                break;
            }
            case InstructionType::SLEEP:
                p.state = Process::SLEEPING;
                p.sleep_remaining = inst.sleep_ticks;
                p.logs.push_back(timestamp() + " SLEEP: for " + std::to_string(inst.sleep_ticks) + " ticks");
                break;
            default:
                p.logs.push_back(timestamp() + " Unknown instruction type.");
                break;
        }

        p.current_instruction++;
        p.executed++;
    }

    void simulate_tick() {
        std::lock_guard<std::recursive_mutex> lock(proc_mutex);
        tick_count++;
        batch_tick_counter++;

        // Handle Processes in SLEEPING state
        for (auto& p : processes) {
            if (p.state == Process::SLEEPING) {
                p.sleep_remaining--;
                if (p.sleep_remaining <= 0) {
                    p.state = Process::READY;
                    ready_queue.push_back(find_process_by_id(p.id));
                    p.logs.push_back(timestamp() + " Woke up from sleep");
                }
            }
        }

        // Assign new processes if core is idle
        for (size_t i = 0; i < running.size(); ++i) {
            if (running[i] == processes.end()) {
                if (!ready_queue.empty()) {
                    running[i] = ready_queue.front();
                    ready_queue.pop_front();
                    running[i]->state = Process::RUNNING;
                    quantum_left[i] = (cfg.scheduler == "rr") ? cfg.quantum_cycles : INT_MAX;
                    running[i]->logs.push_back(timestamp() + " Dispatched " + running[i]->name + " to core " + std::to_string(i));
                }
            }
        }

        // Execute one instruction per core
        for (size_t i = 0; i < running.size(); ++i) {
            if (running[i] != processes.end()) {
                Process& p = *running[i];
                p.executed++;
                quantum_left[i]--;

                p.logs.push_back(timestamp() + " Core " + std::to_string(i) + 
                                " executed 1 instr of " + p.name);

                // Check if process finished
                if (p.executed >= p.total_instructions) {
                    p.finished = true;
                    p.state = Process::FINISHED;
                    running[i] = processes.end();
                    p.logs.push_back(timestamp() + " " + p.name + 
                                    " finished on core " + std::to_string(i));
                }
                // Round Robin: quantum expired, then preempt
                else if (cfg.scheduler == "rr" && quantum_left[i] <= 0) {
                    p.logs.push_back(timestamp() + " " + p.name + 
                                    " preempted on core " + std::to_string(i));
                    p.state = Process::READY;
                    ready_queue.push_back(find_process_by_id(p.id));
                    running[i] = processes.end();
                }
            }
            // If core is now idle, assign next process
            if (running[i] == processes.end() && !ready_queue.empty()) {
                auto next_proc = ready_queue.front();
                ready_queue.pop_front();
                next_proc->state = Process::RUNNING;
                running[i] = next_proc;
                quantum_left[i] = (cfg.scheduler == "rr") ? cfg.quantum_cycles : INT_MAX;
                next_proc->logs.push_back(timestamp() + " Dispatched " + next_proc->name +
                                        " to core " + std::to_string(i));
            }
        }
        
        if (cfg.batch_process_freq > 0 && batch_tick_counter >= cfg.batch_process_freq) {
            batch_tick_counter = 0;
            add_process_locked("p" + std::to_string(auto_process_counter++));
        }
    }

    // ===== REPORT =====
    void report_util() {
        std::lock_guard<std::recursive_mutex> lock(proc_mutex);

        // counts how many cores are used
        int cores_used = 0;
        for (const auto& core : running)
            if (core != processes.end())
                cores_used++;

        std::ostringstream oss;
        oss << "===== CPU UTILIZATION REPORT =====\n";
        oss << "Timestamp: " << timestamp() << "\n";
        oss << "Total CPU ticks: " << tick_count << "\n";
        oss << "Cores: " << cfg.num_cpu << "\n";

        // List all processes with their status
        for (const auto& p : processes) {
            std::string state_str;
            if (p.finished) state_str = "finished";
            else if (p.state == Process::RUNNING) state_str = "running";
            else state_str = "ready";

            oss << "  " << p.name << " | ID: " << p.id 
                << " | " << state_str 
                << " | Progress: " << p.executed << "/" << p.total_instructions << "\n";
        }

        oss << "==================================\n\n";

        // Print to console
        std::cout << oss.str();

        // Append to log file
        std::ofstream log("csopesy-log.txt", std::ios::app);
        if (log.is_open()) {
            log << oss.str();
            log.close();
        } else {
            std::cerr << "Warning: Could not open csopesy-log.txt for writing.\n";
        }
    }
};

// ========================
// MAIN
// ========================
int main() {
    OSEmulator os;
    os.run();
    return 0;
}
