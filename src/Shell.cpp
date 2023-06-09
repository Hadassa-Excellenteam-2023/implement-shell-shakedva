
#include "Shell.h"
#include <iostream>
#include <string>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

const std::string PATH_BEGINNING = "/bin/";
const char *FORK_ERR = "fork failed";
const char *EXECV_ERR = "execv failed";
const char *COMMAND_NOT_FOUND = ": command not found";
const char BG_TOKEN = '&';
const std::string WHITESPACES(" \t\f\v\n\r");
const std::string MYJOBS_COMMAND = "myjobs";
const int RUNNING = 0;

const char OUT_REDIRECTION_TOKEN = '>';

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

/*
 * Receives the input from the user and tokenize it into the command and its variables
 */
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

/*
 * Execute the user's command
 */
void Shell::executeCommand(std::string command, std::string commandsVariables) {
    std::string outputFileName;
    bool redirectOut = false;
    bool runInBackground = false;
    int fd = -1;
    char *args[3];
    args[0] = command.data();
    if(commandsVariables.empty())
        args[1] = NULL;

    else
    {
        if(commandsVariables.back() == BG_TOKEN)
        {
            runInBackground = true;
            commandsVariables.pop_back();
        }
        // check for redirection token
        std::size_t pos = commandsVariables.find_first_of(OUT_REDIRECTION_TOKEN);
        if(pos != std::string::npos) {
            outputFileName = commandsVariables.substr(pos + 1);
            commandsVariables = commandsVariables.substr(0, pos);
            redirectOut = true;
            fd = openOutputFd(outputFileName);
        }


        args[1] = commandsVariables.empty() ? NULL : commandsVariables.data();
    }
    args[2] = NULL;
    if (command == PATH_BEGINNING + MYJOBS_COMMAND)
        myJobsCommand();

    else {
        pid_t pid = fork();
        if (pid < 0) // can not fork
            perror(FORK_ERR);

        else if (pid == 0) // child process
        {
            if(!validateCommand(command)) {
                std::cout << command << COMMAND_NOT_FOUND << std::endl;
                return;
            }

            if (redirectOut && dup2(fd, STDOUT_FILENO) < 0) {  // Redirect stdout to the file descriptor
                perror("dup2 err");
                exit(EXIT_FAILURE);
            }

            if (execv(args[0], args) < 0)
                perror(EXECV_ERR);
        }
        else // parent process
        {
            if (runInBackground) {
                _myJobs.push_back({command, commandsVariables, pid, RUNNING});
            } else {
                waitpid(pid, NULL, 0);
                if(redirectOut)
                    close(fd);
            }
        }

    }
}

/*
 * Allows the user to see all the current processes that are working in the background and their information
 */
void Shell::myJobsCommand() {
    checkBackgroundJobs();
    for (const auto &job: _myJobs) {
        std::cout << job.pid << job.command << " " << job.commandsVariables << " " << job.status << std::endl;
    }
}
/*
 * Check if a background job is finished
 */
bool isJobFinished(Job job) {
    return job.status != 0;
}
/*
 * Updates the data structure that holds all the background jobs.
 */
void Shell::checkBackgroundJobs()
{
    for (auto it = _myJobs.begin(); it != _myJobs.end(); it++) {
        int status = 0;
        pid_t result = waitpid(it->pid, &status, WNOHANG);
        it->status = result;
    }
    _myJobs.erase(std::remove_if(_myJobs.begin(), _myJobs.end(), isJobFinished), _myJobs.end());
}

int Shell::openOutputFd(const std::string& outputFileName) {
    int fd = open(outputFileName.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        perror("open err");
        exit(EXIT_FAILURE);
    }
    return fd;
}

bool Shell::validateCommand(const std::string& command) {
    return access(command.c_str(), F_OK) == 0;
}


