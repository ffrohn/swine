<head>
    <title>SwInE</title>
    <style>
        p {text-align: justify;}
    </style>
</head>

SwInE (**S**MT **w**ith **In**teger **E**xponentiation) is an SMT solver with support for integer exponentation.
To handle integer exponentation, it uses *counterexample-guided abstraction refinement*.
More precisely, it abstracts integer exponentiation with an uninterpreted function, inspects the models that are found by an underlying SMT solver with support for non-linear integer arithmetic and uninterpreted functions, and computes lemmas to eliminate those models if they violate the semantics of exponentiation.

There are two implementations of SwInE:

* The original implementation [SwInE 1](https://github.com/ffrohn/swine) is built on top of [SMT-Switch](https://github.com/stanford-centaur/smt-switch) and uses the SMT solvers [Z3](https://github.com/Z3Prover/z3/) and [CVC5](https://cvc5.github.io/) as backends.
* The more efficient re-implementation [SwInE 2](https://github.com/ffrohn/swine-z3) is built directly on top of Z3.

# News

* **29.09.2026:** Exponentiation has been standardized in [SMT-LIB](https://smt-lib.org/theories-Ints.shtml)! The [current release of SwInE 2](https://github.com/ffrohn/swine-z3/releases/tag/v0.2.1) already implements the new standard.
* **04.08.2025:** [This fork](https://github.com/Rc-Cookie/swine-z3) of SwInE 2 features an implementation of a complete algorithm for a decidable fragment of integer arithmetic with exponentiation. It is based on the paper [The complexity of Presburger arithmetic with power or powers](https://arxiv.org/abs/2305.03037) by M. Benedikt, D. Chistikov, and A. Mansutti. We hope to merge it into the main development branch soon!
* **04.08.2025:** SwInE will be presented at the upcoming [SMT workshop](https://github.com/Rc-Cookie/swine-z3)!

# Downloading SwInE

* [Here](https://github.com/ffrohn/swine-z3/releases) you can find the latest releases of SwInE 2, including nightly releases.
* [Here](https://github.com/ffrohn/swine/releases) you can find the latest releases of SwInE 1.

# Input Format

## SwInE 2

SwInE 2 supports the SMT-LIB logic [QF_EIA](https://smt-lib.org/logics-all.shtml#QF_EIA), which includes a binary function symbol `**` for exponentiation.
[Here](./leading2.smt2) you can find an example.
Please use `(set-logic ALL)` to enable support for integer exponentiation.

The semantics of `**` is `s ** t = s`<sup>`t`</sup> if `s`<sup>`t`</sup> is an integer, and `s ** t = 0`, otherwise.

## SwInE 1

SwInE 1 supports an extension of the SMT-LIB logic [QF_NIA](https://smt-lib.org/logics-all.shtml#QF_NIA) with an additional binary function symbol `exp`, whose arguments have to be of sort `Int`.
[Here](./leading1.smt2) you can find an example.
Please use `(set-logic ALL)` to enable support for integer exponentiation.

The semantics of `exp` is `exp(s,t) = s`<sup>`|t|`</sup>.

# Using SwInE

Both implementations of SwInE support the flag `--help`, which provides detailed information on using SwInE.

# Build

For SwInE 2, see [here](https://github.com/ffrohn/swine-z3).

For SwInE 1, proceed as follows:

1. think about using one of our [releases](https://github.com/ffrohn/swine/releases) instead
2. install [Docker](https://www.docker.com/)
3. go to the subdirectory `scripts`
4. execute `./build-container.sh` to initialize the Docker container that is used for building SwInE
5. execute `./build.sh` to build a statically linked binary (`build/swine`)
6. if you want to contribute to SwInE, execute `./qtcreator.sh` to start a pre-configured IDE, which runs in a Docker container as well
7. if you experience any problems, contact `florian.frohn [at] cs.rwth-aachen.de`

