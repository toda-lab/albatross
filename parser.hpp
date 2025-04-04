#ifndef cxparser_h
#define cxparser_h

#include <cstdlib>

#include "albatross.hpp"

namespace Albatross {

class Parser {
    Solver* solver;
    FILE* file;
    char curr_char;
    int lineno;

    void next (void);
    void skipWhitespace (bool no_line_break = true);
    void skipLine (void);
    void abort_unless (bool x);
    int  parseInt (bool no_line_break);
    void parseWord (std::vector<char>& name, bool no_line_break);
    void readClause (std::vector<int>& lits, bool no_line_break);
    void readVar (int& idx, std::vector<char>& name);
    void readComment (std::vector<char>& name, std::vector<int>& lits);

public:

    Parser (Solver* s, FILE* f);

    void parse_dimacs (void);
};

}

#endif
