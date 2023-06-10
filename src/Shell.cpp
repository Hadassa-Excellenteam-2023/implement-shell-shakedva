
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
const char IN_REDIRECTION_TOKEN = '<';
const char PIPE_TOKEN = '|';

/*
 * Constructor that receives commands and their arguments and execute them.
 */
Shell::Shell() {
    while (true) {
        std::string commandLine;
        std::getline(std::cin, commandLine);
        std::vector<std::pair<std::string, std::string>> commands = tokenizeCommands(commandLine);
        executeCommand(commands);
    }
}

/*
 * Receives the input from the user and tokenize it into the executable command and its variables
 */
std::vector<std::pair<std::string, std::string>> Shell::tokenizeCommands(const std::string &commandLine) {
    std::string command;
    std::istringstream commandLineIss(commandLine);
    std::vector<std::pair<std::string, std::string>> commands;

    while (std::getline(commandLineIss, command, PIPE_TOKEN)) {
        std::string executable, variables;
        std::istringstream commandIss(command);
        commandIss >> executable;
        std::getline(commandIss, variables);

        commands.emplace_back(addPathBeginning(executable), trim(variables));
    }
    return commands;
}

std::string Shell::addPathBeginning(std::string &s) {
    if (s.rfind(PATH_BEGINNING, 0) != 0)
        s = PATH_BEGINNING + s;
    return s;
}

/*
 * Execute the user's command
 */
void Shell::executeCommand(const std::vector<std::pair<std::string, std::string>> &commands) {
    if (commands.empty())
        return;
    else if (commands.size() == 1) {
        std::string command = commands[0].first;
        std::string commandsVariables = commands[0].second;

        std::string outputFileName, inputFileName;
        bool redirectOut = false;
        bool redirectIn = false;
        bool runInBackground = false;
        int fd = -1;
        char *args[3];
        args[0] = command.data();
        if (commandsVariables.empty())
            args[1] = NULL;

        else {
            if (commandsVariables.back() == BG_TOKEN) {
                runInBackground = true;
                commandsVariables.pop_back();
            }
            redirectIn = parseInputRedirection(inputFileName, commandsVariables, fd);
            redirectOut = parseOutputRedirection(outputFileName, commandsVariables, fd);
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
                if (!validateCommand(command)) {
                    std::cout << command << COMMAND_NOT_FOUND << std::endl;
                    return;
                }

                if (redirectOut && dup2(fd, STDOUT_FILENO) < 0) {  // Redirect stdout to the fd
                    perror("dup2 err");
                    return;
                }
                if (redirectIn && dup2(fd, STDIN_FILENO) < 0) {
                    perror("dup2 err");
                    return;
                }

                if (execv(args[0], args) < 0)
                    perror(EXECV_ERR);
            } else // parent process
            {
                if (runInBackground) {
                    _myJobs.push_back({command, commandsVariables, pid, RUNNING});
                } else {
                    waitpid(pid, NULL, 0);
                    if (redirectOut || redirectIn)
                        close(fd);
                }
            }

        }
    } else {
        std::string outputFileName, inputFileName;
        bool runInBackground = false;
        int fd[2], in = 0, out = 1;
        char *args[3];
        for (size_t i = 0; i < commands.size(); i++) {
            std::string command = commands[i].first;
            std::string commandsVariables = commands[i].second;
            args[0] = command.data();

            if (commandsVariables.empty())
                args[1] = NULL;
            else {
//                redirectIn = parseInputRedirection(inputFileName, commandsVariables, fd[PIPE_READ]);
//                redirectOut = parseOutputRedirection(outputFileName, commandsVariables, fd[PIPE_WRITE]);
                args[1] = commandsVariables.empty() ? NULL : commandsVariables.data();
            }
            args[2] = NULL;
            if (command == PATH_BEGINNING + MYJOBS_COMMAND)
                myJobsCommand();

            else {
                pipe(fd);
                pid_t pid = fork();
                if (pid < 0) // can not fork
                    perror(FORK_ERR);
                else if (pid == 0) // child process
                {
                    if (!validateCommand(command)) {
                        std::cout << command << COMMAND_NOT_FOUND << std::endl;
                        return;
                    }

                    if (in != 0) {
                        dup2(in, 0); // direct stdin to in (from 2nd command)
                        close(in);
                    }

                    if (i == commands.size() - 1) {
                        if (dup2(1, STDOUT_FILENO) < 0) {  // Redirect stdout to the stdout
                            perror("dup2 err");
                            return;
                        }
                    }
                        // not include the last
                    else {
                        if (dup2(fd[1], STDOUT_FILENO) < 0) {  // Redirect stdout to the fd
                            perror("dup2 err");
                            return;
                        }
                    }
//                    dup2 (fd[1], 1);
//                   close (out);

                    if (execv(args[0], args) < 0)
                        perror(EXECV_ERR);
                } else // parent process
                {
                    waitpid(pid, NULL, 0);
                    close(fd[1]); // close the out file
                    in = fd[0];
                }
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
void Shell::checkBackgroundJobs() {
    for (auto &_myJob: _myJobs) {
        int status = 0;
        pid_t result = waitpid(_myJob.pid, &status, WNOHANG);
        _myJob.status = result;
    }
    _myJobs.erase(std::remove_if(_myJobs.begin(), _myJobs.end(), isJobFinished), _myJobs.end());
}

bool Shell::validateCommand(const std::string &command) {
    return access(command.c_str(), F_OK) == 0;
}

int Shell::openOutputFd(const std::string &outputFileName) {
    int fd = open(outputFileName.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        perror("open err");
        exit(EXIT_FAILURE); //todo not exit
    }
    return fd;
}

bool Shell::parseOutputRedirection(std::string &outputFileName, std::string &commandsVariables, int &fd) {
    std::size_t pos = commandsVariables.find_first_of(OUT_REDIRECTION_TOKEN);
    if (pos != std::string::npos) {
        outputFileName = trim(commandsVariables.substr(pos + 1));
        commandsVariables = trim(commandsVariables.substr(0, pos));
        fd = openOutputFd(outputFileName);
        return true;
    }
    return false;
}

bool Shell::parseInputRedirection(std::string &inputFileName, std::string &commandsVariables, int &fd) {
    std::size_t pos = commandsVariables.find_first_of(IN_REDIRECTION_TOKEN);
    if (pos != std::string::npos) {
        inputFileName = trim(commandsVariables.substr(pos + 1));
        commandsVariables = inputFileName;
        fd = openInputFd(inputFileName);
        return true;
    }
    return false;
}

int Shell::openInputFd(const std::string &inputFileName) {
    int fd = open(inputFileName.c_str(), O_RDONLY);
    if (fd < 0) {
        perror("open err");
        exit(EXIT_FAILURE);
    }
    return fd;
}

std::string ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACES);
    return (start == std::string::npos) ? "" : s.substr(start);
}

std::string rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACES);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

// Taken from https://www.techiedelight.com/
std::string Shell::trim(const std::string &s) {
    return rtrim(ltrim(s));
}


