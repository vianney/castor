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
#include <iostream>

#include "xsddecimal.h"

namespace castor {

class Store;

/**
 * An RDF value
 */
struct Value {

    /**
     * Value class ids
     */
    enum Class {
        CLASS_BLANK,
        CLASS_IRI,
        CLASS_SIMPLE_LITERAL,
        CLASS_TYPED_STRING,
        CLASS_BOOLEAN,
        CLASS_NUMERIC,
        CLASS_DATETIME,
        CLASS_OTHER
    };

    /**
     * Standard value type ids. Higher ids means custom URI.
     */
    enum Type {
        TYPE_BLANK,         //!< blank node
        TYPE_IRI,           //!< IRI
        TYPE_PLAIN_STRING,  //!< plain literal
        TYPE_TYPED_STRING,  //!< xsd:string
        TYPE_BOOLEAN,       //!< xsd:boolean
        TYPE_INTEGER,       //!< xsd:integer
        TYPE_POSINTEGER,    //!< xsd:positiveInteger
        TYPE_NONPOSINTEGER, //!< xsd:nonPositiveInteger
        TYPE_NEGINTEGER,    //!< xsd:negativeInteger
        TYPE_NONNEGINTEGER, //!< xsd:nonNegativeInteger
        TYPE_BYTE,          //!< xsd:byte
        TYPE_SHORT,         //!< xsd:short
        TYPE_INT,           //!< xsd:int
        TYPE_LONG,          //!< xsd:long
        TYPE_UNSIGNEDBYTE,  //!< xsd:unsignedByte
        TYPE_UNSIGNEDSHORT, //!< xsd:unsignedShort
        TYPE_UNSIGNEDINT,   //!< xsd:unsignedInt
        TYPE_UNSIGNEDLONG,  //!< xsd:unsignedLong
        TYPE_FLOAT,         //!< xsd:float
        TYPE_DOUBLE,        //!< xsd:double
        TYPE_DECIMAL,       //!< xsd:decimal
        TYPE_DATETIME,      //!< xsd:dateTime

        TYPE_FIRST_CUSTOM,
        TYPE_UNKOWN = -1,

        // internal values
        TYPE_FIRST_XSD = TYPE_TYPED_STRING,
        TYPE_LAST_XSD = TYPE_DATETIME,
        TYPE_FIRST_NUMERIC = TYPE_INTEGER,
        TYPE_LAST_NUMERIC = TYPE_DECIMAL,
        TYPE_FIRST_INTEGER = TYPE_INTEGER,
        TYPE_LAST_INTEGER = TYPE_UNSIGNEDLONG,
        TYPE_FIRST_FLOATING = TYPE_FLOAT,
        TYPE_LAST_FLOATING = TYPE_DOUBLE
    };

    /**
     * URIs corresponding to the value types
     */
    static char *TYPE_URIS[TYPE_FIRST_CUSTOM];

    /**
     * Value cleanup flags
     */
    enum CleanFlags {
        /**
         * Nothing to do
         */
        CLEAN_NOTHING = 0,
        /**
         * Datatype URI should be freed
         */
        CLEAN_TYPE_URI = 1,
        /**
         * Lexical form should be freed
         */
        CLEAN_LEXICAL = 2,
        /**
         * Type-specific data should be freed
         */
        CLEAN_DATA = 4
    };


    /**
     * ID of the value, starting from 1. Or 0 if not part of the store.
     */
    int id;
    /**
     * Datatype of the value. May be higher than the enumeration.
     */
    Type type;
    /**
     * URI of the datatype. NULL if type <= TYPE_PLAIN_STRING.
     */
    char* typeUri;
    /**
     * Lexical form. May be NULL if there is a native representation.
     */
    char* lexical;
    /**
     * Cleanup flags
     */
    CleanFlags cleanup;
    union {
        /**
         * Language tag. Unspecified if type is not TYPE_PLAIN_STRING.
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
         * range TYPE_FIRST_INTEGER..TYPE_LAST_INTEGER.
         */
        long integer;
        /**
         * Floating point representation of the lexical. Unspecified if type not
         * in range TYPE_FIRST_FLOATING..TYPE_LAST_FLOATING.
         */
        double floating;
        /**
         * xsd:decimal representation of the lexical. Unspecified if type is not
         * TYPE_DECIMAL.
         */
        XSDDecimal* decimal;
        /**
         * Date and time representation of the lexical. Unspecified if type is
         * not TYPE_DATETIME.
         */
        // TODO  DateTime* datetime;
    };

