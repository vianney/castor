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
#ifndef CASTOR_CONSTRAINTS_TRIPLE_H
#define CASTOR_CONSTRAINTS_TRIPLE_H

#include "config.h"
#include "solver/constraint.h"
#include "query.h"
#include "pattern.h"

#ifdef CASTOR_CSTR_TIMING
#include <sys/time.h>
#include <sys/resource.h>
#endif

namespace castor {

/**
 * Statement constraint
 */
class TripleConstraint : public cp::StatelessConstraint {
public:
    TripleConstraint(Query* query, TriplePattern pat);
    bool propagate();

#ifdef CASTOR_CSTR_TIMING
    static long time[3];
    static long count[3];
#endif

private:
    Store* store_; //!< The store containing the triples
    TriplePattern pat_; //!< The triple pattern
    /**
     * CP variables corresponding to the components of the triple pattern or
     * nullptr if the component is a fixed value.
     */
    cp::RDFVar* x_[TriplePattern::COMPONENTS];

#ifdef CASTOR_CSTR_TIMING
    static void addtime(int index, rusage &start);
#endif
};

}

#endif // CASTOR_CONSTRAINTS_TRIPLE_H
