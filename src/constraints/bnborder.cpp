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
#include "bnborder.h"

#include <cassert>

#include "expression.h"

namespace castor {

BnBOrderConstraint::BnBOrderConstraint(Query* query) :
        Constraint(query->solver()), query_(query) {
    VariableSet vars(query);
    for(Order order : query->orders())
        vars += order.expression()->variables();
    assert(vars.size() > 0);
    for(Variable* x : vars)
        x->cp()->registerBind(this);
    boundOrderVals_ = new Value[query->orders().size()];
    boundOrderError_ = new bool[query->orders().size()];
    bound_ = nullptr;
}

BnBOrderConstraint::~BnBOrderConstraint() {
    delete [] boundOrderVals_;
    delete [] boundOrderError_;
}

void BnBOrderConstraint::updateBound(Solution* sol) {
    bound_ = sol;
    bound_->restore();
    unsigned i = 0;
    for(Order order : query_->orders()) {
        if(VariableExpression* varexpr = dynamic_cast<VariableExpression*>(order.expression())) {
            boundOrderVals_[i].id(varexpr->variable()->valueId());
            boundOrderError_[i] = (boundOrderVals_[i].id() > 0);
        } else {
            boundOrderError_[i] = !order.expression()->evaluate(boundOrderVals_[i]);
            if(!boundOrderError_[i])
                boundOrderVals_[i].ensureInterpreted(*query_->store());
        }
        ++i;
    }
    solver_->refresh(this);
}

void BnBOrderConstraint::reset() {
    bound_ = nullptr;
}

bool BnBOrderConstraint::propagate() {
    if(bound_ == nullptr)
        return true;
    unsigned i = 0;
    for(Order order : query_->orders()) {
        if(boundOrderError_[i])
            return true; // don't know what to do if evaluation error
        Expression* expr = order.expression();
        bool desc = order.isDescending();
        Value* bval = &boundOrderVals_[i];
        if(VariableExpression* varexpr = dynamic_cast<VariableExpression*>(expr)) {
            cp::RDFVar* x = varexpr->variable()->cp();
            assert(bval->id() > 0);
            if(desc) {
                if(!x->updateMin(bval->id()))
                    return false;
            } else {
                if(!x->updateMax(bval->id()))
                    return false;
            }
            if(i == query_->orders().size() - 1) {
                if(!x->remove(bval->id()))
                    return false;
            }
            if(!x->bound() ||
                    (!desc && x->value() < bval->id()) ||
                    (desc && x->value() > bval->id()))
                return true;
        } else {
            for(Variable* var : expr->variables()) {
                if(var->isBound())
                    var->setFromCP();
                else
                    return true;
            }
            Value val;
            if(!expr->evaluate(val))
                return true;
            val.ensureInterpreted(*query_->store());
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
        ++i;
    }
    return false;
}

}
