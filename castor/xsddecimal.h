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
#ifndef CASTOR_XSDDECIMAL_H
#define CASTOR_XSDDECIMAL_H

#include "model.h"

namespace castor {

/**
 * Wrapper for an xsd:decimal value.
 */
class XSDDecimal {
    rasqal_xsd_decimal *val;

public:
    /**
     * Construct a new decimal.
     */
    XSDDecimal() {
        val = rasqal_new_xsd_decimal(REDLAND.rasqal);
    }
    /**
     * Copy constructor.
     */
    XSDDecimal(const XSDDecimal &o) {
        val = rasqal_new_xsd_decimal(REDLAND.rasqal);
        rasqal_xsd_decimal_set_string(val, rasqal_xsd_decimal_as_string(o.val));
    }
    /**
     * Construct a new decimal from a lexical form.
     */
    XSDDecimal(const char* lexical) {
        val = rasqal_new_xsd_decimal(REDLAND.rasqal);
        rasqal_xsd_decimal_set_string(val, lexical);
    }
    /**
     * Construct a new decimal from an integer.
     */
    XSDDecimal(long integer) {
        val = rasqal_new_xsd_decimal(REDLAND.rasqal);
        rasqal_xsd_decimal_set_long(val, integer);
    }
    /**
     * Construct a new decimal from a floating point number
     */
    XSDDecimal(double floating) {
        val = rasqal_new_xsd_decimal(REDLAND.rasqal);
        rasqal_xsd_decimal_set_double(val, floating);
    }

    ~XSDDecimal() {
        rasqal_free_xsd_decimal(val);
    }

    /**
     * @return lexical form of the decimal
     */
    std::string getString() {
        char *str;
        size_t n;
        str = rasqal_xsd_decimal_as_counted_string(val, &n);
        return std::string(str, n);
    }

    /**
     * @return value as floating point number (may loose precision)
     */
    double getFloat() { return rasqal_xsd_decimal_get_double(val); }

    /**
     * @return whether this value is zero
     */
    bool isZero() { return rasqal_xsd_decimal_is_zero(val); }

    /**
     * Compare two decimal numbers
     *
     * @param o second decimal
     * @return <0 if this < o, 0 if this == o and >0 if this > o
     */
    int compare(const XSDDecimal &o) { return rasqal_xsd_decimal_compare(val, o.val); }

    bool operator==(const XSDDecimal &o) { return rasqal_xsd_decimal_equals(val, o.val); }
    bool operator!=(const XSDDecimal &o) { return !rasqal_xsd_decimal_equals(val, o.val); }
    bool operator<(const XSDDecimal &o) { return compare(o) < 0; }
    bool operator>(const XSDDecimal &o) { return compare(o) > 0; }
    bool operator<=(const XSDDecimal &o) { return compare(o) <= 0; }
    bool operator>=(const XSDDecimal &o) { return compare(o) >= 0; }

    XSDDecimal* negate() {
        XSDDecimal *result = new XSDDecimal();
        rasqal_xsd_decimal_negate(result->val, val);
        return result;
    }

    XSDDecimal* add(const XSDDecimal &o) {
        XSDDecimal *result = new XSDDecimal();
        rasqal_xsd_decimal_add(result->val, val, o.val);
        return result;
    }

    XSDDecimal* substract(const XSDDecimal &o) {
        XSDDecimal *result = new XSDDecimal();
        rasqal_xsd_decimal_subtract(result->val, val, o.val);
        return result;
    }

    XSDDecimal* multiply(const XSDDecimal &o) {
        XSDDecimal *result = new XSDDecimal();
        rasqal_xsd_decimal_multiply(result->val, val, o.val);
        return result;
    }

    XSDDecimal* divide(const XSDDecimal &o) {
        XSDDecimal *result = new XSDDecimal();
        rasqal_xsd_decimal_divide(result->val, val, o.val);
        return result;
    }
};

}

#endif // CASTOR_XSDDECIMAL_H
