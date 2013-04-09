/* This file is part of Castor
 *
 * Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
 * Copyright (C) 2010-2013, Université catholique de Louvain
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
#ifndef CASTOR_CONSTRAINTS_DISTINCT_H
#define CASTOR_CONSTRAINTS_DISTINCT_H

#include <set>

#include "solver/constraint.h"
#include "query.h"

namespace castor {

class DistinctConstraint : public cp::Constraint {
public:
    DistinctConstraint(Query* query);
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
    public:
        LexLess(unsigned size, unsigned index = -1) :
            size_(size), index_(index) {}
        template <typename T>
        bool operator()(T* a, T* b) const;

    private:
        unsigned size_;  //!< number of values in a solution
        unsigned index_; //!< index to ignore
    };

    Query* query_;

    typedef std::multiset<Value::id_t*, LexLess> SolSet;
    SolSet* solutions_;
    SolSet** indexes_; //!< solution indexes
};

}

#endif // CASTOR_CONSTRAINTS_DISTINCT_H
