#include <iostream>
#include <cmath>
#include <algorithm>

#include "albatross.hpp"
#include "parser.hpp"

namespace Albatross {


void Solver::read_dimacs (FILE *file)
{
    Parser parser(this, file);
    parser.parse_dimacs();
}

void Solver::read_model (std::vector<int>& model)
{
    const int n = vars();
    if (model.size() < n+1)
        model.resize(n+1);
    assert(model.size() == n+1);
    model[0] = 0;
    for (int i = 1; i <= n; i++)
        model[i] = val(i) > 0? 1:-1;
}

void Solver::derive (std::vector<int>& clause, std::vector<int>& model)
{
    int min_i;
    const int max_size = ivars.size();
    if (clause.size() < max_size)
        clause.resize(max_size);
    int clause_size = upderiver_derive(deriver, 
            &clause[0], max_size,
            &min_i,
            &ovars[0], 1, 
            &model[0],  model.size());
    clause.resize(clause_size);
    //sort(clause.begin(), clause.end());
}

void Solver::set_variable
    (int idx, upderiver_var_tag tag, char name[], int n)
{
    upderiver_setvar(deriver, idx, tag, name, n);

    if (tag == CECD_IVAR)
        ivars.push_back(idx);
    if (tag == CECD_OVAR)
        ovars.push_back(idx);
    if (tag == CECD_WVAR)
        wvars.push_back(idx);

    if (n > 0)
        names[idx] = std::string(name);
}

void Solver::add_clause_to_deriver (std::vector<int>& clause)
{
    upderiver_addclause(deriver, &clause[0], &clause[0]+clause.size());
}

void Solver::print_elapsed (std::ostream& out)
{
    std::chrono::system_clock::time_point curr_clock = 
        std::chrono::system_clock::now();
    double elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>
        (curr_clock-start_clock).count()/1000.0;
    out << "c elapsed(s) " << elapsed  << std::endl; 
}

void Solver::print_assignment
    (std::ostream& out, const std::vector<int>& model)
{
    out << "c assign:";
#ifdef PRINT_BY_NAME
    for (int i = 0; i < wvars.size(); i++)
        out << " " << (model[wvars[i]]>0? "":"~") << names[wvars[i]];
    std::cout << std::endl;
#else
    for (int i = 0; i < wvars.size(); i++)
        std::cout << " " << (model[wvars[i]] > 0? "":"-") << wvars[i];
    std::cout << std::endl;
#endif // PRINT_BY_NAME
}

void Solver::print_derived (std::ostream& out, const std::vector<int>& clause)
{
    out << "c derived:";
#ifdef PRINT_BY_NAME
    for (auto i = clause.begin(); i != clause.end(); i++)
        out << " " << (Albatross::lit_sign(*i)? "~":"") 
            << names[Albatross::lit_var(*i)];
    out << std::endl;
#else
    for (auto i = clause.begin(); i != clause.end(); i++)
        out << " " << (Albatross::lit_sign(*i)? "-":"") 
            << Albatross::lit_var(*i);
    out << std::endl;
#endif
}

void Solver::print_model (std::ostream& out, const std::vector<int>& model)
{
    for (int i = 1; i < model.size(); i++) {
        if (i%10 == 1)
            out << "v";
        out << " " << (model[i]>0? i: -i);
        if (i+1 == model.size())
            out << " 0";
        if (i%10 == 0 || i+1 == model.size())
            out << std::endl;
    }
}

}
