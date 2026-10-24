#include "tokenizer.h"
#include <sstream>
#include <iostream>

static TokenType classifyWord(const std::vector<Token> &tokens, const std::string &word){
    if (tokens.empty())
        return TOKEN_COMMAND;

    TokenType prev = tokens.back().type;
    if (prev == TOKEN_REDIRECT ||
        prev == TOKEN_APPEND ||
        prev == TOKEN_INPUT ||
        prev == TOKEN_ERROR ||
        prev == TOKEN_REDIRECT_AND_ERROR ||
        prev == TOKEN_COMMAND ||
        prev == TOKEN_ARGUMENT ||
        word[0] == '-' || word.find('/') != std::string::npos || word.find('.') != std::string::npos)
    {
        return TOKEN_ARGUMENT;
    }
    return TOKEN_COMMAND;
}

static bool isSpecial(char c){
    return c == '|' || c == '<' || c == '>' || c == '&';
}

std::vector<Token> tokenize(const std::string &input){
    std::vector<Token> tokens;
    size_t i = 0;
    size_t n = input.size();

    while (i < n){
        char c = input[i];
        if (isspace((unsigned char)c)){                         // skip spaces 
            i++;
            continue;
        }

        // ** special symbols (spliting tokens even without spaces) **
        
        if (c == '|'){
            tokens.push_back({TOKEN_PIPE, "|"});
            i++;
            continue;
        }

        if (c == '<'){
            tokens.push_back({TOKEN_INPUT, "<"});
            i++;
            continue;
        }

        if (c == '&'){
            tokens.push_back({TOKEN_BACKGROUND, "&"});
            i++;
            continue;
        }

        if (c == '>'){
            // ** ( correcte condition order ) longest match first: >>& then >> then > **
            if (input.compare(i, 3, ">>&") == 0){
                tokens.push_back({TOKEN_REDIRECT_AND_ERROR, ">>&"});
                i += 3;
            }
            else if (input.compare(i, 2, ">>") == 0){
                tokens.push_back({TOKEN_APPEND, ">>"});
                i += 2;
            }
            else{
                tokens.push_back({TOKEN_REDIRECT, ">"});
                i++;
            }
            continue;
        }

        // ** "2>" only counts when it starts a word (so file2> is not an error redirect) **
        if (c == '2' && i + 1 < n && input[i + 1] == '>'){
            tokens.push_back({TOKEN_ERROR, "2>"});
            i += 2;
            continue;
        }

        // ** normal word (may contain quoted parts) **
        std::string word;
        while (i < n && !isspace((unsigned char)input[i]) && !isSpecial(input[i])){
            if (input[i] == '"' || input[i] == '\''){
                char quote = input[i];
                i++;                                                               // skip the opening quote
                while (i < n && input[i] != quote){
                    word += input[i];
                    i++;
                }
                if (i >= n){
                    std::cerr << "Error: unmatched quote." << std::endl;
                    tokens.clear();
                    tokens.push_back({TOKEN_EOF, ""});
                    return tokens;
                }
                i++;                                                               // skip the closing quote
            }
            else{
                word += input[i];
                i++;
            }
        }
        Token token;
        token.type = classifyWord(tokens, word);
        token.value = word;
        tokens.push_back(token);
    }
    tokens.push_back({TOKEN_EOF, ""});
    return tokens;
}
