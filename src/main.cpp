#include "swine.h"

#include <smt-switch/cvc5_factory.h>
#include <smt-switch/z3_factory.h>
//#include "smt-switch/yices2_factory.h"
#include <smt-switch/smtlib_reader.h>
#include <boost/algorithm/string.hpp>

int main(int argc, char *argv[]) {
    int arg = 0;
    auto get_next = [&]() {
        if (arg < argc-1) {
            return argv[++arg];
        } else {
            std::cout << "Error: Argument missing for " << argv[arg] << std::endl;
            exit(1);
        }
    };
    SmtSolver solver;
    Config config;
    std::optional<std::string> input;
    while (++arg < argc) {
        if (boost::iequals(argv[arg], "--solver")) {
            const std::string solver_str {get_next()};
            if (boost::iequals(solver_str, "z3")) {
                solver = smt::Z3SolverFactory::create(false);
                config.solver_kind = SolverKind::Z3;
            } else if (boost::iequals(solver_str, "cvc5")) {
                solver = smt::Cvc5SolverFactory::create(false);
                config.solver_kind = SolverKind::CVC5;
                //            } else if (boost::iequals(solver_str, "yices")) {
                //                solver = smt::Yices2SolverFactory::create(false);
                //                solver_kind = SolverKind::Yices;
            } else {
                throw std::invalid_argument("unknown solver " + solver_str);
            }
        } else if (boost::iequals(argv[arg], "--validate")) {
            config.validate = true;
        } else if (boost::iequals(argv[arg], "--log")) {
            config.log = true;
        } else if (boost::iequals(argv[arg], "--stats")) {
            config.statistics = true;
        } else if (!input) {
            input = argv[arg];
        } else {
            throw std::invalid_argument("extra argument " + std::string(argv[arg]));
        }
    }
    if (!solver) {
        solver = smt::Z3SolverFactory::create(false);
    }
    if (!input) {
        throw std::invalid_argument("missing input file");
    }
    SmtSolver swine {std::make_shared<Swine>(solver, config)};
    return SmtLibReader(swine).parse(*input);
}
