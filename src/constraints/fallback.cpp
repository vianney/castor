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
#include "fallback.h"

#include "config.h"

namespace castor {

FilterConstraint::FilterConstraint(Store* store, Expression* expr) :
        StatelessConstraint(CASTOR_CONSTRAINTS_FILTER_PRIORITY),
        store_(store), expr_(expr) {
    for(Variable* var : expr->variables())
        var->cp()->registerBind(this);
}

void FilterConstraint::restore() {
    done_ = true;
    int unbound = 0;
    for(Variable* var : expr_->variables()) {
        cp::RDFVar* x = var->cp();
        if(!x->contains(0) && !x->bound()) {
            unbound++;
            if(unbound > 1) {
                // more than one unbound variable, we are not done
                done_ = false;
                return;
            }
        }
    }
}

bool FilterConstraint::propagate() {
    StatelessConstraint::propagate();
    Variable* unbound = nullptr;
    for(Variable* var : expr_->variables()) {
        if(var->cp()->contains(0))
            var->valueId(0);
        else if(var->cp()->bound())
            var->valueId(var->cp()->value());
        else if(unbound)
            return true; // too many unbound variables (> 1)
        else
            unbound = var;
    }
    done_ = true;
    if(!unbound) {
        // all variables are bound -> check
        return expr_->isTrue();
    } else {
        // all variables, except one, are bound -> forward checking
        cp::RDFVar* x = unbound->cp();
        x->clearMarks();
        unsigned n = x->size();
        const Value::id_t* dom = x->domain();
        for(unsigned i = 0; i < n; i++) {
            unbound->valueId(dom[i]);
            if(expr_->isTrue())
                x->mark(dom[i]);
        }
        return x->restrictToMarks();
    }
}

}
