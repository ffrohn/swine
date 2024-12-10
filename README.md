# SwInE -- SMT with Integer Exponentiation

SwInE is an SMT solver with support for integer exponentitaion.
In our corresponding [paper](10.1007/978-3-031-63498-7_21), the underlying approach is explained in detail.

It is based on "standard" SMT solvers without support for integer exponentiation.
This is the original implementation, where the underlying SMT solver can be exchanged via a command line flag.
[Here](https://github.com/ffrohn/swine-z3) you can find a reimplementation that directly uses the API of [Z3](https://github.com/Z3Prover/z3).
Hence, the reimplementation is more efficient and robust than the original, but the underlying solver is fixed.