    Value() { cleanup = CLEAN_NOTHING; }
    Value(const Value &val) { fillCopy(&val); }
    ~Value() { clean(); }

    /**
     * @param flag
     * @return whether the flag has been specified
     */
    bool hasCleanFlag(CleanFlags flag) const { return cleanup & flag; }
    /**
     * Add a cleanup flag
     */
    void addCleanFlag(CleanFlags flag) {
        cleanup = static_cast<CleanFlags>(cleanup | flag);
    }
    /**
     * Remove a cleanup flag
     */
    void removeCleanFlag(CleanFlags flag) {
        cleanup = static_cast<CleanFlags>(cleanup & ~flag);
    }
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
        cleanup = CLEAN_NOTHING;
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
     * Retrieve the id of this value from the store and fill in the id field.
     * Do nothing if id > 0.
     * @param store the store
     */
    void fillId(Store *store);

    /**
     * @return whether this value is a blank node
     */
    bool isBlank() const { return type == TYPE_BLANK; }
    /**
     * @return whether this value is an IRI
     */
    bool isIRI() const { return type == TYPE_IRI; }
    /**
     * @return whether this value is a literal
     */
    bool isLiteral() const { return type >= TYPE_PLAIN_STRING; }
    /**
     * @return whether this value is a plain literal
     */
    bool isPlain() const { return type == TYPE_PLAIN_STRING; }
    /**
     * @return whether this value is a simple literal
     */
    bool isSimple() const { return isPlain() && language == 0; }
    /**
     * @return whether this value is a typed string
     */
    bool isXSDString() const { return type == TYPE_TYPED_STRING; }
    /**
     * @return whether this value is a boolean literal
     */
    bool isBoolean() const { return type == TYPE_BOOLEAN; }
    /**
     * @return whether this value is a numeric literal
     */
    bool isNumeric() const { return type >= TYPE_FIRST_NUMERIC &&
                                    type <= TYPE_LAST_NUMERIC; }
    /**
     * @return whether this value is an integer literal
     */
    bool isInteger() const { return type >= TYPE_FIRST_INTEGER &&
                                    type <= TYPE_LAST_INTEGER; }
    /**
     * @return whether this value is a floating literal
     */
    bool isFloating() const { return type >= TYPE_FIRST_FLOATING &&
                                     type <= TYPE_LAST_FLOATING; }
    /**
     * @return whether this value is a decimal literal
     */
    bool isDecimal() const { return type == TYPE_DECIMAL; }
    /**
     * @return whether this value is a dateTime literal
     */
    bool isDateTime() const { return type == TYPE_DATETIME; }

    /**
     * @return the class of this value
     */
    Class getClass() const {
        if(isBlank()) return CLASS_BLANK;
        else if(isIRI()) return CLASS_IRI;
        else if(isSimple()) return CLASS_SIMPLE_LITERAL;
        else if(isXSDString()) return CLASS_TYPED_STRING;
        else if(isBoolean()) return CLASS_BOOLEAN;
        else if(isNumeric()) return CLASS_NUMERIC;
        else if(isDateTime()) return CLASS_DATETIME;
        else return CLASS_OTHER;
    }

    /**
     * Compare this value with another.
     *
     * @param o second value
     * @return 0 if this == o, -1 if this < o, 1 if this > o,
     *         -2 if a type error occured
     */
    int compare(const Value &o) const;

    bool operator<(const Value &o) const;
    bool operator>(const Value &o) const { return o < *this; }
    bool operator==(const Value &o) const { return rdfequals(o) == 0; }
    bool operator!=(const Value &o) const { return rdfequals(o) != 0; }

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


    /**
     * Apply numeric type promotion rules to make v1 and v2 the same type to
     * evaluate arithmetic operators.
     *
     * @param v1 first numeric value
     * @param v2 second numeric value
     */
    static void promoteNumericType(Value &v1, Value &v2);
};

std::ostream& operator<<(std::ostream &out, const Value &val);
std::ostream& operator<<(std::ostream &out, const Value *val);

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
