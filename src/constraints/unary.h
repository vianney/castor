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
#ifndef CASTOR_CONSTRAINTS_UNARY_H
#define CASTOR_CONSTRAINTS_UNARY_H

#include "solver/constraint.h"
#include "query.h"

namespace castor {

/**
 * Constant constraint: x = v.
 *
 * @param D domain type
 * @param T value type
 */
template<class D, class T>
class ConstantConstraint : public cp::Constraint {
public:
    ConstantConstraint(Query* query, D* x, T v) :
        Constraint(query->solver(), PRIOR_HIGH), x_(x), v_(v) {}
    bool post() { return x_->bind(v_); }

private:
    D* x_;
    T v_;
};

/**
 * b = RDF_True
 */
class TrueConstraint : public ConstantConstraint<cp::TriStateVar, TriState> {
public:
    TrueConstraint(Query* query, cp::TriStateVar* b) :
        ConstantConstraint(query, b, RDF_TRUE) {}
};

/**
 * b = RDF_FALSE
 */
class FalseConstraint : public ConstantConstraint<cp::TriStateVar, TriState> {
public:
    FalseConstraint(Query* query, cp::TriStateVar* b) :
        ConstantConstraint(query, b, RDF_FALSE) {}
};

/**
 * b = RDF_ERROR
 */
class ErrorConstraint : public ConstantConstraint<cp::TriStateVar, TriState> {
public:
    ErrorConstraint(Query* query, cp::TriStateVar* b) :
        ConstantConstraint(query, b, RDF_ERROR) {}
};

/**
 * Constant constraint: x != v.
 *
 * @param D domain type
 * @param T value type
 */
template<class D, class T>
class NotConstantConstraint : public cp::Constraint {
public:
    NotConstantConstraint(Query* query, D* x, T v) :
        Constraint(query->solver(), PRIOR_HIGH), x_(x), v_(v) {}
    bool post() { return x_->remove(v_); }

private:
    D* x_;
    T v_;
};

/**
 * b != RDF_TRUE
 */
class NotTrueConstraint : public NotConstantConstraint<cp::TriStateVar, TriState> {
public:
    NotTrueConstraint(Query* query, cp::TriStateVar* b) :
        NotConstantConstraint(query, b, RDF_TRUE) {}
};

/**
 * b != RDF_FALSE
 */
class NotFalseConstraint : public NotConstantConstraint<cp::TriStateVar, TriState> {
public:
    NotFalseConstraint(Query* query, cp::TriStateVar* b) :
        NotConstantConstraint(query, b, RDF_FALSE) {}
};

/**
 * Ensure a SPARQL variable is bound by removing value 0 from the CP domain
 */
class BoundConstraint : public cp::Constraint {
public:
    BoundConstraint(Query* query, cp::RDFVar* x) :
        Constraint(query->solver(), PRIOR_HIGH), x_(x) {}
    bool post() { return x_->remove(0); }

private:
    cp::RDFVar* x_;
};

/**
 * Restrict domain to a specified range if b is RDF_TRUE or RDF_FALSE, or
 * outside the range if b is RDF_ERROR.
 */
class InRangeConstraint : public cp::Constraint {
public:
    InRangeConstraint(Query* query, cp::RDFVar* x, ValueRange rng,
                      cp::TriStateVar* b) :
        Constraint(query->solver(), PRIOR_HIGH), x_(x), rng_(rng), b_(b) {
        if(!rng.empty()) {
            x->registerMin(this);
            x->registerMax(this);
            b->registerChange(this);
        }
    }
    bool post() override {
        if(rng_.empty())
            return b_->bind(RDF_ERROR);
        else
            return propagate();
    }
    bool propagate() override {
        if(x_->min() >= rng_.from && x_->max() <= rng_.to) {
            domcheck(b_->remove(RDF_ERROR));
            done_ = true;
        } else if(x_->max() < rng_.from || x_->min() > rng_.to) {
            domcheck(b_->bind(RDF_ERROR));
            done_ = true;
        } else if(!b_->contains(RDF_ERROR)) {
            domcheck(x_->updateMin(rng_.from));
            domcheck(x_->updateMax(rng_.to));
            done_ = true;
        } else if(b_->bound() && b_->value() == RDF_ERROR) {
            for(Value::id_t id : rng_)
                domcheck(x_->remove(id));
            done_ = true;
        }
        return true;
    }

private:
    cp::RDFVar* x_;
    ValueRange rng_;
    cp::TriStateVar* b_;
};

/**
 * x >= v <=> b (do nothing on error)
 */
class ConstGEConstraint : public cp::Constraint {
public:
    ConstGEConstraint(Query* query, cp::RDFVar* x, Value::id_t v,
                      cp::TriStateVar* b) :
        Constraint(query->solver(), PRIOR_HIGH), x_(x), v_(v), b_(b) {
        x_->registerMin(this);
        x_->registerMax(this);
        b_->registerChange(this);
    }
    bool propagate() override {
        if(x_->min() >= v_) {
            domcheck(b_->remove(RDF_FALSE));
            done_ = true;
        } else if(x_->max() < v_) {
            domcheck(b_->remove(RDF_TRUE));
            done_ = true;
        } else if(b_->bound() && b_->value() == RDF_TRUE) {
            domcheck(x_->updateMin(v_));
            done_ = true;
        } else if(b_->bound() && b_->value() == RDF_FALSE) {
            domcheck(x_->updateMax(v_ - 1));
            done_ = true;
        }
        return true;
    }

private:
    cp::RDFVar* x_;
    Value::id_t v_;
    cp::TriStateVar* b_;
};

/**
 * x <= v <=> b (do nothing on error)
 */
class ConstLEConstraint : public cp::Constraint {
public:
    ConstLEConstraint(Query* query, cp::RDFVar* x, Value::id_t v,
                      cp::TriStateVar* b) :
        Constraint(query->solver(), PRIOR_HIGH), x_(x), v_(v), b_(b) {
        x_->registerMin(this);
        x_->registerMax(this);
        b_->registerChange(this);
    }
    bool propagate() override {
        if(x_->max() <= v_) {
            domcheck(b_->remove(RDF_FALSE));
            done_ = true;
        } else if(x_->min() > v_) {
            domcheck(b_->remove(RDF_TRUE));
            done_ = true;
        } else if(b_->bound() && b_->value() == RDF_TRUE) {
            domcheck(x_->updateMax(v_));
            done_ = true;
        } else if(b_->bound() && b_->value() == RDF_FALSE) {
            domcheck(x_->updateMin(v_ + 1));
            done_ = true;
        }
        return true;
    }

private:
    cp::RDFVar* x_;
    Value::id_t v_;
    cp::TriStateVar* b_;
};

}

#endif // CASTOR_CONSTRAINTS_UNARY_H
