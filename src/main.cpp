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

void print_help() {
    const auto length {std::string("  --semantics [total|partial]").length()};
    std::cout << "***** SwInE -- SMT with Integer Exponentiation *****" << std::endl;
    std::cout << "usage: swine [args] input.smt2" << std::endl;
    std::cout << "valid arguments:" << std::endl;
    std::cout << "  --solver [z3|cvc5]          : choose the backend SMT solver (default: z3)" << std::endl;
    std::cout << "  --semantics [total|partial] : choose the semantics that should be considered for exp (default: total)" << std::endl;
    for (const auto k: lemma_kind::values) {
        const auto str {std::string("  --no-") + lemma_kind::str(k) + " lemmas"};
        const auto ws {length - str.length()};
        std::cout << str << std::string(ws, ' ') << " : disable " << k << " lemmas" << std::endl;
    }
    for (const auto k: preproc_kind::values) {
        const auto str {std::string("  --no-") + preproc_kind::str(k)};
        const auto ws {length - str.length()};
        std::cout << str << std::string(ws, ' ') << " : disable " << k << std::endl;
    }
    std::cout << "  --validate-sat              : validate SAT results by evaluating the input w.r.t. solution" << std::endl;
    std::cout << "  --validate-unsat c          : validate UNSAT results by forcing exponents to values in {0,...,c}, c in IN" << std::endl;
    std::cout << "  --get-lemmas                : print all lemmas that were used in the final proof if UNSAT is proven" << std::endl;
    std::cout << "  --stats                     : print statistics in the end" << std::endl;
    std::cout << "  --help                      : print this text and exit" << std::endl;
    std::cout << "  --version                   : print the SwInE version and exit" << std::endl;
    std::cout << "  --no-version                : omit the SwInE version at the end of the output" << std::endl;
    std::cout << "  --log                       : enable logging" << std::endl;
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
    Config config;
    std::optional<std::string> input;
    auto show_version {true};
    while (++arg < argc) {
        if (boost::iequals(argv[arg], "--solver")) {
            const std::string str {get_next()};
            if (boost::iequals(str, "z3")) {
                config.solver_kind = SolverKind::Z3;
            } else if (boost::iequals(str, "cvc5")) {
                config.solver_kind = SolverKind::CVC5;
            } else {
                throw std::invalid_argument("unknown solver " + str);
            }
        } else if (boost::iequals(argv[arg], "--validate-sat")) {
            config.validate_sat = true;
        } else if (boost::iequals(argv[arg], "--validate-unsat")) {
            const auto bound {std::stoi(get_next())};
            if (bound >= 0) {
                config.validate_unsat = bound;
            }
        } else if (boost::iequals(argv[arg], "--get-lemmas")) {
            config.get_lemmas = true;
        } else if (boost::iequals(argv[arg], "--log")) {
            config.log = true;
        } else if (boost::iequals(argv[arg], "--stats")) {
            config.statistics = true;
        } else if (boost::iequals(argv[arg], "--version")) {
            version();
            return 0;
        } else if (boost::iequals(argv[arg], "--help")) {
            print_help();
            return 0;
        } else if (boost::iequals(argv[arg], "--no-version")) {
            show_version = false;
        } else if (boost::iequals(argv[arg], "--semantics")) {
            const std::string str {get_next()};
            if (boost::iequals(str, "total")) {
                config.semantics = Semantics::Total;
            } else if (boost::iequals(str, "partial")) {
                config.semantics = Semantics::Partial;
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
    SmtSolver solver;
    switch (config.solver_kind) {
    case SolverKind::Z3:
        solver = smt::Z3SolverFactory::create(false);
        break;
    case SolverKind::CVC5:
        solver = smt::Cvc5SolverFactory::create(false);
        break;
    default:
        throw std::invalid_argument("unknown solver");
    }
    if (!input) {
        throw std::invalid_argument("missing input file");
    }
    SmtSolver swine {std::make_shared<Swine>(solver, config)};
    const auto res {SmtLibReader(swine).parse(*input)};
    if (show_version) {
        version();
    }
    return res;
}
