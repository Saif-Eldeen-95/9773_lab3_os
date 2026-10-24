
/*
 * 
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <glob.h>
#include <wordexp.h>
#include <vector>
#include <string>
#include <cctype>
#include <iostream>
#include <sstream>
#include "command.h"
#include "tokenizer.h"

// Function to free a SimpleCommand and its arguments
static void freeSimple(SimpleCommand *sc)
{
    for (int j = 0; j < sc->_numberOfArguments; j++)
        free(sc->_arguments[j]);
    free(sc->_arguments);
    delete sc;
}

// 1 while the shell is waiting for a foreground command
static volatile sig_atomic_t waiting = 0;

// write one line in the log file for every child that terminated
static void logChild(pid_t pid)
{
    char msg[64];
    int len = snprintf(msg, sizeof(msg), "Child process %d terminated\n", (int)pid);
    int fd = open("shell.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0)
    {
        if (write(fd, msg, len) < 0)
            perror("log");
        close(fd);
    }
}

// Ctrl-C: do not kill the shell, just go to a new line (and show the prompt again)
static void sigintHandler(int sig)
{
    (void)sig;
    if (write(1, "\n", 1) < 0) {}
    if (!waiting)
        if (write(1, "myshell>", 8) < 0) {}
}

// a child terminated: collect every finished child and log it
static void sigchldHandler(int sig)
{
    (void)sig;
    pid_t pid;
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
        logChild(pid);
}

void parse(std::vector<Token> &tokens)
{
    SimpleCommand *currentSimpleCommand = new SimpleCommand();
    if (tokens.empty() || tokens[0].type == TOKEN_EOF)             // if no arguments (only EOF token), return
    return;
    size_t i = 0;
    while (i < tokens.size() && tokens[i].type != TOKEN_EOF){
        // ** TOKEN_PIPE handling ** 
        if(tokens[i].type == TOKEN_PIPE){                          // if pipe, close current command and start a new one
            if(currentSimpleCommand->_numberOfArguments == 0) {    // but check if the current command has any arguments, if not,print error and clear
                std::cerr << "Error: Missing command before pipe." << std::endl;
                freeSimple(currentSimpleCommand);
                Command::_currentCommand.clear();
                return;
            }
            Command::_currentCommand.insertSimpleCommand(currentSimpleCommand); // insert the current command into the command table 
            currentSimpleCommand = new SimpleCommand();
            i++;                                                   // skip the pipe token to read the new command in the next iteration
            continue;
        }
        // ** TOKEN_REDIRECT handling **
        if(tokens[i].type == TOKEN_REDIRECT){
            if(  i + 1 < tokens.size() && tokens[i+1].type == TOKEN_ARGUMENT){                // check if the next token is an argument (filename), if not, print error and clear
                Command::_currentCommand._outFile = strdup(tokens[i+1].value.c_str());      // redirecting output to the file specified in the next token
                i += 2;                                            // skip the redirection token and the filename token
                continue;
            }
            else{
                std::cerr << "Error: Missing filename after redirection." << std::endl;
                freeSimple(currentSimpleCommand);
                Command::_currentCommand.clear();
                return;
            }
        }
        // ** TOKEN_INPUT handling **
        if(tokens[i].type == TOKEN_INPUT){
            if(  i + 1 < tokens.size() && tokens[i+1].type == TOKEN_ARGUMENT){                // check if the next token is an argument (filename), if not, print error and clear
                Command::_currentCommand._inputFile = strdup(tokens[i+1].value.c_str());      // reading input from the file specified in the next token
                i += 2;                                            // skip the redirection token and the filename token
                continue;
            }
            else{
                std::cerr << "Error: Missing filename after input redirection." << std::endl;
                freeSimple(currentSimpleCommand);
                Command::_currentCommand.clear();
                return;
            }
        }
        // ** TOKEN_APPEND handling **
        if(tokens[i].type == TOKEN_APPEND){
            if(  i + 1 < tokens.size() && tokens[i+1].type == TOKEN_ARGUMENT){                // check if the next token is an argument (filename), if not, print error and clear
                Command::_currentCommand._outFile = strdup(tokens[i+1].value.c_str());      // appending output to the file specified in the next token
                Command::_currentCommand._append = 1;                                       // open the file in append mode
                i += 2;                                            // skip the redirection token and the filename token
                continue;
            }
            else{
                std::cerr << "Error: Missing filename after append redirection." << std::endl;
                freeSimple(currentSimpleCommand);
                Command::_currentCommand.clear();
                return;
            }
        }
        // ** TOKEN_ERROR handling **
        if(tokens[i].type == TOKEN_ERROR){
            if(  i + 1 < tokens.size() && tokens[i+1].type == TOKEN_ARGUMENT){                // check if the next token is an argument (filename), if not, print error and clear
                Command::_currentCommand._errFile = strdup(tokens[i+1].value.c_str());      // redirecting errors to the file specified in the next token
                i += 2;                                            // skip the redirection token and the filename token
                continue;
            }
            else{
                std::cerr << "Error: Missing filename after error redirection." << std::endl;
                freeSimple(currentSimpleCommand);
                Command::_currentCommand.clear();
                return;
            }
        }
        // ** TOKEN_REDIRECT_AND_ERROR handling **
        if(tokens[i].type == TOKEN_REDIRECT_AND_ERROR){
            if(  i + 1 < tokens.size() && tokens[i+1].type == TOKEN_ARGUMENT){                // check if the next token is an argument (filename), if not, print error and clear
                Command::_currentCommand._outFile = strdup(tokens[i+1].value.c_str());      // output and errors go to the same file
                Command::_currentCommand._errFile = strdup(tokens[i+1].value.c_str());      // separate copy, because clear() frees each one on its own
                Command::_currentCommand._append = 1;                                       // append mode
                Command::_currentCommand._out_error = 1;                                    // mark that errors are redirected with the output
                i += 2;                                            // skip the redirection token and the filename token
                continue;
            }
            else{
                std::cerr << "Error: Missing filename after output and error redirection." << std::endl;
                freeSimple(currentSimpleCommand);
                Command::_currentCommand.clear();
                return;
            }
        }
        // ** TOKEN_BACKGROUND handling **
        if(tokens[i].type == TOKEN_BACKGROUND){
            if(  i + 1 < tokens.size() && tokens[i+1].type != TOKEN_EOF){                // '&' must be the last token in the line, otherwise print error and clear
                std::cerr << "Error: '&' must be at the end of the command." << std::endl;
                freeSimple(currentSimpleCommand);
                Command::_currentCommand.clear();
                return;
            }
            Command::_currentCommand._background = 1;                                    // run the command in the background
            i++;                                                 // skip the '&' token
            continue;
        }
        // ** normal arguments + wildcard handling (* and ?) **
        std::string word = tokens[i].value;
        if (word.find('*') != std::string::npos || word.find('?') != std::string::npos){
            glob_t g;
            if (glob(word.c_str(), 0, NULL, &g) == 0){                    // find every file that matches
                for (size_t k = 0; k < g.gl_pathc; k++)
                    currentSimpleCommand->insertArgument(strdup(g.gl_pathv[k]));   // each file is a separate argument
                globfree(&g);
                i++;
                continue;
            }
        }
        // ** normal word, or no file matched **
        currentSimpleCommand->insertArgument(strdup(word.c_str()));      //strdup de bet7wel el string le char* 3ashan el insertArgument bta5od const char* 
        i++;
        
    }
    if(currentSimpleCommand->_numberOfArguments == 0) {    // check if the last command has any arguments, if not, print error and clear
        std::cerr << "Error: Missing command after pipe." << std::endl;
        freeSimple(currentSimpleCommand);
        Command::_currentCommand.clear();
        return;
    }
    Command::_currentCommand.insertSimpleCommand(currentSimpleCommand);
    Command::_currentCommand.execute();
}

SimpleCommand::SimpleCommand()
{
    _numberOfAvailableArguments = 5;
    _numberOfArguments = 0;
    _arguments = (char **)malloc(_numberOfAvailableArguments * sizeof(char *));
    _arguments[0] = NULL;
}

void SimpleCommand::insertArgument(char *argument)
{
    if (_numberOfAvailableArguments == _numberOfArguments + 1)
    {
        _numberOfAvailableArguments *= 2;
        _arguments = (char **)realloc(_arguments,
                                      _numberOfAvailableArguments * sizeof(char *));
    }

    _arguments[_numberOfArguments] = argument;
    _arguments[_numberOfArguments + 1] = NULL;

    _numberOfArguments++;
}

Command::Command()
{
    _numberOfAvailableSimpleCommands = 1;
    _simpleCommands = (SimpleCommand **)
        malloc(_numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));

    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _background = 0;
    _append = 0;
    _out_error = 0;
}

void Command::insertSimpleCommand(SimpleCommand *simpleCommand)
{
    if (_numberOfAvailableSimpleCommands == _numberOfSimpleCommands)
    {
        _numberOfAvailableSimpleCommands *= 2;
        _simpleCommands = (SimpleCommand **)realloc(_simpleCommands,
                                                    _numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));
    }

    _simpleCommands[_numberOfSimpleCommands] = simpleCommand;
    _numberOfSimpleCommands++;
}

void Command::clear()
{
    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            free(_simpleCommands[i]->_arguments[j]);
        }

        free(_simpleCommands[i]->_arguments);
        delete _simpleCommands[i];
    }

    if (_outFile)
    {
        free(_outFile);
    }

    if (_inputFile)
    {
        free(_inputFile);
    }

    if (_errFile)
    {
        free(_errFile);
    }

    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _background = 0;
    _append = 0;
    _out_error = 0;
}

void Command::print()
{
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        printf("  %-3d ", i);
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            printf("\"%s\" \t", _simpleCommands[i]->_arguments[j]);
        }
    }

    printf("\n\n");
    printf("  Output       Input        Error        Err&Out       Background\n");
    printf("  ------------ ------------ ------------ ------------ ------------\n");
    printf("  %-12s %-12s %-12s %-12s %-12s\n", _outFile ? _outFile : "default",
           _inputFile ? _inputFile : "default", _errFile ? _errFile : "default", _out_error == 1 ? _errFile : "default"
           ,_background ? "YES" : "NO");
    printf("\n\n");
}

void Command::execute(){
    // nothing to run
    if (_numberOfSimpleCommands == 0){
        clear();
        return;
    }

    // ** built-in commands (run inside the shell itself, no fork) **
    if (_numberOfSimpleCommands == 1){
        char *name = _simpleCommands[0]->_arguments[0];
        if (strcmp(name, "exit") == 0){
            printf("Good bye!!\n");
            exit(0);
        }

        if (strcmp(name, "cd") == 0){
            char *dir = _simpleCommands[0]->_arguments[1];     // cd [dir]
            if (dir == NULL)
                dir = getenv("HOME");                          // no dir -> go home
            if (chdir(dir) != 0)
                perror("cd");
            clear();
            return;
        }
    }

    // save the default input/output/error to restore them after the command
    int defaultIn = dup(0);
    int defaultOut = dup(1);
    int defaultErr = dup(2);

    // ** input: from a file or the default input **
    int fdIn;
    if (_inputFile){
        fdIn = open(_inputFile, O_RDONLY);                     // open() returns -1 if the file does not exist or cannot be opened
        if (fdIn < 0){
            perror(_inputFile);                                // file not found
            close(defaultIn);
            close(defaultOut);
            close(defaultErr);
            clear();
            return;
        }
    }
    else{
        fdIn = dup(defaultIn);
    }

    // ** error: to a file or the default error **
    int fdErr;
    if (_errFile){                                                     // if the user specified an error file, open it for writing (create it if it doesn't exist)
        fdErr = open(_errFile, O_WRONLY | O_CREAT | (_out_error ? O_APPEND : O_TRUNC), 0644); 
        if (fdErr < 0){
            perror(_errFile);
            fdErr = dup(defaultErr);
        }
    }
    else{                                                             // if no error file specified, use the default error
        fdErr = dup(defaultErr);
    }

    int pid = 0;
    for (int i = 0; i < _numberOfSimpleCommands; i++){
        // the input of this command
        dup2(fdIn, 0);
        close(fdIn);

        // the output of this command
        int fdOut;
        if (i == _numberOfSimpleCommands - 1){
            // last command: output goes to a file or the default output
            if (_outFile){                                       // if the user specified an output file, open it for writing (create it if it doesn't exist)
                fdOut = open(_outFile, O_WRONLY | O_CREAT | (_append ? O_APPEND : O_TRUNC), 0644);
                if (fdOut < 0){
                    perror(_outFile);
                    fdOut = dup(defaultOut);
                }
            }
            else{                                                // no output file specified, use the default output
                fdOut = dup(defaultOut);
            }
        }
        else{
            // not the last command: output goes into a pipe, the next command reads from it
            int fdPipe[2];
            if (pipe(fdPipe) < 0){
                perror("pipe");
                break;
            }
            fdOut = fdPipe[1];                        // write end of the pipe
            fdIn = fdPipe[0];                         // read end of the pipe, will be used as input for the next command
        }

        dup2(fdOut, 1);
        close(fdOut);
        dup2(fdErr, 2);

        pid = fork();
        if (pid < 0){
            perror("fork");
            break;
        }
        if (pid == 0){
            // child: close the extra copies and run the command
            close(defaultIn);
            close(defaultOut);
            close(defaultErr);
            close(fdErr);
            execvp(_simpleCommands[i]->_arguments[0], _simpleCommands[i]->_arguments);
            perror(_simpleCommands[i]->_arguments[0]);         // command not found
            _exit(1);
        }
    }

    // restore the default input/output/error of the shell
    dup2(defaultIn, 0);
    dup2(defaultOut, 1);
    dup2(defaultErr, 2);
    close(defaultIn);
    close(defaultOut);
    close(defaultErr);
    close(fdErr);

    // wait for the last command to finish if it's a foreground command
    if (!_background && pid > 0){
        waiting = 1;
        if (waitpid(pid, NULL, 0) > 0)      // if the handler already collected it, it logged it
            logChild(pid);
        waiting = 0;
    }

    clear();
}

void Command::prompt()
{
    std::string input;
    while (true)
    {
        printf("myshell>");
        fflush(stdout);
        if(!std::getline(std::cin, input)) {
            break;
        }
        std::vector<Token> tokens = tokenize(input);
        parse(tokens);
    }
}

Command Command::_currentCommand;
SimpleCommand *Command::_currentSimpleCommand;

int main()
{
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;               // do not let signals break getline

    sa.sa_handler = sigintHandler;          // Ctrl-C
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = sigchldHandler;         // child terminated
    sigaction(SIGCHLD, &sa, NULL);

    Command::_currentCommand.prompt();
    return 0;
}
