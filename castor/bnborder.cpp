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
#include "bnborder.h"

#include <cassert>

namespace castor {

BnBOrderConstraint::BnBOrderConstraint(Query *query) : query(query) {
    VariableSet vars(query);
    for(unsigned i = 0; i < query->getOrderCount(); i++)
        vars += query->getOrder(i)->getVars();
    assert(vars.getSize() > 0);
    for(unsigned i = 0; i < vars.getSize(); i++) {
        VarInt *x = vars[i]->getCPVariable();
        x->registerBind(this);
    }
    boundOrderVals = new Value[query->getOrderCount()];
    boundOrderError = new bool[query->getOrderCount()];
    bound = NULL;
}

BnBOrderConstraint::~BnBOrderConstraint() {
    delete [] boundOrderVals;
    delete [] boundOrderError;
}

void BnBOrderConstraint::updateBound(Solution *sol) {
    bound = sol;
    bound->restore();
    for(unsigned i = 0; i < query->getOrderCount(); i++) {
        if(VariableExpression *varexpr = dynamic_cast<VariableExpression*>(query->getOrder(i))) {
            boundOrderVals[i].id = varexpr->getVariable()->getValueId();
            boundOrderError[i] = (boundOrderVals[i].id > 0);
        } else {
            boundOrderError[i] = !query->getOrder(i)->evaluate(boundOrderVals[i]);
            if(!boundOrderError[i])
                boundOrderVals[i].ensureInterpreted();
        }
    }
    solver->refresh(this);
}

void BnBOrderConstraint::reset() {
    bound = NULL;
}

bool BnBOrderConstraint::propagate() {
    if(bound == NULL)
        return true;
    for(unsigned i = 0; i < query->getOrderCount(); i++) {
        if(boundOrderError[i])
            return true; // don't know what to do if evaluation error
        Expression *expr = query->getOrder(i);
        bool desc = query->isOrderDescending(i);
        Value *bval = &boundOrderVals[i];
        if(VariableExpression *varexpr = dynamic_cast<VariableExpression*>(expr)) {
            VarInt *x = varexpr->getVariable()->getCPVariable();
            assert(bval->id > 0);
            if(desc) {
                if(!x->updateMin(bval->id))
                    return false;
            } else {
                if(!x->updateMax(bval->id))
                    return false;
            }
            if(i == query->getOrderCount() - 1) {
                if(!x->remove(bval->id))
                    return false;
            }
            if(!x->isBound() ||
                    (!desc && x->getValue() < bval->id) ||
                    (desc && x->getValue() > bval->id))
                return true;
        } else {
            for(unsigned j = 0; j < expr->getVars().getSize(); j++) {
                if(expr->getVars()[j]->isBound())
                    expr->getVars()[j]->setValueFromCP();
                else
                    return true;
            }
            Value val;
            if(!expr->evaluate(val))
                return true;
            val.ensureInterpreted();
            if(desc) {
                if(val < *bval)
                    return false;
                else if(val > *bval)
                    return true;
            } else {
                if(val < *bval)
                    return true;
                else if(val > *bval)
                    return false;
            }
        }
    }
    return false;
}

}
