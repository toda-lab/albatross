# Albatross SAT Solver
# Build
- Get [CaDiCaL](https://github.com/arminbiere/cadical).
- Put all files in this repository to the top directory of CaDiCaL.
- Build CaDiCaL.
- Execute `./compile.sh` to compile albatross solver.
# Usage
```
$ ./albatross dimacs-cnf-file
```
- Albatross takes CNFs extended by complementary encoding.
- Examples of such CNFs and experimental results are available from [here](https://github.com/toda-lab/albatross_experiments_20250404).
