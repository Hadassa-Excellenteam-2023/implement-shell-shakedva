
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
const char *DUP2_ERR = "dup2 err";
const std::string WHITESPACES(" \t\f\v\n\r");
const std::string MYJOBS_COMMAND = "myjobs";
const int RUNNING = 0;
const int EXECV_LEN = 3;
const char BG_TOKEN = '&';
const char OUT_REDIRECTION_TOKEN = '>';
const char IN_REDIRECTION_TOKEN = '<';
const char PIPE_TOKEN = '|';
const int PIPE_READ = 0;
const int PIPE_WRITE = 1;

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

/*
 * Adds beginning to each command
 */
std::string Shell::addPathBeginning(std::string &s) {
    if (s.rfind(PATH_BEGINNING, 0) != 0)
        s = PATH_BEGINNING + s;
    return s;
}

/*
 * Execute the user's command
 */
void Shell::executeCommand(std::vector<std::pair<std::string, std::string>> &commands) {
    if (commands.empty())
        return;
    std::string outputFileName, inputFileName;
    bool runInBackground = isBackgroundJob(commands);
    int fd[2];
    int input_descriptor = 0;
    char *args[EXECV_LEN];

    for (size_t i = 0; i < commands.size(); i++) {
        bool redirectIn = false;
        bool redirectOut = false;
        int redirectionFd;

        std::string command = commands[i].first;
        std::string commandsVariables = commands[i].second;
        args[0] = command.data();

        if (commandsVariables.empty())
            args[1] = NULL;
        else {
            redirectIn = parseInputRedirection(inputFileName, commandsVariables, redirectionFd);
            redirectOut = parseOutputRedirection(outputFileName, commandsVariables, redirectionFd);
            args[1] = commandsVariables.empty() ? NULL : commandsVariables.data();
        }
        args[2] = NULL;
        if (command == PATH_BEGINNING + MYJOBS_COMMAND) {
            myJobsCommand();
            return;
        }

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
            // read from input file descriptor
            // stdin for first command and pipes for the rest
            if (input_descriptor != 0) {
                if(dup2(input_descriptor, STDIN_FILENO) < 0) {
                    perror(DUP2_ERR);
                    return;
                }
                close(input_descriptor);
            }

            // check the file descriptor of the last command in the pipe
            if (i == commands.size() - 1)
                handleFdOfLastCommandInPipe(redirectOut, redirectIn, redirectionFd);
            else {
                // pipes write into the file descriptor PIPE_WRITE
                if (dup2(fd[PIPE_WRITE], STDOUT_FILENO) < 0) {
                    perror(DUP2_ERR);
                    return;
                }
            }
            if (execv(args[0], args) < 0)
                perror(EXECV_ERR);
        }
        else // parent process
        {
            if (runInBackground && commands.size() == 1) {
                _myJobs.push_back({command, commandsVariables, pid, RUNNING});
            } else {
                waitpid(pid, NULL, 0);
                close(fd[PIPE_WRITE]); // close the out file
                input_descriptor = fd[PIPE_READ];
                if (redirectIn || redirectOut)
                    close(redirectionFd);
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
    return job.status != RUNNING;
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

/*
 * Returns whether the command exist
 */
bool Shell::validateCommand(const std::string &command) {
    return access(command.c_str(), F_OK) == 0;
}

/*
 * Opens output file descriptor for reading, writing.
 * Overwrite the file from the beginning
 */
int Shell::openOutputFd(const std::string &outputFileName) {
    int fd = open(outputFileName.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        perror("open err");
        exit(EXIT_FAILURE); //todo not exit
    }
    return fd;
}
/*
 * Checks if there is an output redirection, and if so divides the variables into the requested file name and variables.
 */
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
/*
 * Checks if there is an input redirection, and if so divides the variables into the requested file name and variables.
 */
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

/*
 * Opens input file descriptor
 */
int Shell::openInputFd(const std::string &inputFileName) {
    int fd = open(inputFileName.c_str(), O_RDONLY);
    if (fd < 0) {
        perror("open err");
        exit(EXIT_FAILURE);
    }
    return fd;
}
/*
 * Trims trailing whitespaces
 */
std::string ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACES);
    return (start == std::string::npos) ? "" : s.substr(start);
}

/*
 * Trims leading whitespaces
 */
std::string rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACES);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

/*
 * Trims leading and trailing whitespaces
 * Taken from https://www.techiedelight.com/
 */
std::string Shell::trim(const std::string &s) {
    return rtrim(ltrim(s));
}
/*
 * Returns if the user requested for a background job
 */
bool Shell::isBackgroundJob(std::vector<std::pair<std::string, std::string>> &commands) {
    if (!commands.empty() && commands.back().second.back() == BG_TOKEN) {
        commands.back().second.pop_back();
        commands.back().second = trim(commands.back().second);
        return true;
    }
    return false;
}
/*
 * The last command in pipe can be directed out to standard output or to another file descriptor, or an input from
 * a file descriptor.
 */
void Shell::handleFdOfLastCommandInPipe(bool redirectOut, bool redirectIn, int redirectionFd) {

    // direct out to redirection file descriptor
    if (redirectOut &&
        dup2(redirectionFd, STDOUT_FILENO) < 0) {
        perror(DUP2_ERR);
        return;
    }
    // direct input from file descriptor
    if (redirectIn && dup2(redirectionFd, STDIN_FILENO) < 0) {
        perror(DUP2_ERR);
        return;
    }
    // direct out to standard output
    if (dup2(STDOUT_FILENO, STDOUT_FILENO) < 0) {
        perror(DUP2_ERR);
        return;
    }
}


