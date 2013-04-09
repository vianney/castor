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
#ifndef CASTOR_CONSTRAINTS_TRIPLE_H
#define CASTOR_CONSTRAINTS_TRIPLE_H

#include "config.h"
#include "solver/constraint.h"
#include "query.h"
#include "pattern.h"

namespace castor {

/**
 * Triple constraint with Forward-Checking consistency.
 */
class FCTripleConstraint : public cp::Constraint {
public:
    FCTripleConstraint(Query* query, RDFVarTriple triple);
    bool propagate();

private:
    Store* store_; //!< The store containing the triples
    RDFVarTriple triple_; //!< The triple pattern
};

/**
 * Triple constraint providing extra pruning.
 *
 * The smaller the domains, the better the extra pruning will be. Thus this
 * constraint has low priority to ensure it comes last in the propagation
 * queue.
 */
class ExtraTripleConstraint : public cp::Constraint {
public:
    ExtraTripleConstraint(Query* query, RDFVarTriple triple);
    bool propagate();

private:
    Store* store_; //!< The store containing the triples
    RDFVarTriple triple_; //!< The triple pattern
};

/**
 * Triple constraint using the STR algorithm.
 */
class STRTripleConstraint : public cp::Constraint {
public:
    STRTripleConstraint(Query* query, RDFVarTriple triple);
    bool propagate();

private:
    Store* store_; //!< The store containing the triples
    RDFVarTriple triple_; //!< The triple pattern
    cp::ReversibleSet<unsigned> supports_; //!< support triples
};

}

#endif // CASTOR_CONSTRAINTS_TRIPLE_H
