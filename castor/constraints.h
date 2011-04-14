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
    BoundConstraint(VarInt *x) : x(x) {}
    bool post() { return x->remove(0); }
};

/**
 * Statement constraint
 */
class StatementConstraint : public Constraint {
    Store *store; //!< The store containing the triples
    StatementPattern stmt; //!< The statement pattern
    /**
     * CP variables corresponding to the parts of the statement pattern or
     * NULL if the part is a fixed value.
     */
    VarInt *subject, *predicate, *object;
public:
    StatementConstraint(Query *query, StatementPattern &stmt);
    bool propagate();
};

/**
 * Generic filter constraint
 */
class FilterConstraint : public Constraint {
    Store *store; //!< The store containing the values
    Expression *expr; //!< The expression
public:
    FilterConstraint(Store *store, Expression *expr);
    bool propagate();
};

}

#endif // CASTOR_CONSTRAINTS_H
