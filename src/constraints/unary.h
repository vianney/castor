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
#ifndef CASTOR_CONSTRAINTS_UNARY_H
#define CASTOR_CONSTRAINTS_UNARY_H

#include "solver/constraint.h"
#include "query.h"

namespace castor {

/**
 * Always-false constraint
 */
class FalseConstraint : public cp::Constraint {
public:
    FalseConstraint(Query* query) : Constraint(query->solver(), PRIOR_HIGH) {}
    bool post() { return false; }
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
 * Restrict domain to a specified range
 */
class InRangeConstraint : public cp::Constraint {
public:
    InRangeConstraint(Query* query, cp::RDFVar* x, ValueRange rng) :
        Constraint(query->solver(), PRIOR_HIGH), x_(x), rng_(rng) {}
    bool post() {
        return x_->updateMin(rng_.from) && x_->updateMax(rng_.to);
    }

private:
    cp::RDFVar* x_;
    ValueRange rng_;
};

/**
 * Restrict domain to a set of ranges
 */
class InRangesConstraint : public cp::Constraint {
public:
    InRangesConstraint(Query* query, cp::RDFVar* x,
                       std::initializer_list<ValueRange> ranges) :
        Constraint(query->solver(), PRIOR_HIGH), x_(x), ranges_(ranges) {}
    bool post() {
        x_->clearMarks();
        for(ValueRange rng : ranges_) {
            for(Value::id_t id : rng)
                x_->mark(id);
        }
        return x_->restrictToMarks();
    }

private:
    cp::RDFVar* x_;
    std::initializer_list<ValueRange> ranges_;
};

/**
 * Restrict domain to values that are comparable in SPARQL filters
 * (i.e., simple literals, typed strings, booleans, numbers and dates)
 */
class ComparableConstraint : public InRangeConstraint {
public:
    ComparableConstraint(Query* query, cp::RDFVar* x) :
        InRangeConstraint(query, x,
                          query->store()->range(Value::CAT_SIMPLE_LITERAL,
                                                Value::CAT_DATETIME)) {}
};

/**
 * Remove a specified range from a domain
 */
class NotInRangeConstraint : public cp::Constraint {
public:
    NotInRangeConstraint(Query* query, cp::RDFVar* x, ValueRange rng) :
        Constraint(query->solver(), PRIOR_HIGH), x_(x), rng_(rng) {}
    bool post() {
        for(Value::id_t id : rng_) {
            if(!x_->remove(id))
                return false;
        }
        return true;
    }

private:
    cp::RDFVar* x_;
    ValueRange rng_;
};

/**
 * x >= v
 */
class ConstGEConstraint : public cp::Constraint {
public:
    ConstGEConstraint(Query* query, cp::RDFVar* x, Value::id_t v) :
        Constraint(query->solver(), PRIOR_HIGH), x_(x), v_(v) {}
    bool post() { return x_->updateMin(v_); }

private:
    cp::RDFVar* x_;
    Value::id_t v_;
};

/**
 * x <= v
 */
class ConstLEConstraint : public cp::Constraint {
public:
    ConstLEConstraint(Query* query, cp::RDFVar* x, Value::id_t v) :
        Constraint(query->solver(), PRIOR_HIGH), x_(x), v_(v) {}
    bool post() { return x_->updateMax(v_); }

private:
    cp::RDFVar* x_;
    Value::id_t v_;
};

}

#endif // CASTOR_CONSTRAINTS_UNARY_H
