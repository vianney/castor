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
#ifndef CASTOR_CONSTRAINTS_FALLBACK_H
#define CASTOR_CONSTRAINTS_FALLBACK_H

#include "solver/constraint.h"
#include "query.h"
#include "expression.h"

namespace castor {

/**
 * Generic filter constraint
 */
class FilterConstraint : public cp::Constraint {
public:
    FilterConstraint(Query* query, Expression* expr);
    bool propagate();

private:
    Store*      store_; //!< The store containing the values
    Expression* expr_;  //!< The expression
};

}

#endif // CASTOR_CONSTRAINTS_FALLBACK_H
