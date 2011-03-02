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

#include <stdlib.h>
#include <string.h>
#include <rasqal.h>

#include "xsddecimal.h"

struct TXSDDecimal {
    rasqal_xsd_decimal* val;
};

rasqal_world* world = NULL;
int worldUsage = 0;

void init_shared() {
    if(world == NULL)
        world = rasqal_new_world();
    worldUsage++;
}

void free_shared() {
    worldUsage--;
    if(worldUsage == 0) {
        rasqal_free_world(world);
        world = NULL;
    }
}

XSDDecimal* new_xsddecimal() {
    XSDDecimal *self;

    init_shared();
    self = (XSDDecimal*) malloc(sizeof(XSDDecimal));
    self->val = rasqal_new_xsd_decimal(world);
    return self;
}

void free_xsddecimal(XSDDecimal* self) {
    rasqal_free_xsd_decimal(self->val);
    free(self);
    free_shared();
}

void xsddecimal_copy(XSDDecimal* self, XSDDecimal* original) {
    rasqal_xsd_decimal_set_string(self->val,
                                  rasqal_xsd_decimal_as_string(original->val));
}

char* xsddecimal_get_string(XSDDecimal* self) {
    char *str, *result;
    size_t c;

    str = rasqal_xsd_decimal_as_counted_string(self->val, &c);
    c++;
    result = (char*) malloc(c*sizeof(char));
    memcpy(result, str, c);
    return result;
}

double xsddecimal_get_floating(XSDDecimal* self) {
    return rasqal_xsd_decimal_get_double(self->val);
}

void xsddecimal_set_string(XSDDecimal* self, const char* value) {
    rasqal_xsd_decimal_set_string(self->val, value);
}

void xsddecimal_set_integer(XSDDecimal* self, long value) {
    rasqal_xsd_decimal_set_long(self->val, value);
}

void xsddecimal_set_floating(XSDDecimal* self, double value) {
    rasqal_xsd_decimal_set_double(self->val, value);
}

int xsddecimal_compare(XSDDecimal* a, XSDDecimal* b) {
    return rasqal_xsd_decimal_compare(a->val, b->val);
}

bool xsddecimal_equals(XSDDecimal* a, XSDDecimal* b) {
    return rasqal_xsd_decimal_equals(a->val, b->val);
}

bool xsddecimal_iszero(XSDDecimal* self) {
    return rasqal_xsd_decimal_is_zero(self->val);
}

void xsddecimal_negate(XSDDecimal* self) {
    rasqal_xsd_decimal *d;
    d = rasqal_new_xsd_decimal(world);
    rasqal_xsd_decimal_negate(d, self->val);
    rasqal_free_xsd_decimal(self->val);
    self->val = d;
}

void xsddecimal_add(XSDDecimal* self, XSDDecimal* other) {
    rasqal_xsd_decimal *d;
    d = rasqal_new_xsd_decimal(world);
    rasqal_xsd_decimal_add(d, self->val, other->val);
    rasqal_free_xsd_decimal(self->val);
    self->val = d;
}

void xsddecimal_substract(XSDDecimal* self, XSDDecimal* other) {
    rasqal_xsd_decimal *d;
    d = rasqal_new_xsd_decimal(world);
    rasqal_xsd_decimal_subtract(d, self->val, other->val);
    rasqal_free_xsd_decimal(self->val);
    self->val = d;
}

void xsddecimal_multiply(XSDDecimal* self, XSDDecimal* other) {
    rasqal_xsd_decimal *d;
    d = rasqal_new_xsd_decimal(world);
    rasqal_xsd_decimal_multiply(d, self->val, other->val);
    rasqal_free_xsd_decimal(self->val);
    self->val = d;
}

void xsddecimal_divide(XSDDecimal* self, XSDDecimal* other) {
    rasqal_xsd_decimal *d;
    d = rasqal_new_xsd_decimal(world);
    rasqal_xsd_decimal_divide(d, self->val, other->val);
    rasqal_free_xsd_decimal(self->val);
    self->val = d;
}
