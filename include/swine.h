#pragma once

#include <smt-switch/solver.h>
#include <boost/multiprecision/cpp_int.hpp>

#include "config.h"

namespace swine {

std::ostream& operator<<(std::ostream &s, const LemmaKind kind);

class ExpGroup;
class Util;

class Swine: public smt::AbsSmtSolver {

private:

    struct Frame {
        smt::UnorderedTermSet exps;
        std::vector<std::shared_ptr<ExpGroup>> exp_groups;
        // mapping from assumption literals to the corresponding formulas for unsat cores
        smt::UnorderedTermMap assumptions;
        // mapping from preprocessed to original assertions
        smt::UnorderedTermMap preprocessed_assertions;
        std::unordered_map<smt::Term, LemmaKind> lemmas;
        std::unordered_map<smt::Term, smt::TermVec> bounding_lemmas;
        bool has_overflow;
        bool assert_failed;
    };

    struct EvaluatedExponential {
        smt::Term exp_expression;
        boost::multiprecision::cpp_int exp_expression_val;
        boost::multiprecision::cpp_int expected_val;
        smt::Term base;
        boost::multiprecision::cpp_int base_val;
        smt::Term exponent;
        long long exponent_val;
    };

    friend std::ostream& operator<<(std::ostream &s, const EvaluatedExponential &exp);

    struct Statistics {
        unsigned int iterations {0};
        unsigned int symmetry_lemmas {0};
        unsigned int bounding_lemmas {0};
        unsigned int modulo_lemmas {0};
        unsigned int interpolation_lemmas {0};
        unsigned int monotonicity_lemmas {0};
        unsigned int num_assertions {0};
        bool non_constant_base {false};
    };

    friend std::ostream& operator<<(std::ostream &s, const Swine::Statistics &stats);

    struct Interpolant {
        smt::Term t;
        boost::multiprecision::cpp_int factor;
    };

    Statistics stats;
    const Config config;
    std::unique_ptr<Util> util;
    std::vector<Frame> frames;
    std::unordered_map<smt::Term, std::vector<std::pair<boost::multiprecision::cpp_int, long long>>> interpolation_points;
    smt::SmtSolver solver;
    smt::UnorderedTermSet symbols;

    smt::Result check_sat(smt::TermVec assumptions);
    void base_symmetry_lemmas(const smt::Term e, smt::TermVec &lemmas);
    void exp_symmetry_lemmas(const smt::Term e, smt::TermVec &lemmas);
    void symmetry_lemmas(std::unordered_map<smt::Term, LemmaKind> &lemmas);
    void compute_bounding_lemmas(const ExpGroup &g);
    void bounding_lemmas(std::unordered_map<smt::Term, LemmaKind> &lemmas);
    std::optional<EvaluatedExponential> evaluate_exponential(const smt::Term exp_expression) const;
    Interpolant interpolate(smt::Term t, const unsigned pos, const boost::multiprecision::cpp_int x1, const boost::multiprecision::cpp_int x2);
    smt::Term interpolation_lemma(smt::Term t, const bool upper, const std::pair<boost::multiprecision::cpp_int, long long> a, const std::pair<boost::multiprecision::cpp_int, long long> bx2);
    void interpolation_lemma(const EvaluatedExponential &e, std::unordered_map<smt::Term, LemmaKind> &lemmas);
    void interpolation_lemmas(std::unordered_map<smt::Term, LemmaKind> &lemmas);
    smt::TermVec tangent_refinement(const smt::Term exponent1, const smt::Term exponent2, const smt::Term expected1, const smt::Term expected2);
    std::optional<smt::Term> monotonicity_lemma(const EvaluatedExponential &e1, const EvaluatedExponential &e2);
    void monotonicity_lemmas(std::unordered_map<smt::Term, LemmaKind> &lemmas);
    void mod_lemmas(std::unordered_map<smt::Term, LemmaKind> &lemmas);
    void verify() const;
    void brute_force();
    void add_lemma(const smt::Term lemma, const LemmaKind kind);
    std::unordered_map<smt::Term, LemmaKind> preprocess_lemmas(const std::unordered_map<smt::Term, LemmaKind> &lemmas);

public:

