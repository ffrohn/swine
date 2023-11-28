#include "constant_propagator.h"

struct RelOp {

    PrimOp op;
    virtual bool eval(const cpp_int x, const cpp_int y) const  = 0;

    RelOp(const PrimOp op): op(op) {}

};

struct GeOp: public RelOp {

    bool eval(const cpp_int x, const cpp_int y) const override {
        return x >= y;
    }

    GeOp(): RelOp(Ge) {}

};

struct GtOp: public RelOp {

    bool eval(const cpp_int x, const cpp_int y) const override {
        return x > y;
    }

    GtOp(): RelOp(Gt) {}

};

struct LeOp: public RelOp {

    bool eval(const cpp_int x, const cpp_int y) const override {
        return x <= y;
    }

    LeOp(): RelOp(Le) {}

};

struct LtOp: public RelOp {

    bool eval(const cpp_int x, const cpp_int y) const override {
        return x < y;
    }

    LtOp(): RelOp(Lt) {}

};

ConstantPropagator::ConstantPropagator(Util &util): util(util) {}

Term ConstantPropagator::propagate(Term expression) const {
    static GeOp ge;
    static GtOp gt;
    static LeOp le;
    static LtOp lt;
    static std::vector<RelOp*> rel_ops {&ge, &gt, &le, &lt};
    if (!util.is_app(expression)) {
        return expression;
    } else {
        TermVec children;
        bool ground {true};
        for (const auto &c: expression) {
            children.push_back(propagate(c));
            ground &= children.back()->is_value();
        }
        const auto op {expression->get_op()};
        switch (op.prim_op) {
        case Plus: {
            TermVec non_ground;
            cpp_int ground {0};
            auto num_ground {0};
            for (auto it = children.begin(); it != children.end();) {
                const auto c {*it};
                if (c->is_value()) {
                    const auto val {util.value(c)};
                    if (val == 0) {
                        it = children.erase(it);
                        continue;
                    }
                    ground += val;
                    ++num_ground;
                } else {
                    non_ground.push_back(c);
                }
                ++it;
            }
            if (children.size() == 1) {
                return *children.begin();
            } else if (non_ground.empty()) {
                return util.term(ground);
            } else if (num_ground <= 1) {
                return util.solver->make_term(op, children);
            } else {
                non_ground.push_back(util.term(ground));
                return util.solver->make_term(op, non_ground);
            }
        }
        case Mult: {
            TermVec non_ground;
            cpp_int ground {1};
            auto num_ground {0};
            for (auto it = children.begin(); it != children.end();) {
                const auto c {*it};
                if (c->is_value()) {
                    const auto val {util.value(c)};
                    if (val == 0) {
                        return util.term(0);
                    } else if (val == 1) {
                        it = children.erase(it);
                        continue;
                    }
                    ground *= val;
                    ++num_ground;
                } else {
                    non_ground.push_back(c);
                }
                ++it;
            }
            if (children.size() == 1) {
                return *children.begin();
            } else if (non_ground.empty()) {
                return util.term(ground);
            } else if (num_ground <= 1) {
                return util.solver->make_term(op, children);
            } else {
                non_ground.push_back(util.term(ground));
                return util.solver->make_term(op, non_ground);
            }
        }
        case Minus: {
            if (ground) {
                auto it {children.begin()};
                auto res {util.value(*it)};
                for (++it; it != children.end(); ++it) {
                    res -= util.value(*it);
                }
                return util.term(res);
            }
            break;
        }
        case Ge:
        case Gt:
        case Le:
        case Lt: {
            if (ground) {
                for (const auto &o: rel_ops) {
                    if (expression->get_op() == o->op) {
                        auto it {children.begin()};
                        auto last {util.value(*it)};
                        auto res {true};
                        for (++it; it != children.end(); ++it) {
                            const auto current {util.value(*it)};
                            res &= o->eval(last, current);
                        }
                        return util.solver->make_term(res);
                    }
                }
            }
            break;
        }
        case Pow: {
            if (children.size() != 2) {
                throw std::invalid_argument("Pow must have exactly two arguments.");
            }
            if (ground) {
                return util.term(pow(util.value(children.at(0)), stol(util.value(children.at(1)).str())));
            } else if (children.at(0) == util.term(1) || children.at(1) == util.term(0)) {
                return util.term(1);
            } else if (children.at(1) == util.term(1)) {
                return children.at(0);
            }
            break;
        }
        case Apply: {
            if (util.is_abstract_exp(expression)) {
                if (children.at(1)->is_value() && children.at(2)->is_value()) {
                    const auto exponent {util.value(children.at(2))};
                    if (exponent >= 0 || util.config.semantics == Semantics::Total) {
                        return util.term(pow(util.value(children.at(1)), abs(stol(exponent.str()))));
                    }
                }
            }
            break;
        }
        case Mod: {
            if (children.size() != 2) {
                throw std::invalid_argument("Mod must have exactly two arguments.");
            }
            if (ground) {
                auto fst {util.value(children.at(0))};
                auto snd {util.value(children.at(1))};
                if (fst >= 0 && snd >= 0) {
                    return util.term(fst % snd);
                }
            }
            break;
        }
        case And: {
            TermVec new_children;
            for (const auto &c: children) {
                if (c == util.False) {
                    return util.False;
                } else if (c != util.True) {
                    new_children.push_back(c);
                }
            }
            if (new_children.empty()) {
                return util.True;
            } else if (new_children.size() == 1) {
                return new_children.front();
            } else {
                return util.solver->make_term(op, new_children);
            }
        }
        case Or: {
            TermVec new_children;
            for (const auto &c: children) {
                if (c == util.True) {
                    return util.True;
                } else if (c != util.False) {
                    new_children.push_back(c);
                }
            }
            if (new_children.empty()) {
                return util.False;
            } else if (new_children.size() == 1) {
                return new_children.front();
            } else {
                return util.solver->make_term(op, new_children);
            }

        }
        case Implies: {
            if (children.size() == 2) {
                if (children.at(0) == util.True) {
                    return children.at(1);
                } else if (children.at(1) == util.False || children.at(1) == util.True) {
                    return util.True;
                } else if (children.at(1) == util.False) {
                    return children.at(0);
                }
            }
            break;
        }
        default: break;
        }
        return util.solver->make_term(op, children);
    }
}
