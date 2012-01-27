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
#ifndef CASTOR_BNBORDER_H
#define CASTOR_BNBORDER_H

#include "solver/constraint.h"
#include "query.h"

namespace castor {

class Query;

/**
 * Branch&Bound ORDER static constraint
 */
class BnBOrderConstraint : public cp::Constraint {
public:
    BnBOrderConstraint(Query *query);
    ~BnBOrderConstraint();

    /**
     * Update the bound to sol
     */
    void updateBound(Solution *sol);

    /**
     * Clear the bound.
     */
    void reset();

    bool propagate();

private:
    Query *query;
    Solution *bound; //!< the current bound
    Value *boundOrderVals; //!< the evaluated ordering expressions given bound
    bool *boundOrderError; //!< has an error occured while evaluating the ordering expression?
};

}

#endif // CASTOR_BNBORDER_H
