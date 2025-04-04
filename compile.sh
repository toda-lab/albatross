#!/bin/bash

#g++ -O3 -o albatross albatross.cpp parser.cpp solver.cpp upderiver.c -Lbuild/ -lcadical
g++ -DNDEBUG -O3 -o albatross albatross.cpp parser.cpp solver.cpp upderiver.c -Lbuild/ -lcadical
