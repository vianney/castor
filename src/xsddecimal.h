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
#ifndef CASTOR_XSDDECIMAL_H
#define CASTOR_XSDDECIMAL_H

#include "librdfwrapper.h"

namespace castor {

/**
 * Wrapper for an xsd:decimal value.
 */
class XSDDecimal {
public:
    /**
     * Construct a new decimal.
     */
    XSDDecimal() {
        val_ = rasqal_new_xsd_decimal(librdf::World::instance().rasqal);
    }
    /**
     * Copy constructor.
     */
    XSDDecimal(const XSDDecimal& o) {
        val_ = rasqal_new_xsd_decimal(librdf::World::instance().rasqal);
        rasqal_xsd_decimal_set_string(val_, rasqal_xsd_decimal_as_string(o.val_));
    }
    /**
     * Construct a new decimal from a lexical form.
     */
    XSDDecimal(const char* lexical) {
        val_ = rasqal_new_xsd_decimal(librdf::World::instance().rasqal);
        rasqal_xsd_decimal_set_string(val_, lexical);
    }
    /**
     * Construct a new decimal from an integer.
     */
    XSDDecimal(long integer) {
        val_ = rasqal_new_xsd_decimal(librdf::World::instance().rasqal);
        rasqal_xsd_decimal_set_long(val_, integer);
    }
    /**
     * Construct a new decimal from a floating point number
     */
    XSDDecimal(double floating) {
        val_ = rasqal_new_xsd_decimal(librdf::World::instance().rasqal);
        rasqal_xsd_decimal_set_double(val_, floating);
    }

    ~XSDDecimal() {
        rasqal_free_xsd_decimal(val_);
    }

    /**
     * @return lexical form of the decimal
     */
    std::string getString() const {
        size_t n;
        char* str = rasqal_xsd_decimal_as_counted_string(val_, &n);
        return std::string(str, n);
    }

    /**
     * @return value as floating point number (may loose precision)
     */
    double getFloat() const { return rasqal_xsd_decimal_get_double(val_); }

    /**
     * @return whether this value is zero
     */
    bool isZero() const { return rasqal_xsd_decimal_is_zero(val_); }

    /**
     * Compare two decimal numbers
     *
     * @param o second decimal
     * @return <0 if this < o, 0 if this == o and >0 if this > o
     */
    int compare(const XSDDecimal& o) const { return rasqal_xsd_decimal_compare(val_, o.val_); }

    bool operator==(const XSDDecimal& o) const { return rasqal_xsd_decimal_equals(val_, o.val_); }
    bool operator!=(const XSDDecimal& o) const { return !rasqal_xsd_decimal_equals(val_, o.val_); }
    bool operator<(const XSDDecimal& o) const { return compare(o) < 0; }
    bool operator>(const XSDDecimal& o) const { return compare(o) > 0; }
    bool operator<=(const XSDDecimal& o) const { return compare(o) <= 0; }
    bool operator>=(const XSDDecimal& o) const { return compare(o) >= 0; }

    XSDDecimal* negate() const {
        XSDDecimal* result = new XSDDecimal();
        rasqal_xsd_decimal_negate(result->val_, val_);
        return result;
    }

    XSDDecimal* add(const XSDDecimal& o) const {
        XSDDecimal* result = new XSDDecimal();
        rasqal_xsd_decimal_add(result->val_, val_, o.val_);
        return result;
    }

    XSDDecimal* substract(const XSDDecimal& o) const {
        XSDDecimal* result = new XSDDecimal();
        rasqal_xsd_decimal_subtract(result->val_, val_, o.val_);
        return result;
    }

    XSDDecimal* multiply(const XSDDecimal& o) const {
        XSDDecimal* result = new XSDDecimal();
        rasqal_xsd_decimal_multiply(result->val_, val_, o.val_);
        return result;
    }

    XSDDecimal* divide(const XSDDecimal& o) const {
        XSDDecimal* result = new XSDDecimal();
        rasqal_xsd_decimal_divide(result->val_, val_, o.val_);
        return result;
    }

private:
    rasqal_xsd_decimal* val_;
};

}

#endif // CASTOR_XSDDECIMAL_H
