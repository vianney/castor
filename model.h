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
#ifndef MODEL_H
#define MODEL_H

#include "defs.h"

/**
 * Standard value type ids. Higher ids means custom URI.
 */
typedef enum {
    VALUE_TYPE_BLANK,         // blank node
    VALUE_TYPE_IRI,           // IRI
    VALUE_TYPE_PLAIN_STRING,  // plain literal
    VALUE_TYPE_TYPED_STRING,  // xsd:string
    VALUE_TYPE_BOOLEAN,       // xsd:boolean
    VALUE_TYPE_INTEGER,       // xsd:integer
    VALUE_TYPE_POSINTEGER,    // xsd:positiveInteger
    VALUE_TYPE_NONPOSINTEGER, // xsd:nonPositiveInteger
    VALUE_TYPE_NEGINTEGER,    // xsd:negativeInteger
    VALUE_TYPE_NONNEGINTEGER, // xsd:nonNegativeInteger
    VALUE_TYPE_BYTE,          // xsd:byte
    VALUE_TYPE_SHORT,         // xsd:short
    VALUE_TYPE_INT,           // xsd:int
    VALUE_TYPE_LONG,          // xsd:long
    VALUE_TYPE_UNSIGNEDBYTE,  // xsd:unsignedByte
    VALUE_TYPE_UNSIGNEDSHORT, // xsd:unsignedShort
    VALUE_TYPE_UNSIGNEDINT,   // xsd:unsignedInt
    VALUE_TYPE_UNSIGNEDLONG,  // xsd:unsignedLong
    VALUE_TYPE_FLOAT,         // xsd:float
    VALUE_TYPE_DOUBLE,        // xsd:double
    VALUE_TYPE_DECIMAL,       // xsd:decimal
    VALUE_TYPE_DATETIME,      // xsd:dateTime

    VALUE_TYPE_FIRST_CUSTOM,
    VALUE_TYPE_UNKOWN = -1,

    // internal values
    VALUE_TYPE_FIRST_XSD = VALUE_TYPE_TYPED_STRING,
    VALUE_TYPE_LAST_XSD = VALUE_TYPE_DATETIME,
    VALUE_TYPE_FIRST_NUMERIC = VALUE_TYPE_INTEGER,
    VALUE_TYPE_LAST_NUMERIC = VALUE_TYPE_DECIMAL,
    VALUE_TYPE_FIRST_INTEGER = VALUE_TYPE_INTEGER,
    VALUE_TYPE_LAST_INTEGER = VALUE_TYPE_UNSIGNEDLONG,
    VALUE_TYPE_FIRST_FLOATING = VALUE_TYPE_FLOAT,
    VALUE_TYPE_LAST_FLOATING = VALUE_TYPE_DECIMAL
} ValueType;

/**
 * URIs corresponding to the value types
 */
char *VALUETYPE_URIS[VALUE_TYPE_FIRST_CUSTOM];

/**
 * Structure for date and time information, compatible with SQL_TIMESTAMP_STRUCT
 */
typedef struct {
    short year;
    unsigned short month;
    unsigned short day;
    unsigned short hour;
    unsigned short minute;
    unsigned short second;
    unsigned int fraction; // in 1000 millionths of a second (10e-9)
} DateTime;

/**
 * Structure defining a value
 */
typedef struct {
    /**
     * ID of the value, starting from 0. Or -1 if unknown.
     */
    int id;
    /**
     * Datatype of the value. May be higher than the enumeration.
     * Or -1 if unknown.
     */
    ValueType type;
    /**
     * URI of the datatype. NULL if type <= VALUE_TYPE_PLAIN_STRING.
     */
    char* typeUri;
    /**
     * Language tag id, starting from 0. Or -1 if unknown.
     */
    int language;
    /**
     * String representation of the language tag.
     * NULL if type <= VALUE_TYPE_IRI.
     */
    char* languageTag;
    /**
     * Lexical form.
     */
    char* lexical;
    union {
        /**
         * Integer representation of the lexical. Unspecified if type not in
         * range VALUE_TYPE_FIRST_INTEGER..VALUE_TYPE_LAST_INTEGER.
         */
        long integer;
        /**
         * Floating point representation of the lexical. Unspecified if type not
         * in range VALUE_TYPE_FIRST_FLOATING..VALUE_TYPE_LAST_FLOATING.
         */
        double floating;
        /**
         * Date and time representation of the lexical. Unspecified if type is
         * not VALUE_TYPE_DATETIME.
         */
        DateTime datetime;
    };
} Value;

/**
 * Small statement structure containing ids.
 */
typedef struct {
    int subject;
    int predicate;
    int object;
} Statement;

#endif // MODEL_H
