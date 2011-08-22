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
#ifndef CASTOR_DISTINCT_H
#define CASTOR_DISTINCT_H

#include "solver/constraint.h"
#include "query.h"
#include <set>

namespace castor {

class DistinctConstraint : public Constraint {
public:
    DistinctConstraint(Query *query);
    ~DistinctConstraint();

    /**
     * Add the current state of the variables as a solution.
     */
    void addSolution();

    /**
     * Clear all solutions.
     */
    void reset();

    bool propagate();

private:
    /**
     * Comparator for solutions
     */
    class LexLess {
        int size; //!< number of values in a solution
        int index; //!< index to ignore
    public:
        LexLess(int size, int index = -1) : size(size), index(index) {}
        template <typename T>
        bool operator()(T* a, T* b) const;
    };

    Query *query;

    typedef std::multiset<int*, LexLess> SolSet;
    SolSet *solutions;
    SolSet **indexes; //!< solution indexes
};

}

#endif // CASTOR_DISTINCT_H
