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
#include "arithmetic.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>

namespace castor {

ArithmeticChannelConstraint::ArithmeticChannelConstraint(Query* query,
                        cp::RDFVar* x, cp::NumVar* n, cp::TriStateVar* b) :
        cp::Constraint(query->solver(), PRIOR_MEDIUM),
        store_(query->store()), x_(x), n_(n), b_(b) {
    x->registerBounds(this);
    n->registerBounds(this);
    b->registerChange(this);
}


namespace {
/**
 * Iterator over values returning the numerical approximation.
 */
class NumApproxIterator :
        public std::iterator<std::random_access_iterator_tag, long, long> {
public:
    NumApproxIterator(Store* store, Value::id_t id) : store_(store), id_(id) {}
    Value::id_t id() const { return id_; }
    bool operator!=(const NumApproxIterator& o) const { return id_ != o.id_; }
    NumApproxIterator& operator++() { ++id_; return *this; }
    NumApproxIterator& operator+=(long n) { id_ += n; return *this; }
    long operator-(NumApproxIterator& o) {
        return static_cast<long>(id_) - static_cast<long>(o.id_);
    }
    long operator*() { return store_->lookupValue(id_).numapprox().lower(); }
private:
    Store* store_;
    Value::id_t id_;
};
}

bool ArithmeticChannelConstraint::propagate() {
    if(b_->contains(RDF_ERROR))
        return true;
    ValueRange rng = store_->range(Value::CAT_NUMERIC);
    domcheck(x_->updateMin(rng.from));
    domcheck(x_->updateMax(rng.to));
    if(min_.id() != x_->min())
        min_ = store_->lookupValue(x_->min());
    if(max_.id() != x_->max())
        max_ = store_->lookupValue(x_->max());
    if(n_->min() < min_.numapprox().lower()) {
        domcheck(n_->updateMin(min_.numapprox().lower()));
    } else if(n_->min() > min_.numapprox().lower()) {
        if(n_->min() > max_.numapprox().lower())
            return false;
        NumApproxIterator it = std::lower_bound(
                    NumApproxIterator(store_, min_.id()),
                    NumApproxIterator(store_, max_.id() + 1),
                    n_->min());
        domcheck(x_->updateMin(it.id()));
        min_ = store_->lookupValue(it.id());
        domcheck(n_->updateMin(min_.numapprox().lower()));
    }
    if(n_->max() > max_.numapprox().lower()) {
        domcheck(n_->updateMax(max_.numapprox().lower()));
    } else if(n_->max() < max_.numapprox().lower()) {
        if(n_->max() < min_.numapprox().lower())
            return false;
        NumApproxIterator it = std::upper_bound(
                    NumApproxIterator(store_, min_.id()),
                    NumApproxIterator(store_, max_.id() + 1),
                    n_->max());
        domcheck(x_->updateMax(it.id() - 1));
        max_ = store_->lookupValue(it.id() - 1);
        domcheck(n_->updateMax(max_.numapprox().lower()));
    }
    if(n_->bound())
        done_ = true;
    return true;
}

NumEqConstraint::NumEqConstraint(Query* query, cp::NumVar* x, cp::NumVar* y,
                                 cp::TriStateVar* b) :
        cp::Constraint(query->solver(), PRIOR_HIGH), x_(x), y_(y), b_(b) {
    x->registerBounds(this);
    y->registerBounds(this);
    b->registerChange(this);
}

bool NumEqConstraint::propagate() {
    if(b_->contains(RDF_ERROR))
        return true;
    if(!b_->contains(RDF_FALSE)) {
        domcheck(x_->updateMin(y_->min()));
        domcheck(x_->updateMax(y_->max()));
        domcheck(y_->updateMin(x_->min()));
        domcheck(y_->updateMax(x_->max()));
        if(x_->bound())
            done_ = true;
    } else if(x_->max() < y_->min() || y_->max() < x_->min()) {
        domcheck(b_->remove(RDF_TRUE));
        done_ = true;
    }
    return true;
}

NumLessConstraint::NumLessConstraint(Query* query, cp::NumVar* x, cp::NumVar* y,
                                     cp::TriStateVar* b) :
        cp::Constraint(query->solver(), PRIOR_HIGH), x_(x), y_(y), b_(b) {
    x->registerBounds(this);
    y->registerBounds(this);
    b->registerChange(this);
}

bool NumLessConstraint::propagate() {
    if(b_->contains(RDF_ERROR))
        return true;
    if(!b_->contains(RDF_FALSE)) {
        domcheck(x_->updateMax(y_->max()));
        domcheck(y_->updateMin(x_->min()));
        if(x_->max() <= y_->min())
            done_ = true;
    } else if(x_->max() < y_->min()) {
        domcheck(b_->remove(RDF_TRUE));
        done_ = true;
    }
    return true;
}

SumConstraint::SumConstraint(Query* query, cp::NumVar* x, cp::NumVar* y,
                             cp::NumVar* z) :
        cp::Constraint(query->solver(), PRIOR_HIGH), x_(x), y_(y), z_(z) {
    x->registerBounds(this);
    y->registerBounds(this);
    z->registerBounds(this);
}

namespace {
//! @return the numerical range corresponding to D(x)
inline NumRange range(cp::NumVar* x) {
    return NumRange(x->min(),
                    x->max() == NumRange::POS_INFINITY ? x->max() :
                                                         x->max() + 1);
}
}

bool SumConstraint::propagate() {
    NumRange r = range(z_) - range(y_);
    domcheck(x_->updateMin(r.lower()));
    domcheck(x_->updateMax(r.upper_inclusive()));
    r = range(z_) - range(x_);
    domcheck(y_->updateMin(r.lower()));
    domcheck(y_->updateMax(r.upper_inclusive()));
    r = range(x_) + range(y_);
    domcheck(z_->updateMin(r.lower()));
    domcheck(z_->updateMax(r.upper_inclusive()));
    if(x_->bound() + y_->bound() + z_->bound() >= 2)
        done_ = true;
    return true;
}

}
