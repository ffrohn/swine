#pragma once

#include <smt-switch/solver.h>
#include <boost/multiprecision/cpp_int.hpp>

#include "term_flattener.h"

using namespace smt;
using namespace boost::multiprecision;

enum SolverKind {
    Z3, CVC5, Yices
};

class Swine: public AbsSmtSolver {

    struct Frame {
        UnorderedTermSet exps;
        UnorderedTermSet symbols;
    };

    struct EvaluatedExp {
        Term exp_expression;
        cpp_int exp_expression_val;
        cpp_int expected_val;
        Term base;
        cpp_int base_val;
        Term exponent;
        long exponent_val;
    };

    SmtSolver solver;
    SolverKind solver_kind;
    Sort int_sort;
    Term exp;
    TermFlattener flattener;
    std::vector<Frame> frames;
    static const bool log {true};
    std::unordered_map<Term, std::vector<long>> secant_points;

public:

    Swine(const SmtSolver solver, const SolverKind solver_kind);
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

    void add_initial_lemmas(const Term e);
    Term term(const cpp_int &value);
    std::optional<EvaluatedExp> evaluate(const Term exp_expression) const;
    Term tangent_lemma(const EvaluatedExp &e, const bool next);
    void tangent_lemmas(const EvaluatedExp &e, TermVec &lemmas);
    Term secant_lemma(const EvaluatedExp &e, const long other_exponent_val);
    void secant_lemmas(const EvaluatedExp &e, TermVec &lemmas);
    TermVec tangent_refinement(const Term exponent1, const Term exponent2, const Term expected1, const Term expected2);
    std::optional<Term> monotonicity_lemma(const EvaluatedExp &e1, const EvaluatedExp &e2);
    void monotonicity_lemmas(TermVec &lemmas);

};
