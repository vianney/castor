/* This file is part of Castor
 *
 * Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
 * Copyright (C) 2010-2012, Université catholique de Louvain
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef CASTOR_CONSTRAINTS_H
#define CASTOR_CONSTRAINTS_H

#include "solver/constraint.h"
#include "query.h"
#include "expression.h"
#include "pattern.h"

namespace castor {

/**
 * Always-false constraint
 */
class FalseConstraint : public cp::Constraint {
public:
    FalseConstraint() : Constraint(PRIOR_HIGH) {}
    bool post() { return false; }
};

/**
 * Ensure a SPARQL variable is bound by removing value 0 from the CP domain
 */
class BoundConstraint : public cp::Constraint {
public:
    BoundConstraint(cp::RDFVar* x) : Constraint(PRIOR_HIGH), x_(x) {}
    bool post() { return x_->remove(0); }

private:
    cp::RDFVar* x_;
};

/**
 * Restrict domain to a specified range
 */
class InRangeConstraint : public cp::Constraint {
public:
    InRangeConstraint(cp::RDFVar* x, ValueRange rng)
        : Constraint(PRIOR_HIGH), x_(x), rng_(rng) {}
    bool post() {
        x_->clearMarks();
        for(Value::id_t id : rng_)
            x_->mark(id);
        return x_->restrictToMarks();
    }

private:
    cp::RDFVar* x_;
    ValueRange rng_;
};

/**
 * Restrict domain to a set of ranges
 */
class InRangesConstraint : public cp::Constraint {
public:
    InRangesConstraint(cp::RDFVar* x, std::initializer_list<ValueRange> ranges)
        : Constraint(PRIOR_HIGH), x_(x), ranges_(ranges) {}
    bool post() {
        x_->clearMarks();
        for(ValueRange rng : ranges_) {
            for(Value::id_t id : rng)
                x_->mark(id);
        }
        return x_->restrictToMarks();
    }

private:
    cp::RDFVar* x_;
    std::initializer_list<ValueRange> ranges_;
};

/**
 * Restrict domain to values that are comparable in SPARQL filters
 * (i.e., simple literals, typed strings, booleans, numbers and dates)
 */
class ComparableConstraint : public InRangeConstraint {
public:
    ComparableConstraint(Store* store, cp::RDFVar* x) : InRangeConstraint(x,
        store->range(Value::CAT_SIMPLE_LITERAL,
                              Value::CAT_DATETIME)) {}
};

/**
 * Remove a specified range from a domain
 */
class NotInRangeConstraint : public cp::Constraint {
public:
    NotInRangeConstraint(cp::RDFVar* x, ValueRange rng)
        : Constraint(PRIOR_HIGH), x_(x), rng_(rng) {}
    bool post() {
        for(Value::id_t id : rng_) {
            if(!x_->remove(id))
                return false;
        }
        return true;
    }

private:
    cp::RDFVar* x_;
    ValueRange rng_;
};

/**
 * x >= v
 */
class ConstGEConstraint : public cp::Constraint {
public:
    ConstGEConstraint(cp::RDFVar* x, Value::id_t v)
        : Constraint(PRIOR_HIGH), x_(x), v_(v) {}
    bool post() { return x_->updateMin(v_); }

private:
    cp::RDFVar* x_;
    Value::id_t v_;
};

/**
 * x <= v
 */
class ConstLEConstraint : public cp::Constraint {
public:
    ConstLEConstraint(cp::RDFVar* x, Value::id_t v)
        : Constraint(PRIOR_HIGH), x_(x), v_(v) {}
    bool post() { return x_->updateMax(v_); }

private:
    cp::RDFVar* x_;
    Value::id_t v_;
};

/**
 * Statement constraint
 */
class TripleConstraint : public cp::StatelessConstraint {
public:
    TripleConstraint(Query* query, TriplePattern pat);
    void restore();
    bool propagate();

private:
    Store* store_; //!< The store containing the triples
    TriplePattern pat_; //!< The triple pattern
    /**
     * CP variables corresponding to the components of the triple pattern or
     * nullptr if the component is a fixed value.
     */
    cp::RDFVar* x_[TriplePattern::COMPONENTS];
};

/**
 * Generic filter constraint
 */
class FilterConstraint : public cp::StatelessConstraint {
public:
    FilterConstraint(Store* store, Expression* expr);
    void restore();
    bool propagate();

private:
    Store*      store_; //!< The store containing the values
    Expression* expr_;  //!< The expression
};

/**
 * Variables must take values of the same classes.
 */
class SameClassConstraint : public cp::StatelessConstraint {
public:
    SameClassConstraint(Store* store, cp::RDFVar* x1, cp::RDFVar* x2);
    void restore();
    bool propagate();

private:
    Store* store_;
    cp::RDFVar* x1_;
    cp::RDFVar* x2_;
};

/**
 * Variable difference constraint x1 != x2
 */
class VarDiffConstraint : public cp::StatelessConstraint {
public:
    VarDiffConstraint(Store* store, cp::RDFVar* x1, cp::RDFVar* x2);
    void restore();
    bool propagate();

private:
    Store* store_;
    cp::RDFVar* x1_;
    cp::RDFVar* x2_;
};

/**
 * Variable equality constraint x1 = x2
 */
class VarEqConstraint : public cp::Constraint {
public:
    VarEqConstraint(Store* store, cp::RDFVar* x1, cp::RDFVar* x2);
    void restore();
    bool post();
    bool propagate();

private:
    Store* store_;
    cp::RDFVar* x1_;
    cp::RDFVar* x2_;
    unsigned s1_, s2_; //!< previous size of the domain
};

/**
 * Variable inequality constraint x1 {<,<=} x2
 */
class VarLessConstraint : public cp::StatelessConstraint {
public:
    VarLessConstraint(Store* store, cp::RDFVar* x1, cp::RDFVar* x2, bool equality);
    void restore();
    bool propagate();

private:
    Store* store_;
    cp::RDFVar* x1_;
    cp::RDFVar* x2_;
    bool equality; //!< true for <=, false for <
};

/**
 * Variable difference in sameTerm sense
 */
class VarDiffTermConstraint : public cp::StatelessConstraint {
public:
    VarDiffTermConstraint(cp::RDFVar* x1, cp::RDFVar* x2);
    void restore();
    bool propagate();

private:
    cp::RDFVar* x1_;
    cp::RDFVar* x2_;
};

/**
 * Variable equality in sameTerm sense
 */
class VarSameTermConstraint : public cp::Constraint {
public:
    VarSameTermConstraint(cp::RDFVar* x1, cp::RDFVar* x2);
    void restore();
    bool post();
    bool propagate();

private:
    cp::RDFVar* x1_;
    cp::RDFVar* x2_;
    unsigned s1_, s2_; //!< previous size of the domain
};

}

#endif // CASTOR_CONSTRAINTS_H