    Swine(const smt::SmtSolver solver, const Config &config);
    Swine(const Swine &) = delete;
    Swine & operator=(const Swine &) = delete;
    ~Swine();
    void set_opt(const std::string option, const std::string value) override;
    void set_logic(const std::string logic) override;
    void assert_formula(const smt::Term & t) override;
    smt::Result check_sat() override;
    smt::Result check_sat_assuming(const smt::TermVec & assumptions) override;
    smt::Result check_sat_assuming_list(const smt::TermList & assumptions) override;
    smt::Result check_sat_assuming_set(const smt::UnorderedTermSet & assumptions) override;
    void push(uint64_t num = 1) override;
    void pop(uint64_t num = 1) override;
    uint64_t get_context_level() const override;
    smt::Term get_value(const smt::Term & t) const override;
    smt::UnorderedTermMap get_model() const override;
    smt::UnorderedTermMap get_array_values(const smt::Term & arr,
                                      smt::Term & out_const_base) const override;
    void get_unsat_assumptions(smt::UnorderedTermSet & out) override;
    smt::Sort make_sort(const std::string name, uint64_t arity) const override;
    smt::Sort make_sort(smt::SortKind sk) const override;
    smt::Sort make_sort(smt::SortKind sk, uint64_t size) const override;
    smt::Sort make_sort(smt::SortKind sk, const smt::Sort & sort1) const override;
    smt::Sort make_sort(smt::SortKind sk,
                   const smt::Sort & sort1,
                   const smt::Sort & sort2) const override;
    smt::Sort make_sort(smt::SortKind sk,
                   const smt::Sort & sort1,
                   const smt::Sort & sort2,
                   const smt::Sort & sort3) const override;
    smt::Sort make_sort(smt::SortKind sk, const smt::SortVec & sorts) const override;
    smt::Sort make_sort(const smt::Sort & sort_con, const smt::SortVec & sorts) const override;
    smt::Sort make_sort(const smt::DatatypeDecl & d) const override;

    smt::DatatypeDecl make_datatype_decl(const std::string & s) override;
    smt::DatatypeConstructorDecl make_datatype_constructor_decl(
        const std::string s) override;
    void add_constructor(smt::DatatypeDecl & dt,
                         const smt::DatatypeConstructorDecl & con) const override;
    void add_selector(smt::DatatypeConstructorDecl & dt,
                      const std::string & name,
                      const smt::Sort & s) const override;
    void add_selector_self(smt::DatatypeConstructorDecl & dt,
                           const std::string & name) const override;
    smt::Term get_constructor(const smt::Sort & s, std::string name) const override;
    smt::Term get_tester(const smt::Sort & s, std::string name) const override;
    smt::Term get_selector(const smt::Sort & s,
                      std::string con,
                      std::string name) const override;

    smt::Term make_term(bool b) const override;
    smt::Term make_term(int64_t i, const smt::Sort & sort) const override;
    smt::Term make_term(const std::string val,
                   const smt::Sort & sort,
                   uint64_t base = 10) const override;
    smt::Term make_term(const smt::Term & val, const smt::Sort & sort) const override;
    smt::Term make_symbol(const std::string name, const smt::Sort & sort) override;
    smt::Term get_symbol(const std::string & name) override;
    smt::Term make_param(const std::string name, const smt::Sort & sort) override;
    /* build a new term */
    smt::Term make_term(smt::Op op, const smt::Term & t) const override;
    smt::Term make_term(smt::Op op, const smt::Term & t0, const smt::Term & t1) const override;
    smt::Term make_term(smt::Op op,
                   const smt::Term & t0,
                   const smt::Term & t1,
                   const smt::Term & t2) const override;
    smt::Term make_term(smt::Op op, const smt::TermVec & terms) const override;
    void reset() override;
    void reset_assertions() override;
    smt::Term substitute(const smt::Term term,
                    const smt::UnorderedTermMap & substitution_map) const override;
    void dump_smt2(std::string filename) const override;

};

std::ostream& operator<<(std::ostream &s, const Swine::Statistics &stats);

}
