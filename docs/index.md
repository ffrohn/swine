<head>
    <title>SwInE</title>
    <style>
        p {text-align: justify;}
    </style>
</head>

SwInE (**S**MT **w**ith **In**teger **E**xponentiation) is an SMT solver with support for integer exponentation.
To handle integer exponentation, it uses *counterexample guided abstraction refinement*.
More specifically, it abstracts integer exponentiation with an uninterpreted function, inspects the models that are found by an underlying SMT solver with support for non-linear integer arithmetic and uninterpreted functions, and computes lemmas to eliminate those models if they violate the semantics of exponentiation.

SwInE is build on top of [Smt-Switch](https://github.com/stanford-centaur/smt-switch) and uses the SMT solvers [Z3](https://github.com/Z3Prover/z3/), and [CVC5](https://cvc5.github.io/) as backends.

# Downloading SwInE

[Here](https://github.com/ffrohn/swine/releases) you can find the latests releases of SwInE.

# Input Format

SwInE supports an extension of the [SMTLIB-format](https://smtlib.cs.uiowa.edu/) with an additional binary function symbol `exp`, whose arguments have to be of sort `Int`.
By default, the semantics of `exp(s,t)` is s<sup>|t|</sup>.
Alternatively, SwInE supports *partial semantics* where `exp(s,t)` is treated like an uninterpreted function if `t` is negative.

# Using SwInE

Please execute `swine --help` for detailed information on using SwInE.

# Build

1. think about using one of our [releases](https://github.com/ffrohn/swine/releases) instead
2. install [Docker](https://www.docker.com/)
3. go to the subdirectory `scripts`
4. execute `./build-container.sh` to initialize the Docker container that is used for building SwInE
5. execute `./build.sh` to build a statically linked binary (`build/swine`)
6. if you want to contribute to SwInE, execute `./qtcreator.sh` to start a pre-configured IDE, which runs in a Docker container as well
7. if you experience any problems, contact `florian.frohn [at] cs.rwth-aachen.de`

