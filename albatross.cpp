#include <iostream>
#include <cassert>


#include "albatross.hpp"


int main (int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " dimacs.cnf" << std::endl;
        exit(EXIT_FAILURE);
    }
    FILE* in = fopen(argv[1], "rb");
    if (in == NULL) {
        std::cerr << "ERROR: Could not open: " << argv[1] << std::endl;
        exit(EXIT_FAILURE);
    }

    Albatross::Solver solver;
    solver.read_dimacs(in);
    fclose(in);

    int res;
    int y = solver.ovars[0];
    std::vector<int> model, clause;

    while (true) {
        res = solver.solve();
        if (res == CaDiCaL::UNSATISFIABLE) {
            std::cout << "s UNSATISFIABLE" << std::endl;
            break;
        }
        solver.read_model(model);
        if (model[y] == 1) {
            std::cout << "s SATISFIABLE" << std::endl;
            solver.print_model(std::cout, model);
            break;
        }
        solver.print_assignment(std::cout, model);
        solver.derive(clause, model);
        solver.print_derived(std::cout, clause);
        if (clause.size() == 0) {
            std::cout << "s UNSATISFIABLE" << std::endl;
            break;
        }
        solver.clause(clause);
    }

    solver.print_elapsed (std::cout);
    return 0;
}
