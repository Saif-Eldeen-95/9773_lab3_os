
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
void parse(std::vector<Token> &tokens)
{
    SimpleCommand *currentSimpleCommand = new SimpleCommand();
    if (tokens.empty() || tokens[0].type == TOKEN_EOF)             // if no arguments (only EOF token), return
    return;
    size_t i = 0;
  while (i < tokens.size() && tokens[i].type != TOKEN_EOF)
    {
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
        // ** normal arguments **
        currentSimpleCommand->insertArgument(strdup(tokens[i].value.c_str())); //strdup de bet7wel el string le char* 3ashan el insertArgument bta5od const char* 
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

void Command::execute()
{
    print();
    /*
        Write your code here
    */

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
    Command::_currentCommand.prompt();
    return 0;
}
