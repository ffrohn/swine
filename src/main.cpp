#include "swine.h"
#include "version.h"

#include <smt-switch/cvc5_factory.h>
#include <smt-switch/z3_factory.h>
#include <smt-switch/smtlib_reader.h>
#include <boost/algorithm/string.hpp>

void version() {
    std::cout << "Build SHA: " << Version::GIT_SHA << " (" << Version::GIT_DIRTY << ")" << std::endl;
}

void argument_parsing_failed(const std::string &str) {
    throw std::invalid_argument("extra argument " + str);
}

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
            const std::string str {get_next()};
            if (boost::iequals(str, "z3")) {
                solver = smt::Z3SolverFactory::create(false);
                config.solver_kind = SolverKind::Z3;
            } else if (boost::iequals(str, "cvc5")) {
                solver = smt::Cvc5SolverFactory::create(false);
                config.solver_kind = SolverKind::CVC5;
            } else {
                throw std::invalid_argument("unknown solver " + str);
            }
        } else if (boost::iequals(argv[arg], "--validate")) {
            config.validate = true;
        } else if (boost::iequals(argv[arg], "--log")) {
            config.log = true;
        } else if (boost::iequals(argv[arg], "--stats")) {
            config.statistics = true;
        } else if (boost::iequals(argv[arg], "--version")) {
            version();
            return 0;
        } else if (boost::iequals(argv[arg], "--semantics")) {
            const std::string str {get_next()};
            if (boost::iequals(str, "total")) {
                config.semantics = Total;
            } else if (boost::iequals(str, "partial")) {
                config.semantics = Partial;
            }  else {
                throw std::invalid_argument("unknown semantics " + str);
            }
        } else if (boost::istarts_with(argv[arg], "--no-")) {
            auto found {false};
            for (const auto k: lemma_kind::values) {
                if (boost::iequals(argv[arg], "--no-" + lemma_kind::str(k))) {
                    config.deactivate(k);
                    found = true;
                    break;
                }
            }
            if (!found) {
                for (const auto k: preproc_kind::values) {
                    if (boost::iequals(argv[arg], "--no-" + preproc_kind::str(k))) {
                        config.deactivate(k);
                        found = true;
                        break;
                    }
                }
            }
            if (!found) {
                argument_parsing_failed(argv[arg]);
            }
        } else if (!input) {
            input = argv[arg];
        } else {
            argument_parsing_failed(argv[arg]);
        }
    }
    if (!solver) {
        solver = smt::Z3SolverFactory::create(false);
    }
    if (!input) {
        throw std::invalid_argument("missing input file");
    }
    SmtSolver swine {std::make_shared<Swine>(solver, config)};
    const auto res {SmtLibReader(swine).parse(*input)};
    version();
    return res;
}
