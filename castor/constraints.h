/* This file is part of Castor
 *
 * Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
 * Copyright (C) 2010-2011, Université catholique de Louvain
 *
 * Castor is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * Castor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Castor; if not, see <http://www.gnu.org/licenses/>.
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
    cp::RDFVar *x;
public:
    BoundConstraint(cp::RDFVar *x) : Constraint(PRIOR_HIGH), x(x) {}
    bool post() { return x->remove(0); }
};

/**
 * Restrict domain to a specified range
 */
class InRangeConstraint : public cp::Constraint {
    cp::RDFVar *x;
    ValueRange rng;
public:
    InRangeConstraint(cp::RDFVar *x, ValueRange rng)
        : Constraint(PRIOR_HIGH), x(x), rng(rng) {}
    bool post() {
        x->clearMarks();
        for(Value::id_t id : rng)
            x->mark(id);
        return x->restrictToMarks();
    }
};

/**
 * Restrict domain to a set of ranges
 */
class InRangesConstraint : public cp::Constraint {
    cp::RDFVar *x;
    ValueRange *ranges;
    unsigned nbRanges;
public:
    InRangesConstraint(cp::RDFVar *x, ValueRange ranges[], unsigned nbRanges)
        : Constraint(PRIOR_HIGH), x(x), nbRanges(nbRanges) {
        this->ranges = new ValueRange[nbRanges];
        memcpy(this->ranges, ranges, nbRanges * sizeof(ValueRange));
    }
    ~InRangesConstraint() {
        delete [] ranges;
    }
    bool post() {
        x->clearMarks();
        for(unsigned i = 0; i < nbRanges; i++) {
            for(Value::id_t id : ranges[i])
                x->mark(id);
        }
        return x->restrictToMarks();
    }
};

/**
 * Restrict domain to values that are comparable in SPARQL filters
 * (i.e., simple literals, typed strings, booleans, numbers and dates)
 */
class ComparableConstraint : public InRangeConstraint {
public:
    ComparableConstraint(Store *store, cp::RDFVar *x) : InRangeConstraint(x,
        store->getClassValues(Value::CLASS_SIMPLE_LITERAL,
                              Value::CLASS_DATETIME)) {}
};

/**
 * Remove a specified range from a domain
 */
class NotInRangeConstraint : public cp::Constraint {
    cp::RDFVar *x;
    ValueRange rng;
public:
    NotInRangeConstraint(cp::RDFVar *x, ValueRange rng)
        : Constraint(PRIOR_HIGH), x(x), rng(rng) {}
    bool post() {
        for(Value::id_t id : rng) {
            if(!x->remove(id))
                return false;
        }
        return true;
    }
};

/**
 * x >= v
 */
class ConstGEConstraint : public cp::Constraint {
    cp::RDFVar *x;
    Value::id_t v;
public:
    ConstGEConstraint(cp::RDFVar *x, Value::id_t v)
        : Constraint(PRIOR_HIGH), x(x), v(v) {}
    bool post() { return x->updateMin(v); }
};

/**
 * x <= v
 */
class ConstLEConstraint : public cp::Constraint {
    cp::RDFVar *x;
    Value::id_t v;
public:
    ConstLEConstraint(cp::RDFVar *x, Value::id_t v)
        : Constraint(PRIOR_HIGH), x(x), v(v) {}
    bool post() { return x->updateMax(v); }
};

/**
 * Statement constraint
 */
class TripleConstraint : public cp::StatelessConstraint {
    Store *store; //!< The store containing the triples
    TriplePattern pat; //!< The triple pattern
    /**
     * CP variables corresponding to the components of the triple pattern or
     * nullptr if the component is a fixed value.
     */
    cp::RDFVar *x[TriplePattern::COMPONENTS];
public:
    TripleConstraint(Query *query, TriplePattern pat);
    void restore();
    bool propagate();
};

/**
 * Generic filter constraint
 */
class FilterConstraint : public cp::StatelessConstraint {
    Store *store; //!< The store containing the values
    Expression *expr; //!< The expression
public:
    FilterConstraint(Store *store, Expression *expr);
    void restore();
    bool propagate();
};

/**
 * Variables must take values of the same classes.
 */
class SameClassConstraint : public cp::StatelessConstraint {
    Store *store;
    cp::RDFVar *x1, *x2;
public:
    SameClassConstraint(Store *store, cp::RDFVar *x1, cp::RDFVar *x2);
    void restore();
    bool propagate();
};

/**
 * Variable difference constraint x1 != x2
 */
class VarDiffConstraint : public cp::StatelessConstraint {
    Store *store;
    cp::RDFVar *x1, *x2;
public:
    VarDiffConstraint(Store *store, cp::RDFVar *x1, cp::RDFVar *x2);
    void restore();
    bool propagate();
};

/**
 * Variable equality constraint x1 = x2
 */
class VarEqConstraint : public cp::Constraint {
    Store *store;
    cp::RDFVar *x1, *x2;
    int s1, s2; //!< previous size of the domain
public:
    VarEqConstraint(Store *store, cp::RDFVar *x1, cp::RDFVar *x2);
    void restore();
    bool post();
    bool propagate();
};

/**
 * Variable inequality constraint x1 {<,<=} x2
 */
class VarLessConstraint : public cp::StatelessConstraint {
    Store *store;
    cp::RDFVar *x1, *x2;
    bool equality; //!< true for <=, false for <
public:
    VarLessConstraint(Store *store, cp::RDFVar *x1, cp::RDFVar *x2, bool equality);
    void restore();
    bool propagate();
};

/**
 * Variable difference in sameTerm sense
 */
class VarDiffTermConstraint : public cp::StatelessConstraint {
    cp::RDFVar *x1, *x2;
public:
    VarDiffTermConstraint(cp::RDFVar *x1, cp::RDFVar *x2);
    void restore();
    bool propagate();
};

/**
 * Variable equality in sameTerm sense
 */
class VarSameTermConstraint : public cp::Constraint {
    cp::RDFVar *x1, *x2;
    int s1, s2; //!< previous size of the domain
public:
    VarSameTermConstraint(cp::RDFVar *x1, cp::RDFVar *x2);
    void restore();
    bool post();
    bool propagate();
};

}

#endif // CASTOR_CONSTRAINTS_H
