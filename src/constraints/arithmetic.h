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
#ifndef CASTOR_CONSTRAINTS_ARITHMETIC_H
#define CASTOR_CONSTRAINTS_ARITHMETIC_H

#include "solver/constraint.h"
#include "query.h"

namespace castor {

/**
 * Channeling constraint between an RDF variable x and a numerical variable n.
 * The channel is only effective if b != RDF_ERROR, otherwise no pruning is
 * performed. No pruning is ever performed on b.
 */
class ArithmeticChannelConstraint : public cp::Constraint {
public:
    ArithmeticChannelConstraint(Query* query, cp::RDFVar* x, cp::NumVar* n,
                                cp::TriStateVar* b);
    bool propagate() override;

private:
    Store* store_;
    cp::RDFVar* x_;
    cp::NumVar* n_;
    cp::TriStateVar* b_;
    Value min_, max_; //!< cached values
};

/**
 * b != RDF_ERROR => x == r
 */
class NumConstantConstraint : public cp::Constraint {
public:
    NumConstantConstraint(Query* query, cp::NumVar* x, NumRange r,
                          cp::TriStateVar* b) :
        cp::Constraint(query->solver(), PRIOR_HIGH), x_(x), r_(r), b_(b) {
        b->registerChange(this);
    }

    bool propagate() override {
        if(!b_->contains(RDF_ERROR)) {
            domcheck(x_->updateMin(r_.lower()));
            domcheck(x_->updateMax(r_.upper_inclusive()));
            done_ = true;
        }
        return true;
    }

private:
    cp::NumVar* x_;
    NumRange r_;
    cp::TriStateVar* b_;
};

/**
 * x == y <=> b.
 * Do nothing on error.
 * As we work on an approximation, never bind to RDF_TRUE.
 */
class NumEqConstraint : public cp::Constraint {
public:
    NumEqConstraint(Query* query, cp::NumVar* x, cp::NumVar* y,
                    cp::TriStateVar* b);
    bool propagate() override;

private:
    cp::NumVar* x_;
    cp::NumVar* y_;
    cp::TriStateVar* b_;
};

/**
 * Inequality constraint: x {<,<=} y <=> b.
 * Do nothing on error.
 * As we work on an approximation, never bind to RDF_TRUE.
 */
class NumLessConstraint : public cp::Constraint {
public:
    NumLessConstraint(Query* query, cp::NumVar* x, cp::NumVar* y,
                      cp::TriStateVar* b);
    bool propagate() override;

private:
    cp::NumVar* x_;
    cp::NumVar* y_;
    cp::TriStateVar* b_;
};

/**
 * x + y = z
 */
class SumConstraint : public cp::Constraint {
public:
    SumConstraint(Query* query, cp::NumVar* x, cp::NumVar* y, cp::NumVar* z);
    bool propagate() override;

private:
    cp::NumVar* x_;
    cp::NumVar* y_;
    cp::NumVar* z_;
};

}

#endif // CASTOR_CONSTRAINTS_ARITHMETIC_H
