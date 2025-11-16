#include <iostream>
#include <string>
#include <cstdlib>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include "os_core.h"
#include "process_console.h"

using namespace std;

ConsoleManager consoles;
bool isInitialized = false;
int instructionLengthPerProcess = 5;

void displayHeader() {
    cout << "Hello, Welcome to CSOPESY command-line interface." << endl;
    cout << "Type 'exit' to quit, 'clear' to clear the screen.\n" << endl;
}

void clearCommand() {
    system("cls");
}

void displayHelp() {
    cout << "\nAvailable commands:\n";
    cout << "  initialize           - Initialize the console menu\n";
    cout << "  screen -s [name]     - Start a new console with the given name\n";
    cout << "  screen -r [name]     - Reopen an existing console\n";
    cout << "  screen -ls           - List all running consoles\n";
    cout << "  scheduler-start      - Start the scheduler test\n";
    cout << "  scheduler-stop       - Stop the scheduler test\n";
    cout << "  report-util          - Generate CPU/process utilization report\n";
    cout << "  clear                - Clear the screen\n";
    cout << "  help                 - Display this help message\n";
    cout << "  exit                 - Exit the program\n\n";
}

void screenCommand(const vector<string>& cmd) {
    if (!isInitialized) {
        cout << "Please run the \"initialize\" command first\n";
        return;
    }

    if (cmd.size() < 2) {
        cout << "Usage:\n"
            << "  screen -s <name> <memory>\n"
            << "  screen -c <name> <memory> \"<instructions>\"\n"
            << "  screen -r <name>\n"
            << "  screen -ls\n";
        return;
    }

    string opt = cmd[1];

    // -----------------------
    // LIST CONSOLES
    // -----------------------
    if (opt == "-ls") {
        consoles.listConsoles();
        return;
    }

    // -----------------------
    // SCREEN -S  (Create process without instructions)
    // -----------------------
    if (opt == "-s") {
        if (cmd.size() < 4) {
            cout << "Usage: screen -s <name> <memory>\n";
            return;
        }

        string name = cmd[2];
        int mem = stoi(cmd[3]);

        cout << "[Parsed] screen -s\n";
        cout << "  Name: " << name << "\n";
        cout << "  Memory: " << mem << "\n";

        return;
    }

    // -----------------------
    // SCREEN -C  (Create process with instructions)
    // -----------------------
    if (opt == "-c") {
        if (cmd.size() < 5) {
            cout << "Usage: screen -c <name> <memory> \"<instructions>\"\n";
            return;
        }

        string name = cmd[2];
        int mem = stoi(cmd[3]);

        // Reconstruct the instruction string (because it may contain spaces)
        string instructions;
        for (int i = 4; i < cmd.size(); i++) {
            instructions += cmd[i] + " ";
        }
        if (instructions.front() == '"') instructions.erase(0, 1);
        if (instructions.back() == '"') instructions.pop_back();

        cout << "[Parsed] screen -c\n";
        cout << "  Name: " << name << "\n";
        cout << "  Memory: " << mem << "\n";
        cout << "  Instructions: " << instructions << "\n";

        return;
    }

    // -----------------------
    // SCREEN -R
    // -----------------------
    if (opt == "-r") {
        if (cmd.size() < 3) {
            cout << "Usage: screen -r <name>\n";
            return;
        }

        cout << "[Parsed] screen -r\n";
        cout << "  Requesting: " << cmd[2] << "\n";
        return;
    }

    cout << "Screen command \"" << opt << "\" not recognized.\n";
}


/*void testMemoryManager(ConsoleManager& cm) {
    cout << "\n[Memory Manager Test]\n";
    cout << "Total memory: " << cm.max_overall_mem << " bytes\n";
    cout << "Memory per frame: " << cm.mem_per_frame << " bytes\n";
    cout << "Frames: " << cm.max_overall_mem / cm.mem_per_frame << "\n";
    cout << "-------------------------\n";
}*/

void checkCommand(const vector<string>& commandBuffer) {
    string command = commandBuffer[0];

    if (command == "clear") {
        clearCommand();
        displayHeader();
    }
    else if (command == "help") {
        displayHelp();
    }
    else if (command == "exit") {
        cout << command << " command recognized. Thank you! Exiting program.\n";
        exit(0);
    }
    else if (!isInitialized) {
        if (command == "initialize") {
            consoles.initialize();
            cout << "Menu initialized" << "\n";
            //testMemoryManager(consoles);
            cout << "Total memory: " << consoles.max_overall_mem << " bytes ("
                << (consoles.max_overall_mem / consoles.mem_per_frame) << " frames of "
                << consoles.mem_per_frame << " bytes)\n";
            //consoles.testMemoryAllocation();
            //consoles.testConfig();
            isInitialized = true;
        }
        else {
            cout << "Please run the \"initialize\" command first\n";
        }
    }
    else {
        if (command == "initialize") {
            cout << "Already initialized.\n";
        }
        else if (command == "screen") {
            screenCommand(commandBuffer);
        }
        else if (command == "scheduler-start") {
            cout << "Running scheduler test\n";
            consoles.schedulerTest(true);
        }
        else if (command == "scheduler-stop") {
            cout << "Stopping scheduler test\n";
            consoles.schedulerTest(false);
        }
        else if (command == "report-util") {
            cout << "Generating report...\n";
            consoles.reportUtil();
        }
        else if (command == "process-smi") {
            cout << "[Parsed] process-smi\n";
        }
        else if (command == "vmstat") {
            cout << "[Parsed] vmstat\n";
        }
        else {
            cout << "Command " << command << " not recognized. Please try again.\n";
        }
    }
}

int main() {
    vector<string> commandBuffer;
    string command;

    displayHeader();

    while (true) {
        commandBuffer.clear();
        cout << "Enter command: ";

        while (cin >> command) {
            commandBuffer.push_back(command);
            if (cin.peek() == '\n')
                break;
        }

        if (!commandBuffer.empty()) {
            checkCommand(commandBuffer);
        }
    }

    return 0;
}
