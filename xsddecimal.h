/* This file is part of Castor
 *
 * Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
 * Copyright (C) 2010 - UCLouvain
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
#ifndef XSDDECIMAL_H
#define XSDDECIMAL_H

#include "defs.h"

/**
 * Opaque type for an xsd:decimal
 */
typedef struct TXSDDecimal XSDDecimal;

/**
 * @return a new xsd:decimal
 */
XSDDecimal* new_xsddecimal();

/**
 * Free an xsd:decimal
 *
 * @param self an xsd:decimal instance
 */
void free_xsddecimal(XSDDecimal* self);

/**
 * Copy from another decimal.
 *
 * @param self an xsd:decimal instance
 * @param original value to copy
 */
void xsddecimal_copy(XSDDecimal* self, XSDDecimal* original);

/**
 * @param self an xsd:decimal instance
 * @return lexical form of the decimal (caller takes ownership)
 */
char* xsddecimal_get_string(XSDDecimal* self);

/**
 * @param self an xsd:decimal instance
 * @return value as a floating point number (may loose precision)
 */
double xsddecimal_get_floating(XSDDecimal* self);

/**
 * Set the value from a lexical form
 *
 * @param self an xsd:decimal instance
 * @param value value
 */
void xsddecimal_set_string(XSDDecimal* self, const char* value);

/**
 * Set the value from an integer
 *
 * @param self an xsd:decimal instance
 * @param value value
 */
void xsddecimal_set_integer(XSDDecimal* self, long value);

/**
 * Set the value from a floating point value
 *
 * @param self an xsd:decimal instance
 * @param value value
 */
void xsddecimal_set_floating(XSDDecimal* self, double value);

/**
 * Compare two decimal numbers
 *
 * @param a first decimal
 * @param b second decimal
 * @return <0 if a < b, 0 if a == b and >0 if a > b
 */
int xsddecimal_compare(XSDDecimal* a, XSDDecimal* b);

/**
 * Compare two decimal numbers
 *
 * @param a first decimal
 * @param b second decimal
 * @return true if a == b, false otherwise
 */
bool xsddecimal_equals(XSDDecimal* a, XSDDecimal* b);

/**
 * @param self a decimal number
 * @return true if self == 0
 */
bool xsddecimal_iszero(XSDDecimal* self);

/**
 * self = -self
 */
void xsddecimal_negate(XSDDecimal* self);

/**
 * self += other
 */
void xsddecimal_add(XSDDecimal* self, XSDDecimal* other);

/**
 * self -= other
 */
void xsddecimal_substract(XSDDecimal* self, XSDDecimal* other);

/**
 * self *= other
 */
void xsddecimal_multiply(XSDDecimal* self, XSDDecimal* other);

/**
 * self /= other
 */
void xsddecimal_divide(XSDDecimal* self, XSDDecimal* other);

#endif // XSDDECIMAL_H
