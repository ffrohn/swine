#pragma once

#include <smt-switch/solver.h>
#include <boost/multiprecision/cpp_int.hpp>

#include "preprocessor.h"
#include "exp_finder.h"
#include "config.h"
#include "term_evaluator.h"

using namespace smt;
using namespace boost::multiprecision;

enum LemmaKind {
    Symmetry, Bounding, Interpolation, Monotonicity, Modulo
};

std::ostream& operator<<(std::ostream &s, const LemmaKind kind);

class Swine: public AbsSmtSolver {

private:

    struct Frame {
        UnorderedTermSet exps;
        std::vector<ExpGroup> exp_groups;
        UnorderedTermSet symbols;
        TermVec assertions;
        TermVec preprocessed_assertions;
        std::unordered_map<Term, LemmaKind> lemmas;
        std::unordered_map<Term, TermVec> bounding_lemmas;
    };

    struct EvaluatedExponential {
        Term exp_expression;
        cpp_int exp_expression_val;
        cpp_int expected_val;
        Term base;
        cpp_int base_val;
        Term exponent;
        long exponent_val;
    };

    friend std::ostream& operator<<(std::ostream &s, const EvaluatedExponential &exp);

    struct Statistics {
        uint iterations {0};
        uint symmetry_lemmas {0};
        uint bounding_lemmas {0};
        uint modulo_lemmas {0};
        uint interpolation_lemmas {0};
        uint monotonicity_lemmas {0};
        uint num_assertions {0};
        bool non_constant_base {false};
    };

    friend std::ostream& operator<<(std::ostream &s, const Swine::Statistics &stats);

    struct Interpolant {
        Term t;
        cpp_int factor;
    };

    Statistics stats;
    Util util;
    Preprocessor preproc;
    ExpFinder exp_finder;
    TermEvaluator eval;
    std::vector<Frame> frames;
    std::unordered_map<Term, std::vector<std::pair<cpp_int, long>>> interpolation_points;
    const Config &config;

public:

    Swine(const SmtSolver solver, const Config &config);
    Swine(const Swine &) = delete;
    Swine & operator=(const Swine &) = delete;
    ~Swine(){};
    void set_opt(const std::string option, const std::string value) override;
    void set_logic(const std::string logic) override;
    void assert_formula(const Term & t) override;
    Result check_sat() override;
    Result check_sat_assuming(const TermVec & assumptions) override;
    Result check_sat_assuming_list(const TermList & assumptions) override;
    Result check_sat_assuming_set(const UnorderedTermSet & assumptions) override;
    void push(uint64_t num = 1) override;
    void pop(uint64_t num = 1) override;
    uint64_t get_context_level() const override;
    Term get_value(const Term & t) const override;
    UnorderedTermMap get_model() const override;
    UnorderedTermMap get_array_values(const Term & arr,
                                      Term & out_const_base) const override;
    void get_unsat_assumptions(UnorderedTermSet & out) override;
    Sort make_sort(const std::string name, uint64_t arity) const override;
    Sort make_sort(SortKind sk) const override;
    Sort make_sort(SortKind sk, uint64_t size) const override;
    Sort make_sort(SortKind sk, const Sort & sort1) const override;
    Sort make_sort(SortKind sk,
                   const Sort & sort1,
                   const Sort & sort2) const override;
    Sort make_sort(SortKind sk,
                   const Sort & sort1,
                   const Sort & sort2,
                   const Sort & sort3) const override;
    Sort make_sort(SortKind sk, const SortVec & sorts) const override;
    Sort make_sort(const Sort & sort_con, const SortVec & sorts) const override;
    Sort make_sort(const DatatypeDecl & d) const override;

    DatatypeDecl make_datatype_decl(const std::string & s) override;
    DatatypeConstructorDecl make_datatype_constructor_decl(
        const std::string s) override;
    void add_constructor(DatatypeDecl & dt,
                         const DatatypeConstructorDecl & con) const override;
    void add_selector(DatatypeConstructorDecl & dt,
                      const std::string & name,
                      const Sort & s) const override;
    void add_selector_self(DatatypeConstructorDecl & dt,
                           const std::string & name) const override;
    Term get_constructor(const Sort & s, std::string name) const override;
    Term get_tester(const Sort & s, std::string name) const override;
    Term get_selector(const Sort & s,
                      std::string con,
                      std::string name) const override;

    Term make_term(bool b) const override;
    Term make_term(int64_t i, const Sort & sort) const override;
    Term make_term(const std::string val,
                   const Sort & sort,
                   uint64_t base = 10) const override;
    Term make_term(const Term & val, const Sort & sort) const override;
    Term make_symbol(const std::string name, const Sort & sort) override;
    Term get_symbol(const std::string & name) override;
    Term make_param(const std::string name, const Sort & sort) override;
    /* build a new term */
    Term make_term(Op op, const Term & t) const override;
    Term make_term(Op op, const Term & t0, const Term & t1) const override;
    Term make_term(Op op,
                   const Term & t0,
                   const Term & t1,
                   const Term & t2) const override;
    Term make_term(Op op, const TermVec & terms) const override;
    void reset() override;
    void reset_assertions() override;
    Term substitute(const Term term,
                    const UnorderedTermMap & substitution_map) const override;
    void dump_smt2(std::string filename) const override;

    void add_symmetry_lemmas(const Term e);
    void compute_bounding_lemmas(const Term e);
    void bounding_lemmas(const Term e, std::unordered_map<Term, LemmaKind> &lemmas);
    std::optional<EvaluatedExponential> evaluate_exponential(const Term exp_expression) const;
    Interpolant interpolate(Term t, const unsigned pos, const cpp_int x1, const cpp_int x2);
    Term interpolation_lemma(Term t, const bool upper, const std::pair<cpp_int, long> a, const std::pair<cpp_int, long> bx2);
    void interpolation_lemma(const EvaluatedExponential &e, std::unordered_map<Term, LemmaKind> &lemmas);
    TermVec tangent_refinement(const Term exponent1, const Term exponent2, const Term expected1, const Term expected2);
    std::optional<Term> monotonicity_lemma(const EvaluatedExponential &e1, const EvaluatedExponential &e2);
    void monotonicity_lemmas(std::unordered_map<Term, LemmaKind> &lemmas);
    void mod_lemmas(std::unordered_map<Term, LemmaKind> &lemmas);
    void verify() const;
    void brute_force() const;
    void add_lemma(const Term lemma, const LemmaKind kind);

};

std::ostream& operator<<(std::ostream &s, const Swine::Statistics &stats);
