
#include "Shell.h"
#include <iostream>
#include <string>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
//#include <signal.h>


const std::string PATH_BEGINNING = "/bin/";
const char *FORK_ERR = "fork failed";
const char *EXECV_ERR = "execv failed";
const char *COMMAND_NOT_FOUND = ": command not found";
const char BG_TOKEN = '&';
const std::string WHITESPACES(" \t\f\v\n\r");
const std::string MYJOBS_COMMAND = "myjobs";
const int RUNNING = 0;

/*
 * Constructor that receives commands and their arguments and execute them.
 */
Shell::Shell() {
    while (true) {
        std::string commandLine;
        std::getline(std::cin, commandLine);
        std::pair<std::string, std::string> tokenized_result = tokenizeCommand(commandLine);

        std::string command = tokenized_result.first;

        if (command != PATH_BEGINNING)
            executeCommand(command, tokenized_result.second);
    }
}

std::pair<std::string, std::string> Shell::tokenizeCommand(const std::string &commandLine) {
    std::string command;
    std::string commandsVariables;
    std::string token;
    std::istringstream iss(commandLine);
    iss >> command;
    while (iss >> token)
        commandsVariables += token + " ";

    if (command.rfind(PATH_BEGINNING, 0) != 0)
        command = PATH_BEGINNING + command;

    // remove the whitespaces in the end.
    std::size_t found = commandsVariables.find_last_not_of(WHITESPACES);
    if (found != std::string::npos)
        commandsVariables.erase(found + 1);

    return std::make_pair(command, commandsVariables);
}

void Shell::executeCommand(std::string command, std::string commandsVariables) {
    bool runInBackground = false;
    if (!commandsVariables.empty() && commandsVariables.back() == BG_TOKEN) {
        runInBackground = true;
        commandsVariables.pop_back();
    }
    char *args[3];
    args[0] = command.data();
    args[1] = commandsVariables.empty() ? NULL : commandsVariables.data();
    args[2] = NULL;

    if (command == PATH_BEGINNING + MYJOBS_COMMAND)
        myJobsCommand();

    else {

        pid_t pid = fork();
        if (pid < 0) // can not fork
            perror(FORK_ERR);

        else if (pid == 0) // child process
        {
            // check if the command exist
            if (access(command.c_str(), F_OK) != 0)
                std::cout << command << COMMAND_NOT_FOUND << std::endl;

            else if (execv(args[0], args) < 0)
                perror(EXECV_ERR);

            else // parent process
            {
                if (runInBackground) {
                    _myJobs.push_back({command, commandsVariables, pid, RUNNING});
                } else
                    waitpid(pid, NULL, 0);
            }

        }

    }
}

    void Shell::myJobsCommand() {

        checkBackgroundJobs();
        for (const auto &job: _myJobs) {
            std::cout << job.pid << job.command << " " << job.commandsVariables << " " << job.status << std::endl;
        }
    }

    bool isJobFinished(Job job) {
        return job.status != 0;
    }

    void Shell::checkBackgroundJobs()
    {
        for (auto it = _myJobs.begin(); it != _myJobs.end(); it++) {
            int status = 0;
            pid_t result = waitpid(it->pid, &status, WNOHANG);
            it->status = result;
        }
        _myJobs.erase(std::remove_if(_myJobs.begin(), _myJobs.end(), isJobFinished), _myJobs.end());
    }

