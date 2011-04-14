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
#ifndef CASTOR_MODEL_H
#define CASTOR_MODEL_H

#include <cstring>
#include <string>
#include <rasqal.h>

namespace castor {
    /**
     * Structure for global redland worlds
     */
    struct Redland {
        raptor_world *raptor;
        rasqal_world *rasqal;

        Redland() {
            rasqal = rasqal_new_world();
            raptor = rasqal_world_get_raptor(rasqal);
        }

        ~Redland() {
            rasqal_free_world(rasqal);
        }
    };

    /**
     * Singleton instance of Redland
     */
    extern Redland REDLAND;
}

#include "xsddecimal.h"

namespace castor {

/**
 * Standard value type ids. Higher ids means custom URI.
 */
enum ValueType {
    VALUE_TYPE_BLANK,         //!< blank node
    VALUE_TYPE_IRI,           //!< IRI
    VALUE_TYPE_PLAIN_STRING,  //!< plain literal
    VALUE_TYPE_TYPED_STRING,  //!< xsd:string
    VALUE_TYPE_BOOLEAN,       //!< xsd:boolean
    VALUE_TYPE_INTEGER,       //!< xsd:integer
    VALUE_TYPE_POSINTEGER,    //!< xsd:positiveInteger
    VALUE_TYPE_NONPOSINTEGER, //!< xsd:nonPositiveInteger
    VALUE_TYPE_NEGINTEGER,    //!< xsd:negativeInteger
    VALUE_TYPE_NONNEGINTEGER, //!< xsd:nonNegativeInteger
    VALUE_TYPE_BYTE,          //!< xsd:byte
    VALUE_TYPE_SHORT,         //!< xsd:short
    VALUE_TYPE_INT,           //!< xsd:int
    VALUE_TYPE_LONG,          //!< xsd:long
    VALUE_TYPE_UNSIGNEDBYTE,  //!< xsd:unsignedByte
    VALUE_TYPE_UNSIGNEDSHORT, //!< xsd:unsignedShort
    VALUE_TYPE_UNSIGNEDINT,   //!< xsd:unsignedInt
    VALUE_TYPE_UNSIGNEDLONG,  //!< xsd:unsignedLong
    VALUE_TYPE_FLOAT,         //!< xsd:float
    VALUE_TYPE_DOUBLE,        //!< xsd:double
    VALUE_TYPE_DECIMAL,       //!< xsd:decimal
    VALUE_TYPE_DATETIME,      //!< xsd:dateTime

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
    VALUE_TYPE_LAST_FLOATING = VALUE_TYPE_DOUBLE
};

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
extern char *VALUETYPE_URIS[VALUE_TYPE_FIRST_CUSTOM];

/**
 * Value cleanup flags
 */
enum ValueCleanFlags {
    /**
     * Nothing to do
     */
    VALUE_CLEAN_NOTHING = 0,
    /**
     * Datatype URI should be freed
     */
    VALUE_CLEAN_TYPE_URI = 1,
    /**
     * Lexical form should be freed
     */
    VALUE_CLEAN_LEXICAL = 2,
    /**
     * Type-specific data should be freed
     */
    VALUE_CLEAN_DATA = 4
};

/**
 * An RDF value
 */
struct Value {
    /**
     * ID of the value, starting from 1. Or 0 if not part of the store.
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
     * Lexical form. May be NULL if there is a native representation.
     */
    char* lexical;
    /**
     * Cleanup flags
     */
    ValueCleanFlags cleanup;
    union {
        /**
         * Language tag. Unspecified if type is not VALUE_TYPE_PLAIN_STRING.
         */
        struct {
            /**
             * Language tag id, starting from 0. Or -1 if unknown.
             */
            int language;
            /**
             * String representation of the language tag.
             */
            char* languageTag;
        };
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
         * xsd:decimal representation of the lexical. Unspecified if type is not
         * VALUE_TYPE_DECIMAL.
         */
        XSDDecimal* decimal;
        /**
         * Date and time representation of the lexical. Unspecified if type is
         * not VALUE_TYPE_DATETIME.
         */
        // TODO  DateTime* datetime;
    };

    Value() { cleanup = VALUE_CLEAN_NOTHING; }
    Value(const Value &val) { fillCopy(&val); }
    ~Value() { clean(); }

