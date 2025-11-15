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

void screenCommand(const vector<string>& commandBuffer) {
    if (!isInitialized) {
        cout << "Please run the \"initialize\" command first\n";
        return;
    }

    if (commandBuffer.size() == 2) {
        if (commandBuffer[1] == "-s" || commandBuffer[1] == "-r")
            cout << "Usage: screen [-r | -s] [name]\n";
        else if (commandBuffer[1] == "-ls") {
            cout << "Listing all running consoles\n";
            consoles.listConsoles();
        }
        else {
            cout << "Screen command \"" << commandBuffer[1] << "\" not recognized. Try again.\n";
        }
    }
    else if (commandBuffer.size() == 3) {
        if (commandBuffer[1] == "-s") {
            if (consoles.consoleExists(commandBuffer[2])) {
                cout << "Console \"" << commandBuffer[2] << "\" already exists.\n";
            }
            else {
                clearCommand();
                consoles.addConsole(commandBuffer[2], true);
                consoles.loopConsole(commandBuffer[2]);
                clearCommand();
                displayHeader();
            }
        }
        else if (commandBuffer[1] == "-r") {
            if (!consoles.consoleExists(commandBuffer[2])) {
                cout << "Process \"" << commandBuffer[2] << "\" not found." << endl;
            }
            else if (consoles.getConsoleStatus(commandBuffer[2]) == process_console::TERMINATED) {
                cout << "Process \"" << commandBuffer[2] << "\" not found." << endl;
            }
            else {
                cout << "Reopening console \"" << commandBuffer[2] << "\"\n";
                consoles.displayConsole(commandBuffer[2]);
                consoles.loopConsole(commandBuffer[2]);
                clearCommand();
                displayHeader();
            }
        }
        else {
            cout << "Screen command \"" << commandBuffer[1] << "\" not recognized. Try again.\n";
        }
    }
    else {
        cout << "Usage: screen [-r | -s] [name]\n";
    }
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
