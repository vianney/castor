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
#include "expression.h"
#include "pattern.h"

namespace castor {

/**
 * Always-false constraint
 */
class FalseConstraint : public Constraint {
public:
    FalseConstraint() : Constraint(PRIOR_HIGH) {}
    bool post() { return false; }
};

/**
 * Ensure a SPARQL variable is bound by removing value 0 from the CP domain
 */
class BoundConstraint : public Constraint {
    VarInt *x;
public:
    BoundConstraint(VarInt *x) : Constraint(PRIOR_HIGH), x(x) {}
    bool post() { return x->remove(0); }
};

/**
 * Restrict domain to a specified range
 */
class InRangeConstraint : public Constraint {
    VarInt *x;
    ValueRange rng;
public:
    InRangeConstraint(VarInt *x, ValueRange rng)
        : Constraint(PRIOR_HIGH), x(x), rng(rng) {}
    bool post() {
        x->clearMarks();
        for(Value::id_t id : rng)
            x->mark(id);
        return x->restrictToMarks();
    }
};

/**
 * Restrict domain to values that are comparable in SPARQL filters
 * (i.e., simple literals, typed strings, booleans, numbers and dates)
 */
class ComparableConstraint : public InRangeConstraint {
public:
    ComparableConstraint(Store *store, VarInt *x) : InRangeConstraint(x,
        store->getClassValues(Value::CLASS_SIMPLE_LITERAL,
                              Value::CLASS_DATETIME)) {}
};

/**
 * Remove a specified range from a domain
 */
class NotInRangeConstraint : public Constraint {
    VarInt *x;
    ValueRange rng;
public:
    NotInRangeConstraint(VarInt *x, ValueRange rng)
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
class ConstGEConstraint : public Constraint {
    VarInt *x;
    Value::id_t v;
public:
    ConstGEConstraint(VarInt *x, Value::id_t v)
        : Constraint(PRIOR_HIGH), x(x), v(v) {}
    bool post() { return x->updateMin(v); }
};

/**
 * x <= v
 */
class ConstLEConstraint : public Constraint {
    VarInt *x;
    Value::id_t v;
public:
    ConstLEConstraint(VarInt *x, Value::id_t v)
        : Constraint(PRIOR_HIGH), x(x), v(v) {}
    bool post() { return x->updateMax(v); }
};

/**
 * Statement constraint
 */
class StatementConstraint : public StatelessConstraint {
    Store *store; //!< The store containing the triples
    StatementPattern stmt; //!< The statement pattern
    /**
     * CP variables corresponding to the parts of the statement pattern or
     * NULL if the part is a fixed value.
     */
    VarInt *subject, *predicate, *object;
public:
    StatementConstraint(Query *query, StatementPattern &stmt);
    void restore();
    bool propagate();
};

/**
 * Generic filter constraint
 */
class FilterConstraint : public StatelessConstraint {
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
class SameClassConstraint : public StatelessConstraint {
    Store *store;
    VarInt *x1, *x2;
public:
    SameClassConstraint(Store *store, VarInt *x1, VarInt *x2);
    void restore();
    bool propagate();
};

/**
 * Variable difference constraint x1 != x2
 */
class VarDiffConstraint : public StatelessConstraint {
    Store *store;
    VarInt *x1, *x2;
public:
    VarDiffConstraint(Store *store, VarInt *x1, VarInt *x2);
    void restore();
    bool propagate();
};

/**
 * Variable equality constraint x1 = x2
 */
class VarEqConstraint : public Constraint {
    Store *store;
    VarInt *x1, *x2;
    int s1, s2; //!< previous size of the domain
public:
    VarEqConstraint(Store *store, VarInt *x1, VarInt *x2);
    void restore();
    bool post();
    bool propagate();
};

/**
 * Variable inequality constraint x1 {<,<=} x2
 */
class VarLessConstraint : public StatelessConstraint {
    Store *store;
    VarInt *x1, *x2;
    bool equality; //!< true for <=, false for <
public:
    VarLessConstraint(Store *store, VarInt *x1, VarInt *x2, bool equality);
    void restore();
    bool propagate();
};

}

#endif // CASTOR_CONSTRAINTS_H