    /**
     * @param flag
     * @return whether the flag has been specified
     */
    bool hasCleanFlag(ValueCleanFlags flag) const { return cleanup & flag; }
    /**
     * Add a cleanup flag
     */
    void addCleanFlag(ValueCleanFlags flag) { cleanup = (ValueCleanFlags)(cleanup | flag); }
    /**
     * Remove a cleanup flag
     */
    void removeCleanFlag(ValueCleanFlags flag) { cleanup = (ValueCleanFlags)(cleanup & ~flag); }
    /**
     * Clean this structure according to cleanup.
     */
    void clean();

    /**
     * Make a copy of a value
     *
     * @param value the value
     */
    void fillCopy(const Value *value) {
        memcpy(this, value, sizeof(Value));
        cleanup = VALUE_CLEAN_NOTHING;
    }
    /**
     * Make a xsd:boolean
     *
     * @param value the value
     */
    void fillBoolean(bool value);
    /**
     * Make a xsd:integer
     *
     * @param value the value
     */
    void fillInteger(long value);
    /**
     * Make a xsd:double
     *
     * @param value the value
     */
    void fillFloating(double value);
    /**
     * Make a xsd:decimal
     *
     * @param value the value (ownership taken)
     */
    void fillDecimal(XSDDecimal *value);
    /**
     * Make a simple literal
     *
     * @param lexical the lexical form
     * @param freeLexical should the lexical form be freed on clean?
     */
    void fillSimpleLiteral(char *lexical, bool freeLexical);
    /**
     * Make an IRI
     *
     * @param lexical the IRI
     * @param freeLexical should the lexical form be freed on clean?
     */
    void fillIRI(char *lexical, bool freeLexical);

    /**
     * @return whether this value is a blank node
     */
    bool isBlank() const { return type == VALUE_TYPE_BLANK; }
    /**
     * @return whether this value is an IRI
     */
    bool isIRI() const { return type == VALUE_TYPE_IRI; }
    /**
     * @return whether this value is a literal
     */
    bool isLiteral() const { return type >= VALUE_TYPE_PLAIN_STRING; }
    /**
     * @return whether this value is a plain literal
     */
    bool isPlain() const { return type == VALUE_TYPE_PLAIN_STRING; }
    /**
     * @return whether this value is a typed string
     */
    bool isXSDString() const { return type == VALUE_TYPE_TYPED_STRING; }
    /**
     * @return whether this value is a boolean literal
     */
    bool isBoolean() const { return type == VALUE_TYPE_BOOLEAN; }
    /**
     * @return whether this value is a numeric literal
     */
    bool isNumeric() const { return IS_VALUE_TYPE_NUMERIC(type); }
    /**
     * @return whether this value is an integer literal
     */
    bool isInteger() const { return IS_VALUE_TYPE_INTEGER(type); }
    /**
     * @return whether this value is a floating literal
     */
    bool isFloating() const { return IS_VALUE_TYPE_FLOATING(type); }
    /**
     * @return whether this value is a decimal literal
     */
    bool isDecimal() const { return type == VALUE_TYPE_DECIMAL; }
    /**
     * @return whether this value is a dateTime literal
     */
    bool isDateTime() const { return type == VALUE_TYPE_DATETIME; }

    /**
     * Compare this value with another.
     *
     * @param o second value
     * @return 0 if this == o, -1 if this < o, 1 if this > o,
     *         -2 if a type error occured
     */
    int compare(const Value &o) const;

    bool operator<(const Value &o) const { return compare(o) == -1; }
    bool operator>(const Value &o) const { return compare(o) == 1; }
    bool operator==(const Value &o) const { return compare(o) == 0; }
    bool operator!=(const Value &o) const { return compare(o) != 0; }

    /**
     * Test the RDFterm-equality as defined in SPARQL 1.0, section 11.4.10.
     *
     * @param o second value
     * @return 0 if this RDF-equals o, 1 if !(this RDF-equals o),
     *         -1 if a type error occured
     */
    int rdfequals(const Value &o) const;

    /**
     * Ensure this value has a non-null lexical. A new lexical will be created
     * if necessary.
     */
    void ensureLexical();

    /**
     * @return a string representation of this value
     */
    std::string getString() const;
};

/**
 * Apply numeric type promotion rules to make v1 and v2 the same type to
 * evaluate arithmetic operators.
 *
 * @param v1 first numeric value
 * @param v2 second numeric value
 */
void promoteNumericType(Value &v1, Value &v2);

/**
 * Small statement structure containing ids.
 */
struct Statement {
    int subject;
    int predicate;
    int object;
};

}

#endif // CASTOR_MODEL_H
