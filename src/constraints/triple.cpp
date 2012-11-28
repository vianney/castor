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
#include "triple.h"

namespace castor {

TripleConstraint::TripleConstraint(Query* query, RDFVarTriple triple) :
        Constraint(query->solver(), CASTOR_CONSTRAINTS_STATEMENT_PRIORITY),
        store_(query->store()), triple_(triple) {
    for(int i = 0; i < triple_.COMPONENTS; i++)
        triple_[i]->registerBind(this);
}

bool TripleConstraint::propagate() {
    Triple min, max;
    int bound = triple_.COMPONENTS;
    for(int i = 0; i < triple_.COMPONENTS; i++) {
        min[i] = triple_[i]->min();
        max[i] = triple_[i]->max();
        bound -= triple_[i]->bound() ? 0 : 1;
    }

    if(bound == 0) {
        // nothing bound, we do not want to check all triples
        return true;
    }

    if(bound >= triple_.COMPONENTS - 1)
        done_ = true;

    Store::TripleRange q(store_, min, max);

    if(bound == triple_.COMPONENTS) {
        // all variables are bound, just check
        return q.next(nullptr);
    }

    for(int i = 0; i < triple_.COMPONENTS; i++)
        if(min[i] != max[i]) triple_[i]->clearMarks();
    Triple t;
    while(q.next(&t)) {
        for(int i = 0; i < triple_.COMPONENTS; i++)
            if(min[i] != max[i] && !triple_[i]->contains(t[i])) goto nextTriple;
        for(int i = 0; i < triple_.COMPONENTS; i++)
            if(min[i] != max[i]) triple_[i]->mark(t[i]);
    nextTriple:
        ;
    }
    for(int i = 0; i < triple_.COMPONENTS; i++)
        if(min[i] != max[i] && !triple_[i]->restrictToMarks()) return false;
    return true;
}

}
