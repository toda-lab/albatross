#ifndef albatross_h
#define albatross_h

#include <cstdlib>
#include <cassert>
#include <vector>
#include <set>
#include <map>
#include <string>
#include <chrono>

#include "src/cadical.hpp"
#include "upderiver.h"

namespace Albatross {

static inline int lit_neg  (int lit) {return -lit;}
static inline int lit_sign (int lit) {return lit < 0? 1: 0;}
static inline int lit_var  (int lit) {return std::abs(lit);}
static inline int toLit    (int idx) {return idx;}

class Solver : public CaDiCaL::Solver {
    upderiver* deriver;
    std::chrono::system_clock::time_point start_clock;

public:
    std::vector<int> ivars;
    std::vector<int> ovars;
    std::vector<int> wvars;
    std::map<int,std::string>  names;

    Solver () {
        start_clock = std::chrono::system_clock::now();
        deriver = upderiver_new();
        upderiver_addfunc(deriver,
            Albatross::lit_neg,
            Albatross::lit_sign,
            Albatross::lit_var,
            Albatross::toLit);
    }

    virtual ~Solver () {
        upderiver_delete(deriver);
    }

    void read_dimacs(FILE *file);
    void set_variable (int idx, upderiver_var_tag tag, char name[], int n);
    void add_clause_to_deriver (std::vector<int>& clause);
    void read_model (std::vector<int>& model);
    void derive (std::vector<int>& clause, std::vector<int>& model);

    void print_elapsed (std::ostream& out);
    void print_derived (std::ostream& out, const std::vector<int>& clause);
    void print_model (std::ostream& out, const std::vector<int>& model);
    void print_assignment
        (std::ostream& out, const std::vector<int>& assign);
};

}

#endif
