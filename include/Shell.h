#pragma once
#include <iostream>
#include <vector>


struct Job {
    std::string command;
    std::string commandsVariables;
    pid_t pid;
    int status;
};

class Shell
{
public:
    Shell();
private:
    std::pair<std::string, std::string> tokenizeCommand(const std::string&);
    void executeCommand(std::string, std::string);
    void myJobsCommand();
    void checkBackgroundJobs();
    int openOutputFd(const std::string&);
    bool validateCommand(const std::string&);
    std::vector <Job> _myJobs;
};