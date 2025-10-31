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
#include <iomanip>
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

    // P9
	long long created_tick = 0;
	long long terminated_tick = -1;

    std::map<std::string, std::uint16_t> memory_table;
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
        if (memory_table.find(name) == memory_table.end()) {
            memory_table[name] = 0;
        }
        return memory_table[name];
    }

    void set_variable(const std::string& name, std::uint16_t value) {
        memory_table[name] = value;
    }

    std::uint16_t parse_operand(const std::string& operand) {
        if (std::isdigit(operand[0])) {
            return static_cast<std::uint16_t>(std::stoi(operand));
        }
        return get_variable(operand);
    }

    size_t memory_bytes() const {
        return memory_table.size() * sizeof(std::uint16_t);
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

        // Resets
		auto_process_counter = 1;
		tick_count = 0;
		batch_tick_counter = 0;

        std::cout << "Initialized successfully from " << fname << "\n";
        std::cout << "num-cpu: " << cfg.num_cpu << " | scheduler: " << cfg.scheduler << "\n";
    }

    static std::uint16_t clamp_uint16(std::int64_t value) {
        if (value < 0) value = 0;
        else if (value > 65535) value = 65535;
        return static_cast<std::uint16_t>(value);
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
        if (processes.empty()) { 
            std::cout << "No processes available.\n"; 
            return; 
        }
        
        int cores_used = 0;
        for (const auto& core : running)
            if (core != processes.end()) cores_used++;
        
        std::cout << "CPU utilization: " << std::fixed << std::setprecision(0) 
                << (100.0 * cores_used / cfg.num_cpu) << "%\n";
        std::cout << "Cores used: " << cores_used << "\n";
        std::cout << "Cores available: " << (cfg.num_cpu - cores_used) << "\n";
        std::cout << "\n";
        std::cout << "--------------------------------------\n";
        std::cout << "Running processes:\n";
        
        // Show running processes with their core assignments
        for (size_t i = 0; i < running.size(); ++i) {
            if (running[i] != processes.end()) {
                const auto& p = *running[i];
                std::cout << p.name << "\t" << timestamp() 
                        << "\tCore: " << i << "\t\t"
                        << p.current_instruction << " / " << p.total_instructions << "\n";
            }
        }
        
        std::cout << "\nFinished processes:\n";
        for (const auto& p : processes) {
            if (p.finished) {
                std::cout << p.name << "\t" << timestamp() 
                        << "\tFinished\t\t" << p.total_instructions 
                        << " / " << p.total_instructions << "\n";
            }
        }
        std::cout << "--------------------------------------\n";
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
        print_inst.message = " Hello world from " + name + "!";
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
                case 5: // FOR LOOP
                case 6: {
                    inst.type = InstructionType::FOR_START;
                    inst.for_repeats = for_dist(rng);
                    
                    std::uniform_int_distribution<int> body_size(2, 4);
                    int body_count = body_size(rng);
                    
                    for (int j = 0; j < body_count; ++j) {
                        Instruction body_inst;
                        int body_type = type_dist(rng) % 4; 
                        
                        switch (body_type) {
                            case 0:
                                body_inst.type = InstructionType::PRINT;
                                body_inst.message = "Loop iteration from " + name;
                                break;
                            case 1:
                                body_inst.type = InstructionType::DECLARE;
                                body_inst.var1 = "loop_var" + std::to_string(i) + "_" + std::to_string(j);
                                body_inst.value1 = value_dist(rng);
                                break;
                            case 2:
                                body_inst.type = InstructionType::ADD;
                                body_inst.var1 = "loop_result" + std::to_string(i) + "_" + std::to_string(j);
                                body_inst.var2 = "var" + std::to_string(std::max(1, i-1));
                                body_inst.value2 = value_dist(rng);
                                break;
                            case 3:
                                body_inst.type = InstructionType::SUBTRACT;
                                body_inst.var1 = "loop_result" + std::to_string(i) + "_" + std::to_string(j);
                                body_inst.var2 = "var" + std::to_string(std::max(1, i-1));
                                body_inst.value2 = value_dist(rng);
                                break;
                        }
                        inst.for_instructions.push_back(body_inst);
                    }
                    break;
                }
                default: // Default to PRINT
                    inst.type = InstructionType::PRINT;
                    inst.message = " Hello world from " + name + "!";
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
        p.logs.push_back(timestamp() + "Hello world from " + name + "!");
        processes.push_back(std::move(p));

        // P9
		p.created_tick = tick_count;    // stamp creation tick
		p.terminated_tick = -1;         // not finished yet

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
        if (p.current_instruction >= p.instructions.size()) {
            p.finished = true;
            p.state = Process::FINISHED;
            p.logs.push_back(timestamp() + " Process finished execution.");
            return;
        }

        const Instruction& inst = p.instructions[p.current_instruction];

        // Handle FOR loops
        if (!p.loop_stack.empty()) {
            auto& current_loop = p.loop_stack.back();
            if (current_loop.index < current_loop.instructions.size()) {
                const auto& loop_inst = current_loop.instructions[current_loop.index];
                execute_loop_instruction(p, loop_inst);
                current_loop.index++;
                return; 
            } else {
                current_loop.repeats_itself--;
                if (current_loop.repeats_itself > 0) {
                    current_loop.index = 0; // Reset loop body
                    p.logs.push_back(timestamp() + " FOR: Starting iteration " + 
                                    std::to_string(inst.for_repeats - current_loop.repeats_itself + 1));
                    return;
                } else {
                    // Loop completely finished
                    p.loop_stack.pop_back();
                    p.logs.push_back(timestamp() + " FOR: Loop completed");
                    p.current_instruction++;
                    p.executed++; // Only increment once when the entire FOR loop completes
                    return;
                }
            }
        }

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
                std::int64_t result = static_cast<std::int64_t>(val2) + static_cast<std::int64_t>(inst.value2);
                result = clamp_uint16(result);
                p.set_variable(inst.var1, static_cast<std::uint16_t>(result));
                p.logs.push_back(timestamp() + " ADD: " + inst.var1 + " = " + std::to_string(result));
                break;
            }
            
            case InstructionType::SUBTRACT: {
                std::uint16_t val2 = p.parse_operand(inst.var2);
                std::int64_t left = static_cast<std::int64_t>(val2);
                std::int64_t right = static_cast<std::int64_t>(inst.value2);
                std::int64_t result = left - right;
                result = clamp_uint16(result);
                p.set_variable(inst.var1, static_cast<std::uint16_t>(result));
                p.logs.push_back(timestamp() + " SUBTRACT: " + inst.var1 + " = " + std::to_string(result));
                break;
            }
            
            case InstructionType::SLEEP:
                p.state = Process::SLEEPING;
                p.sleep_remaining = inst.sleep_ticks;
                p.logs.push_back(timestamp() + " SLEEP: for " + std::to_string(inst.sleep_ticks) + " ticks");
                break;
                
            case InstructionType::FOR_START: {
                // Start a new FOR loop
                if (p.loop_stack.size() >= 3) {
                    p.logs.push_back(timestamp() + " FOR: Maximum nesting level reached, skipping");
                    break;
                }
                
                Process::LoopContext loop;
                loop.start = p.current_instruction;
                loop.repeats_itself = inst.for_repeats;
                loop.instructions = inst.for_instructions;
                loop.index = 0;
                
                p.loop_stack.push_back(loop);
                p.logs.push_back(timestamp() + " FOR: Starting loop with " + std::to_string(inst.for_repeats) + " iterations");
                return; 
            }
            
            default:
                p.logs.push_back(timestamp() + " Unknown instruction type.");
                break;
        }

        p.current_instruction++;
        p.executed++;
    }

    void execute_loop_instruction(Process& p, const Instruction& inst) {
        switch (inst.type) {
            case InstructionType::PRINT:
                p.logs.push_back(timestamp() + " PRINT (in loop): " + inst.message);
                break;
                
            case InstructionType::DECLARE:
                p.set_variable(inst.var1, inst.value1);
                p.logs.push_back(timestamp() + " DECLARE (in loop): " + inst.var1 + " = " + std::to_string(inst.value1));
                break;
                
            case InstructionType::ADD: {
                std::uint16_t val2 = p.parse_operand(inst.var2);
                std::int64_t result = static_cast<std::int64_t>(val2) + static_cast<std::int64_t>(inst.value2);
                result = clamp_uint16(result);
                p.set_variable(inst.var1, static_cast<std::uint16_t>(result));
                p.logs.push_back(timestamp() + " ADD (in loop): " + inst.var1 + " = " + std::to_string(result));
                break;
            }
            
            case InstructionType::SUBTRACT: {
                std::uint16_t val2 = p.parse_operand(inst.var2);
                std::int64_t left = static_cast<std::int64_t>(val2);
                std::int64_t right = static_cast<std::int64_t>(inst.value2);
                std::int64_t result = left - right;
                result = clamp_uint16(result);
                p.set_variable(inst.var1, static_cast<std::uint16_t>(result));
                p.logs.push_back(timestamp() + " SUBTRACT (in loop): " + inst.var1 + " = " + std::to_string(result));
                break;
            }
            
            default:
                p.logs.push_back(timestamp() + " Unknown loop instruction.");
                break;
        }
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
                    running[i]->logs.push_back(timestamp() + " Dispatched to core " + std::to_string(i));
                }
            }
        }

        // Execute one instruction per core
        for (size_t i = 0; i < running.size(); ++i) {
            if (running[i] != processes.end()) {
                Process& p = *running[i];
                
                // Handle delay per execution
                if (p.delay_remaining > 0) {
                    p.delay_remaining--;
                    continue;
                }
                
                // Execute the actual instruction
                execute_instruction(p, i);
                quantum_left[i]--;

                if (cfg.delay_per_exec > 0) {
                    p.delay_remaining = cfg.delay_per_exec - 1;
                }

                // Check if process finished
                if (p.current_instruction >= p.instructions.size() && p.loop_stack.empty()) {
                    p.finished = true;
                    p.state = Process::FINISHED;
					p.terminated_tick = tick_count; // P9: stamp termination tick
                    running[i] = processes.end();
                    p.logs.push_back(timestamp() + " Process finished on core " + std::to_string(i));

                    size_t released_bytes = p.memory_table.size() * sizeof(std::uint16_t);
                    p.memory_table.clear();
                    p.logs.push_back(timestamp() + "Released memory (" + std::to_string(released_bytes) + " bytes)");  
                }
                // Round Robin: quantum expired and not sleeping
                else if (cfg.scheduler == "rr" && quantum_left[i] <= 0 && p.state != Process::SLEEPING) {
                    p.logs.push_back(timestamp() + " Preempted on core " + std::to_string(i));
                    p.state = Process::READY;
                    ready_queue.push_back(find_process_by_id(p.id));
                    running[i] = processes.end();
                }
                // Process went to sleep
                else if (p.state == Process::SLEEPING) {
                    running[i] = processes.end();
                }
            }
        }

        // Try to assign processes to newly available cores
        for (size_t i = 0; i < running.size(); ++i) {
            if (running[i] == processes.end() && !ready_queue.empty()) {
                auto next_proc = ready_queue.front();
                ready_queue.pop_front();
                next_proc->state = Process::RUNNING;
                running[i] = next_proc;
                quantum_left[i] = (cfg.scheduler == "rr") ? cfg.quantum_cycles : INT_MAX;
                next_proc->logs.push_back(timestamp() + " Dispatched to core " + std::to_string(i));
            }
        }
        
        // Skip exisiting process names
        if (cfg.batch_process_freq > 0 && batch_tick_counter >= cfg.batch_process_freq) {
            batch_tick_counter = 0;

			std::lock_guard<std::recursive_mutex> lock(proc_mutex); // thread safety

            std::string name;
            do {
                name = "p" + std::to_string(auto_process_counter++);
			} while (find_process_by_name(name) != processes.end());

            add_process_locked(name);
        }
    }

    // ===== REPORT =====
    void report_util() {
        std::lock_guard<std::recursive_mutex> lock(proc_mutex);

        // Counts
        int total = 0, running_cnt = 0, finished_cnt = 0;
        long long tat_sum = 0;

        for (const auto& p : processes) {
            total++;
            if (p.finished) {
                finished_cnt++;
                if (p.terminated_tick >= 0) {
                    tat_sum += (p.terminated_tick - p.created_tick);
                }
            }
            else if (p.state == Process::RUNNING) {
                running_cnt++;
            }
		}

        // Cores
        int cores_used = 0;
        for (const auto& core : running)
            if (core != processes.end())
                cores_used++;

        double cpu_util = (cfg.num_cpu > 0) ? (100.0 * cores_used / cfg.num_cpu) : 0.0;
        double throughput = (tick_count > 0) ? (static_cast<double>(finished_cnt) / tick_count) : 0.0;
        double avg_tat = (finished_cnt > 0) ? (static_cast<double>(tat_sum) / finished_cnt) : 0.0;

		// Report string
        std::ostringstream oss;
        oss << "===== CPU UTILIZATION REPORT (with Runtime Stats) =====\n";
        oss << "Timestamp: " << timestamp() << "\n";
        oss << "Total CPU ticks: " << tick_count << "\n";
        oss << "Cores: " << cfg.num_cpu << " | Busy: " << cores_used
            << " | Idle: " << (cfg.num_cpu - cores_used) << "\n";
        oss << std::fixed << std::setprecision(2);
		oss << "CPU Utilization: " << cpu_util << "%\n";
		oss << "Throughput: " << throughput << " processes/tick\n";
		oss << "Average TAT: " << avg_tat << " ticks\n";
        oss << "Process Counts -> Total: " << total
            << " | Running: " << running_cnt
			<< " | Finished: " << finished_cnt << "\n";
		oss << "======================================================================\n";

        // List all processes with their status
        oss << std::left;
        oss << std::setw(8) << "Process"
            << " | " << std::setw(4) << "ID"
            << " | " << std::setw(10) << "State"
            << " | " << std::setw(12) << "Progress"
            << " | " << std::setw(11) << "Memory"
            << " | " << "TAT\n";
		oss << "----------------------------------------------------------------------\n";

        for (const auto& p : processes) {
            std::string state_str;
            if (p.finished) state_str = "finished";
            else if (p.state == Process::RUNNING) state_str = "running";
			else if (p.state == Process::SLEEPING) state_str = "sleeping"; // added sleeping state
            else state_str = "ready";

            // Compute per-process tat if finished
			std::string tat_str = "-";
            if (p.finished && p.terminated_tick >= 0)
				tat_str = std::to_string(p.terminated_tick - p.created_tick);

            oss << std::setw(8) << p.name 
                << " | " << std::setw(4) << p.id 
                << " | " << std::setw(10) << state_str 
                << " | " << std::setw(12) << (std::to_string(p.executed) + "/" + std::to_string(p.total_instructions)) 
                << " | " << std::setw(2) << p.memory_bytes() 
                << " (" << p.memory_table.size() << " vars)" 
                << " | " << tat_str << "\n";

        }

        oss << "======================================================================\n";

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