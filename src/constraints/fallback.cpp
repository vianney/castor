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
#include "fallback.h"

#include "config.h"

namespace castor {

FilterConstraint::FilterConstraint(Query* query, Expression* expr) :
        Constraint(query->solver(), PRIOR_MEDIUM),
        store_(query->store()), expr_(expr) {
    for(Variable* var : expr->variables())
        var->cp()->registerBind(this);
}

bool FilterConstraint::propagate() {
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
