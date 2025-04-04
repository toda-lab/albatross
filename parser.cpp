#include <iostream>
#include <cassert>
#include <cstdlib>
#include <vector>

#include "parser.hpp"

namespace Albatross {

#define SKIP_AND_RETURN_UNLESS(x) do {if (!(x)) {skipLine(); return;}} while(0);

inline void Parser::next (void)
{
    if (curr_char == EOF) {
        std::cerr << "ERROR:" << lineno 
            << ": Reached EOF and no next char." << std::endl;
        exit(EXIT_FAILURE);
    }
#ifndef NUNLOCKED
    curr_char = getc_unlocked(file);
#else
    curr_char = getc(file);
#endif
    if (curr_char == '\n')
        lineno++;
}

Parser::Parser (Solver* s, FILE* f)
        : solver (s), file (f), lineno(1)
{
    next();
}

inline void Parser::skipWhitespace (bool no_line_break)
{
    if (no_line_break) {
        while (curr_char == '\t' || curr_char == ' ')
            next();
    } else {
        while ((curr_char >= 9 && curr_char <= 13) || curr_char == 32)
            next();
    }
}

inline void Parser::skipLine (void)
{
    while (true) {
        if (curr_char == EOF) return;
        if (curr_char == '\n') { next(); return; }
        next(); 
    }
}

inline void Parser::abort_unless (bool x)
{
    if (!x) {
        std::cerr << "ERROR:" << lineno
            << ": Unexpected char: " << curr_char << std::endl;
        exit(EXIT_FAILURE);
    }
}

inline int Parser::parseInt (bool no_line_break)
{
    skipWhitespace(no_line_break);
    bool negative = false;
    if (curr_char == '-') {
        negative = true;
        next(); 
    }
    abort_unless(isdigit(curr_char));
    int  val = 0;
    for (; isdigit(curr_char); next())
        val = val*10 + (curr_char - '0');
    abort_unless((curr_char >= 9 && curr_char <= 13) || curr_char == 32
        || curr_char == EOF);
    return negative ? -val : val;
}

inline void Parser::parseWord (std::vector<char>& name, bool no_line_break)
{
    skipWhitespace(no_line_break);
    name.clear();
    // while curr_char is neither whitespace nor EOF:
    while (!((curr_char >= 9 && curr_char <= 13) || curr_char == 32 )
        && curr_char != EOF) {
        name.push_back(curr_char);
        next();
    }
    name.push_back('\0');
    abort_unless((curr_char >= 9 && curr_char <= 13) || curr_char == 32
        || curr_char == EOF);
}

void Parser::readClause (std::vector<int>& lits, bool no_line_break)
{
    int parsed_lit;
    lits.clear();
    while ((parsed_lit = parseInt(no_line_break)) != 0)
        lits.push_back(parsed_lit);
}

void Parser::readVar (int& idx, std::vector<char>& name)
{
    // Format: variable_index 0 variable_name
    // or
    // Format: variable_index 0
    idx = parseInt(true);
    abort_unless(idx > 0);
    int tmp = parseInt(true);
    abort_unless(tmp == 0);
    parseWord(name, true);
}

void Parser::readComment (std::vector<char>& name, std::vector<int>& lits)
{
    int idx;

    abort_unless('c' == curr_char);
    next();
    SKIP_AND_RETURN_UNLESS(' ' == curr_char || '\t' == curr_char);
    while (' ' == curr_char || '\t' == curr_char) next();

    if ('I' == curr_char) {
        next();
        SKIP_AND_RETURN_UNLESS('V' == curr_char); next();
        SKIP_AND_RETURN_UNLESS('A' == curr_char); next();
        SKIP_AND_RETURN_UNLESS('R' == curr_char); next();
        SKIP_AND_RETURN_UNLESS(' ' == curr_char || '\t' == curr_char);
        readVar(idx, name);
        solver->set_variable(idx, CECD_IVAR, &name[0], name.size());
    } else if ('O' == curr_char) {
        next();
        SKIP_AND_RETURN_UNLESS('V' == curr_char); next();
        SKIP_AND_RETURN_UNLESS('A' == curr_char); next();
        SKIP_AND_RETURN_UNLESS('R' == curr_char); next();
        SKIP_AND_RETURN_UNLESS(' ' == curr_char || '\t' == curr_char);
        readVar(idx, name);
        solver->set_variable(idx, CECD_OVAR, &name[0], name.size());
    } else if ('W' == curr_char) {
        next();
        SKIP_AND_RETURN_UNLESS('V' == curr_char); next();
        SKIP_AND_RETURN_UNLESS('A' == curr_char); next();
        SKIP_AND_RETURN_UNLESS('R' == curr_char); next();
        SKIP_AND_RETURN_UNLESS(' ' == curr_char || '\t' == curr_char);
        readVar(idx, name);
        solver->set_variable(idx, CECD_WVAR, &name[0], name.size());
    }

    skipLine();
}

void Parser::parse_dimacs (void)
{
    std::vector<int>  lits;
    std::vector<char> name;

    while (true) {
        skipWhitespace(false); //allowing line break
        if (curr_char == EOF)
            break;
        else if (curr_char == 'p') {
            next();
            SKIP_AND_RETURN_UNLESS(' ' == curr_char || '\t' == curr_char); next();
            SKIP_AND_RETURN_UNLESS('c' == curr_char); next();
            SKIP_AND_RETURN_UNLESS('n' == curr_char); next();
            SKIP_AND_RETURN_UNLESS('f' == curr_char); next();
            SKIP_AND_RETURN_UNLESS(' ' == curr_char || '\t' == curr_char);
            skipLine();
        } else if (curr_char == 'c')
            readComment(name, lits);
        else {
            readClause(lits, false); // allowing line break
            solver->clause(lits);
            // add all clauses including output variables to deriver.
            // NOTE: all comment lines declaring the output variable should be 
            //       present before clause lines
            //       all clauses in complementary encoding are assumed to
            //       include the output variable.
            bool found = false;
            for (auto i = lits.begin(); i != lits.end(); i++) {
                for (auto j = solver->ovars.begin(); j != solver->ovars.end(); j++) {
                    if (std::abs(*i) == *j) {
                        found = true;
                        solver->add_clause_to_deriver(lits);
                        break;
                    }
                }
                if (found)
                    break;
            }
        }
    }
}
}
