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

FilterConstraint::FilterConstraint(Query* query, Expression* expr,
                                   cp::TriStateVar *b) :
        Constraint(query->solver(), PRIOR_LOWEST),
        store_(query->store()), expr_(expr), b_(b) {
    for(Variable* var : expr->variables())
        var->cp()->registerBind(this);
    b_->registerBind(this);
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
    if(unbound == nullptr) {
        // all variables are bound -> set truth value
        domcheck(b_->bind(expr_->evaluateEBV()));
        done_ = true;
    } else if(b_->bound()) {
        // all variables, except one, are bound -> forward checking
        TriState ebv = b_->value();
        cp::RDFVar* x = unbound->cp();
        x->clearMarks();
        unsigned n = x->size();
        const Value::id_t* dom = x->domain();
        for(unsigned i = 0; i < n; i++) {
            unbound->valueId(dom[i]);
            if(expr_->evaluateEBV() == ebv)
                x->mark(dom[i]);
        }
        domcheck(x->restrictToMarks());
        done_ = true;
    }
    return true;
}

}
