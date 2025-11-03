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


/*
* This function prints the ASCII text header
*/
void displayHeader() {
    cout << "\033[32m" << "Hello, Welcome to CSOPESY command-line interface." << "\033[0m" << endl;
    cout << "\033[33m" << "Type 'exit' to quit, 'clear' to clear the screen.\n" << "\033[0m" << endl;
}

/*
* This function clears the console screen
*/
void clearCommand() {
    system("cls");
}

/*
* This function processes the screen command
* User may opt to start a new console or reopen an existing console
*
* @param commandBuffer - a vector of strings containing the command and its arguments
*/
void screenCommand(const vector<string>& commandBuffer) {
    // screen command error validation
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
    // if screen command is valid
    else if (commandBuffer.size() == 3) {
        // if screen command is "start console"
        if (commandBuffer[1] == "-s") {
            // check if console already exists
            if (consoles.consoleExists(commandBuffer[2])) {
                cout << "Console \"" << commandBuffer[2] << "\" already exists.\n";
            }
            // if console does not exist, create new console
            else {
                clearCommand();
                consoles.addConsole(commandBuffer[2], true); // Add new console to console list
                consoles.loopConsole(commandBuffer[2]); // initialize console program
                clearCommand();
                displayHeader();
            }
        }
        // if screen command is "reopen console"
        else if (commandBuffer[1] == "-r") {
            // Check if console exists
            if (!consoles.consoleExists(commandBuffer[2])) {
                cout << "Process \"" << commandBuffer[2] << "\" not found." << endl;
            }
            // Check if the console has finished execution
            else if (consoles.getConsoleStatus(commandBuffer[2]) == process_console::TERMINATED) {
                cout << "Process \"" << commandBuffer[2] << "\" not found." << endl;
            }
            // If console exists and is still running, reopen console
            else {
                cout << "Reopening console \"" << commandBuffer[2] << "\"\n";
                consoles.displayConsole(commandBuffer[2]); // Display reopened console
                consoles.loopConsole(commandBuffer[2]); // Reinitialize console program
                clearCommand();
                displayHeader();
            }
        }
        // if screen command syntax is invalid
        else {
            cout << "Screen command \"" << commandBuffer[1] << "\" not recognized. Try again.\n";
        }
    }
    // if command syntax is invalid
    else {
        cout << "Usage: screen [-r | -s] [name]\n";
    }
}

/*
* This function checks if the input command is valid (accepted) or not
* including specific actions for clear, exit, and screen commands
*
* @param commandBuffer - a vector of strings containing the command and its arguments
*/
void checkCommand(const vector<string>& commandBuffer) {

    string command = commandBuffer[0];

    if (command == "clear") {
        clearCommand();
        displayHeader();
    }
    else if (command == "exit") {
        cout << command << " command recognized. Thank you! Exiting program.\n";
        exit(0);
    }
    else if (!isInitialized) {
        if (command == "initialize") {
            cout << "Menu initialized.\n";
            consoles.initialize();
            // consoles.testConfig();
            isInitialized = true;
        }
        else if (command == "screen" || command == "scheduler-start" || command == "scheduler-stop" || command == "report-util") {
            cout << "Please run the \"initialize\" command first\n";
        }
        else {
            cout << "Command " << command << " not recognized. Please try again.\n";
        }
    }

    else if (isInitialized) {
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

    // consoles.testConfig();

    displayHeader();

    while (true) {
        commandBuffer.clear();

        cout << "Enter command: ";

        // This loop reads the command and iteratively pushes each string read 
        // into the commandBuffer vector until it detects a newline
        while (cin >> command) {
            commandBuffer.push_back(command);
            if (cin.peek() == '\n')
                break;
        }

        // if commandBuffer is not empty, check command entered
        if (!commandBuffer.empty()) {
            checkCommand(commandBuffer);
        }
    }

    return 0;
}
