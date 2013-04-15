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
#ifndef CASTOR_CONSTRAINTS_BOOL_H
#define CASTOR_CONSTRAINTS_BOOL_H

#include "solver/constraint.h"
#include "query.h"

namespace castor {

/**
 * !x = y
 */
class NotConstraint : public cp::Constraint {
public:
    NotConstraint(Query* query, cp::TriStateVar* x, cp::TriStateVar* y) :
        Constraint(query->solver(), PRIOR_HIGH), x_(x), y_(y) {
        x->registerChange(this);
        y->registerChange(this);
    }
    bool propagate() override;

private:
    cp::TriStateVar* x_;
    cp::TriStateVar* y_;
};

/**
 * x && y = b
 */
class AndConstraint : public cp::Constraint {
public:
    AndConstraint(Query* query, cp::TriStateVar* x, cp::TriStateVar* y,
                  cp::TriStateVar* b) :
        Constraint(query->solver(), PRIOR_HIGH), x_(x), y_(y), b_(b) {
        x->registerChange(this);
        y->registerChange(this);
        b->registerChange(this);
    }
    bool propagate() override;

private:
    cp::TriStateVar* x_;
    cp::TriStateVar* y_;
    cp::TriStateVar* b_;
};

/**
 * x || y = b
 */
class OrConstraint : public cp::Constraint {
public:
    OrConstraint(Query* query, cp::TriStateVar* x, cp::TriStateVar* y,
                 cp::TriStateVar* b) :
        Constraint(query->solver(), PRIOR_HIGH), x_(x), y_(y), b_(b) {
        x->registerChange(this);
        y->registerChange(this);
        b->registerChange(this);
    }
    bool propagate() override;

private:
    cp::TriStateVar* x_;
    cp::TriStateVar* y_;
    cp::TriStateVar* b_;
};


}

#endif // CASTOR_CONSTRAINTS_BOOL_H
