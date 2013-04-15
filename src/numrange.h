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
#ifndef CASTOR_NUMRANGE_H
#define CASTOR_NUMRANGE_H

#include <cmath>
#include <limits>

#include "xsddecimal.h"

namespace castor {

/**
 * A numerical range with integer precision.
 * The described range is [lb, ub).
 */
class NumRange {
public:

    /**
     * An upper bound with this value will be treated as positive infinity.
     */
    static constexpr long POS_INFINITY = std::numeric_limits<long>::max();
    /**
     * A lower bound with this value will be treated as negative infinity.
     */
    static constexpr long NEG_INFINITY = std::numeric_limits<long>::min();

    /**
     * Construct an empty range.
     */
    NumRange() : lb_(0), ub_(0) {}
    /**
     * Construct a range.
     * @param lb lower bound (inclusive)
     * @param ub upper bound (exclusive)
     */
    NumRange(long lb, long ub) : lb_(lb), ub_(ub) {}
    /**
     * Construct the range [val,val+1).
     * If val is POS_INFINITY, the range is [val,+infinity).
     * If val is NEG_INFINITY, the range is (-infinity,val+1).
     * @param val
     */
    explicit NumRange(long val) : lb_(val),
        ub_(val == POS_INFINITY ? val : val + 1) {}
    /**
     * Like NumRange(long), but using the floor of val and detecting infinity.
     * @param val
     */
    explicit NumRange(double val) {
        val = std::floor(val);
        if(val <= NEG_INFINITY) {
            lb_ = NEG_INFINITY;
            ub_ = NEG_INFINITY + 1;
        } else if(val >= POS_INFINITY) {
            lb_ = ub_ = POS_INFINITY;
        } else {
            lb_ = val;
            ub_ = lb_ + 1;
        }
    }
    /**
     * Like NumRange(double), but with decimals.
     * @param val
     */
    explicit NumRange(const XSDDecimal& val) {
        XSDDecimal* v = val.floor();
        if(val <= DECIMAL_NEG_INFINITY) {
            lb_ = NEG_INFINITY;
            ub_ = NEG_INFINITY + 1;
        } else if(val >= DECIMAL_POS_INFINITY) {
            lb_ = ub_ = POS_INFINITY;
        } else {
            lb_ = v->getLong();
            ub_ = lb_ + 1;
        }
        delete v;
    }

    //! Copyable
    NumRange(const NumRange& o) = default;
    NumRange& operator=(const NumRange& o) = default;

    /**
     * @return whether the range is empty
     */
    bool empty() const { return ub_ <= lb_ && ub_ != POS_INFINITY; }

    //! @return the lower bound
    long lower() const { return lb_; }
    //! @return the upper bound
    long upper() const { return ub_; }
    //! @return the inclusive upper bound
    long upper_inclusive() const { return ub_ == POS_INFINITY ? ub_ : ub_ - 1; }

    /**
     * @param o
     * @return whether this and o define the same range
     */
    bool operator==(const NumRange& o) const {
        return lb_ == o.lb_ && ub_ == o.ub_;
    }

    /**
     * @param o
     * @return whether this and o do not define the same range
     */
    bool operator!=(const NumRange& o) const {
        return lb_ != o.lb_ || ub_ != o.ub_;
    }

    /**
     * @pre !empty() && !o.empty()
     * @param o
     * @return whether all the values in this range are strictly smaller than
     *         any value in range o
     */
    bool operator<(const NumRange& o) const {
        return ub_ <= o.lb_ && ub_ != POS_INFINITY && o.lb_ != NEG_INFINITY;
    }

    /**
     * @pre !empty() && !o.empty()
     * @param o
     * @return whether all the values in this range are strictly greater than
     *         any value in range o
     */
    bool operator>(const NumRange& o) const {
        return lb_ > o.ub_ && o.ub_ != POS_INFINITY && lb_ != NEG_INFINITY;
    }

    NumRange operator+(const NumRange& o) const;

    NumRange operator-(const NumRange& o) const;

private:
    long lb_; //!< the lower bound (inclusive)
    long ub_; //!< the upper bound (exclusive)

    //! Decimal version of POS_INFINITY
    static const XSDDecimal DECIMAL_POS_INFINITY;
    //! Decimal version of NEG_INFINITY
    static const XSDDecimal DECIMAL_NEG_INFINITY;
};

std::ostream& operator<<(std::ostream& out, const NumRange& rng);

}

#endif // CASTOR_NUMRANGE_H
