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

#define IS_VALUE_TYPE_XSD(type) \
    ((type) >= VALUE_TYPE_FIRST_XSD && (type) <= VALUE_TYPE_LAST_XSD)
#define IS_VALUE_TYPE_NUMERIC(type) \
    ((type) >= VALUE_TYPE_FIRST_NUMERIC && (type) <= VALUE_TYPE_LAST_NUMERIC)
#define IS_VALUE_TYPE_INTEGER(type) \
    ((type) >= VALUE_TYPE_FIRST_INTEGER && (type) <= VALUE_TYPE_LAST_INTEGER)
#define IS_VALUE_TYPE_FLOATING(type) \
    ((type) >= VALUE_TYPE_FIRST_FLOATING && (type) <= VALUE_TYPE_LAST_FLOATING)

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
 * Value cleanup flags
 */
typedef enum {
    /**
     * The caller has nothing to do
     */
    VALUE_CLEAN_NOTHING = 0,
    /**
     * The caller should free the datatype URI
     */
    VALUE_CLEAN_TYPE_URI = 1,
    /**
     * The caller should free the language tag
     */
    VALUE_CLEAN_LANGUAGE_TAG = 2,
    /**
     * The caller should free the lexical form
     */
    VALUE_CLEAN_LEXICAL = 4
} ValueCleanFlags;

/**
 * Structure defining a value
 */
typedef struct {
    /**
     * ID of the value, starting from 0. Or -1 if not part of a store.
     */
    int id;
    /**
     * Datatype of the value. May be higher than the enumeration.
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
     * Lexical form. May be NULL if there is a native representation.
     */
    char* lexical;
    /**
     * Cleanup flags
     */
    ValueCleanFlags cleanup;
    union {
        /**
         * Boolean representation of the lexical. Unspecified if type is not
         * VALUE_TYPE_BOOLEAN.
         */
        bool boolean;
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

/**
 * Clean a value according to its cleanup flags. This does not free the value
 * object itself.
 *
 * @param val the value to clean
 */
void model_value_clean(Value* val);

/**
 * Compare two values.
 *
 * @param arg1 first value
 * @param arg2 second value
 * @return 0 if arg1 == arg2, -1 if arg1 < arg2, 1 if arg1 > arg2,
 *         -2 if a type error occured
 */
int model_value_compare(Value* arg1, Value* arg2);

/**
 * Test the RDFterm-equality as defined in SPARQL 1.0, section 11.4.10.
 *
 * @param arg1 first value
 * @param arg2 second value
 * @return 1 if arg1 == arg2, 0 if arg1 != arg2, -1 if a type error occured
 */
int model_value_equal(Value* arg1, Value* arg2);

/**
 * Ensure val has a non-null lexical. If a new lexical is created, freeLexical
 * can be true and the caller should free it accordingly.
 */
void model_value_ensure_lexical(Value* val);

/**
 * @param val a value
 * @return a string representation of the value; it is the caller's
 *         responsibility to free it
 */
char* model_value_string(Value* val);

#endif // MODEL_H
