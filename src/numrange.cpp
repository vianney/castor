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
#include "numrange.h"

namespace castor {

const XSDDecimal NumRange::DECIMAL_POS_INFINITY(NumRange::POS_INFINITY);

const XSDDecimal NumRange::DECIMAL_NEG_INFINITY(NumRange::NEG_INFINITY);

std::ostream& operator<<(std::ostream& out, const NumRange& rng) {
    if(rng.empty()) {
        out << "[]";
    } else {
        if(rng.lower() == NumRange::NEG_INFINITY)
            out << "]-inf";
        else
            out << "[" << rng.lower();
        if(rng.upper() == NumRange::POS_INFINITY)
            out << ",+inf[";
        else
            out << "," << rng.upper() << "[";
    }
    return out;
}

namespace {
//! Addition with overflow check
inline long add(long a, long b) {
    if(b > 0 && a > NumRange::POS_INFINITY - b)
        return NumRange::POS_INFINITY;
    else if(b < 0 && a < NumRange::NEG_INFINITY - b)
        return NumRange::NEG_INFINITY;
    else
        return a + b;
}
//! Substraction with overflow check
inline long substract(long a, long b) {
    if(b > 0 && a < NumRange::NEG_INFINITY + b)
        return NumRange::NEG_INFINITY;
    else if(b < 0 && a > NumRange::POS_INFINITY + b)
        return NumRange::POS_INFINITY;
    else
        return a - b;
}
}

NumRange NumRange::operator +(const NumRange& o) const  {
    assert(!empty() && !o.empty());
    NumRange result;
    if(lb_ == NEG_INFINITY || o.lb_ == NEG_INFINITY) {
        result.lb_ = NEG_INFINITY;
    } else {
        result.lb_ = add(lb_, o.lb_);
    }
    if(ub_ == POS_INFINITY || o.ub_ == POS_INFINITY) {
        result.ub_ = POS_INFINITY;
    } else {
        result.ub_ = add(ub_, o.ub_);
        if(result.ub_ < POS_INFINITY)
            result.ub_ += 1; // we need an exclusive upper bound
    }
    return result;
}

NumRange NumRange::operator -(const NumRange& o) const {
    assert(!empty() && !o.empty());
    NumRange result;
    if(lb_ == NEG_INFINITY || o.ub_ == POS_INFINITY) {
        result.lb_ = NEG_INFINITY;
    } else {
        result.lb_ = substract(lb_, o.ub_);
    }
    if(ub_ == POS_INFINITY || o.lb_ == NEG_INFINITY) {
        result.ub_ = POS_INFINITY;
    } else {
        result.ub_ = substract(ub_, o.lb_);
    }
    return result;
}

}
