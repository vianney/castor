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

FCTripleConstraint::FCTripleConstraint(Query* query, RDFVarTriple triple) :
        Constraint(query->solver(), PRIOR_MEDIUM),
        store_(query->store()), triple_(triple) {
    for(int i = 0; i < triple_.COMPONENTS; i++)
        triple_[i]->registerBind(this);
}

bool FCTripleConstraint::propagate() {
    Triple min, max;
    int unbound = -1;
    for(int i = 0; i < triple_.COMPONENTS; i++) {
        if(!triple_[i]->bound()) {
            if(unbound == -1)
                unbound = i;
            else
                return true; // too many unbound variables (> 1)
        }
        min[i] = triple_[i]->min();
        max[i] = triple_[i]->max();
    }

    Store::TripleRange q(store_, min, max);

    if(unbound == -1) {
        // all variables are bound, just check
        if(!q.next(nullptr))
            return false;
        done_ = true;
        return true;
    }

    triple_[unbound]->clearMarks();
    Triple t;
    while(q.next(&t))
        triple_[unbound]->mark(t[unbound]);
    if(!triple_[unbound]->restrictToMarks())
        return false;
    done_ = true;
    return true;
}

////////////////////////////////////////////////////////////////////////////////

ExtraTripleConstraint::ExtraTripleConstraint(Query* query, RDFVarTriple triple) :
        Constraint(query->solver(), PRIOR_LOW),
        store_(query->store()), triple_(triple) {
    for(int i = 0; i < triple_.COMPONENTS; i++)
        triple_[i]->registerBind(this);
}

bool ExtraTripleConstraint::propagate() {
    Triple min, max;
    int a = -1, b = -1;
    for(int i = 0; i < triple_.COMPONENTS; i++) {
        if(!triple_[i]->bound()) {
            if(a == -1)
                a = i;
            else if(b == -1)
                b = i;
            else
                return true; // too many unbound variables (> 2)
        }
        min[i] = triple_[i]->min();
        max[i] = triple_[i]->max();
    }

    if(a == -1 || b == -1) {
        // too few unbound variables (< 2), let FCTripleConstraint handle it
        done_ = true;
        return true;
    }

    Store::TripleRange q(store_, min, max);

    triple_[a]->clearMarks();
    triple_[b]->clearMarks();
    Triple t;
    while(q.next(&t)) {
        if(triple_[a]->contains(t[a]) && triple_[b]->contains(t[b])) {
            triple_[a]->mark(t[a]);
            triple_[b]->mark(t[b]);
        }
    }
    if(!(triple_[a]->restrictToMarks() && triple_[b]->restrictToMarks()))
        return false;
    done_ = true;
    return true;
}

}
