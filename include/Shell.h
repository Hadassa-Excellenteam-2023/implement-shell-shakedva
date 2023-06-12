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
    std::vector<std::pair<std::string, std::string>> tokenizeCommands(const std::string&);
    void executeCommand(std::vector<std::pair<std::string, std::string>>&);
    void myJobsCommand();
    void checkBackgroundJobs();
    bool isBackgroundJob(std::vector<std::pair<std::string, std::string>>&);
    int openOutputFd(const std::string&);
    int openInputFd(const std::string&);
    bool validateCommand(const std::string&);
    bool parseOutputRedirection(std::string&, std::string&, int &);
    bool parseInputRedirection(std::string&, std::string&, int &);
    std::string trim(const std::string &);
    static std::string addPathBeginning(std::string&);
    std::vector <Job> _myJobs;
};