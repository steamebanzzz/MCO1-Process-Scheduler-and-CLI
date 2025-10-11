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

struct Config {
    int num_cpu = 1;
    std::string scheduler = "fcfs";
    int quantum_cycles = 1;
    int batch_process_freq = 0;
    int min_ins = 3;
    int max_ins = 8;
    int delay_per_exec = 0;
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
    int total_instructions = 0;
    int executed = 0;
    std::vector<std::string> logs;
    std::vector<std::string> ins_types = { "LOAD", "STORE", "ADD", "SUB", "MUL", "DIV", "JMP", "CMP" };

    void print_smi_unsafe() const {
        std::cout << "Process name: " << name << "\n";
        std::cout << "ID: " << id << "\n";
        std::cout << "Logs:\n";
        for (const auto& l : logs) std::cout << "  " << l << "\n";
        if (finished) std::cout << "Finished!\n";
        else {
            std::cout << "Current instruction line: " << executed << "\n";
            std::cout << "Lines of code: " << total_instructions << "\n";
        }
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
#ifdef _WIN32
        localtime_s(&tm, &tt);
#else
        tm = *std::localtime(&tt);
#endif
        std::ostringstream ss;
        ss << "(" << std::put_time(&tm, "%m/%d/%Y %I:%M:%S%p") << ")";
        return ss.str();
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
        std::cout << "ID\tName\tState\n";
        for (const auto& p : processes)
            std::cout << p.id << "\t" << p.name << "\t" << (p.finished ? "finished" : "running") << "\n";
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

    // ===== PROCESS HANDLING =====
    void add_process_locked(const std::string& name) {
        std::uniform_int_distribution<int> ins(cfg.min_ins, cfg.max_ins);
        Process p;
        p.id = next_pid++;
        p.name = name;
        p.total_instructions = ins(rng);
        p.executed = 0;
        p.logs.push_back(timestamp() + " Core:0 \"Hello world from " + name + "\"");
        processes.push_back(std::move(p));
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
                // Wait briefly to ensure the scheduler has time to tick
                std::this_thread::sleep_for(std::chrono::milliseconds(600));

                Process snapshot;
                {
                    std::lock_guard<std::recursive_mutex> lock(proc_mutex);
                    auto it = find_process_by_id(pid);
                    if (it == processes.end()) {
                        std::cout << "Process not found.\n";
                        continue;
                    }
                    snapshot = *it; // Copy while locked
                }

                // Print outside the lock (scheduler can keep updating logs)
                snapshot.print_smi_unsafe();

                // Optional: auto-exit if process has finished
                if (snapshot.finished) {
                    std::cout << "Process has finished execution. Returning to main menu.\n";
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

    void simulate_tick() {
        std::lock_guard<std::recursive_mutex> lock(proc_mutex);
        tick_count++;
        batch_tick_counter++;

        std::uniform_int_distribution<int> core(0, std::max(0, cfg.num_cpu - 1));
        std::uniform_int_distribution<int> instr(0, 7);

        for (auto& p : processes) {
            if (p.finished) continue;
            p.executed++;
            std::string itype = p.ins_types[instr(rng)];
            p.logs.push_back(timestamp() + " Core:" + std::to_string(core(rng)) +
                " Executed " + itype + " in " + p.name);
            if (p.executed >= p.total_instructions) {
                p.finished = true;
                p.logs.push_back("Finished!");
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
        std::cout << "===== CPU UTILIZATION REPORT =====\n";
        std::cout << "Total CPU ticks: " << tick_count << "\n";
        std::cout << "Cores: " << cfg.num_cpu << "\n";
        for (const auto& p : processes)
            std::cout << "  " << p.name << " | ID: " << p.id
            << " | " << (p.finished ? "finished" : "running")
            << " | Progress: " << p.executed << "/" << p.total_instructions << "\n";
        std::cout << "==================================\n";
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

