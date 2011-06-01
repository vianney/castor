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
 * Ensure a SPARQL variable is bound by removing value 0 from the CP domain
 */
class BoundConstraint : public Constraint {
    VarInt *x;
public:
    BoundConstraint(VarInt *x) : Constraint(CSTR_PRIOR_HIGH), x(x) {}
    bool post() { return x->remove(0); }
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
 * Difference constraint v1 != v2
 */
class DiffConstraint : public StatelessConstraint {
    Query *query;
    VarVal v1, v2;
    VarInt *x1, *x2; //!< CP variable or NULL if fixed value
public:
    DiffConstraint(Query *query, VarVal v1, VarVal v2);
    void restore();
    bool post();
    bool propagate();
};

/**
 * Equality constraint v1 = v2
 */
class EqConstraint : public Constraint {
    Query *query;
    VarVal v1, v2;
    VarInt *x1, *x2; //!< CP variable or NULL if fixed value
    int s1, s2; //!< previous size of the domain
public:
    EqConstraint(Query *query, VarVal v1, VarVal v2);
    void restore();
    bool post();
    bool propagate();
};

/**
 * Inequality constraint v1 < v2
 */
class LessConstraint : public StatelessConstraint {
    Query *query;
    VarVal v1, v2;
    VarInt *x1, *x2; //!< CP variable or NULL if fixed value
public:
    LessConstraint(Query *query, VarVal v1, VarVal v2);
    void restore();
    bool post();
    bool propagate();
};

}

#endif // CASTOR_CONSTRAINTS_H
