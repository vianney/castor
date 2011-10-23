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
#include <cstdint>
#include <iostream>

#include "util.h"
#include "xsddecimal.h"

namespace castor {

class Store;

/**
 * An RDF value
 */
struct Value {

    ////////////////////////////////////////////////////////////////////////////
    // Static definitions

    typedef unsigned id_t;

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
        CLASS_OTHER,

        CLASSES_COUNT //!< number of classes
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

        TYPE_CUSTOM,        //!< custom datatype
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
    static char *TYPE_URIS[TYPE_CUSTOM];
    /**
     * length of predefined type URIs
     */
    static unsigned TYPE_URIS_LEN[TYPE_CUSTOM];

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



    ////////////////////////////////////////////////////////////////////////////
    // Public fields

    /**
     * ID of the value, starting from 1. Or 0 if not part of the store.
     */
    id_t id;
    /**
     * Datatype of the value. May be higher than the enumeration.
     */
    Type type;
    /**
     * URI of the datatype. NULL if type <= TYPE_PLAIN_STRING.
     * @note not necessarily null-terminated
     */
    char* typeUri;
    unsigned typeUriLen; //!< length of typeUri
    /**
     * Lexical form. May be NULL if there is a native representation.
     * @note not necessarily null-terminated
     */
    char* lexical;
    unsigned lexicalLen; //!< length of lexical
    /**
     * Cleanup flags
     */
    CleanFlags cleanup;
    /**
     * Has the types literal been interpreted?
     */
    bool isInterpreted;
    union {
        /**
         * Language tag. Unspecified if type is not TYPE_PLAIN_STRING.
         * @note not necessarily null-terminated
         */
        struct {
            char* language;
            unsigned languageLen;
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



    ////////////////////////////////////////////////////////////////////////////
    // Constructors

    /**
     * Create an uninitialized value
     */
    Value() : id(0), cleanup(CLEAN_NOTHING), isInterpreted(false) {}
    /**
     * Copy constructor
     */
    Value(const Value &val) : cleanup(CLEAN_NOTHING) { fillCopy(val); }
    /**
     * Create a value from a raptor_term
     * @param term the term
     */
    Value(const raptor_term *term);
    /**
     * Create a value from a rasqal_literal
     * @param literal the literal
     * @pre literl->type != RASQAL_LITERAL_VARIABLE
     */
    Value(const rasqal_literal *literal);
    ~Value() { clean(); }



    ////////////////////////////////////////////////////////////////////////////
    // Cleanup

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



    ////////////////////////////////////////////////////////////////////////////
    // Fill methods

    /**
     * Make a copy of a value
     *
     * @param value the value
     * @param deep should we perform a deep copy?
     */
    void fillCopy(const Value &value, bool deep=false);
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
     * @param len length of the lexical form
     * @param freeLexical should the lexical form be freed on clean?
     */
    void fillSimpleLiteral(char *lexical, unsigned len, bool freeLexical);
    /**
     * Make an IRI
     *
     * @param lexical the IRI
     * @param len length of the lexical form
     * @param freeLexical should the lexical form be freed on clean?
     */
    void fillIRI(char *lexical, unsigned len, bool freeLexical);
    /**
     * Make a blank node
     *
     * @param lexical the blank node identifier
     * @param len length of the lexical form
     * @param freeLexical should the lexical form be freed on clean?
     */
    void fillBlank(char *lexical, unsigned len, bool freeLexical);



    ////////////////////////////////////////////////////////////////////////////
    // Tests and comparisons

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
    bool isSimple() const { return isPlain() && language == NULL; }
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
     * @pre (id > 0 && o.id > 0) || ensureInterpreted()
     * @param o second value
     * @return 0 if this == o, -1 if this < o, 1 if this > o,
     *         -2 if a type error occured
     */
    int compare(const Value &o) const;

    /**
     * Test the RDFterm-equality as defined in SPARQL 1.0, section 11.4.10.
     *
     * @pre ensureLexical
     * @param o second value
     * @return 0 if this RDF-equals o, 1 if !(this RDF-equals o),
     *         -1 if a type error occured
     */
    int rdfequals(const Value &o) const;

    bool operator<(const Value &o) const;
    bool operator>(const Value &o) const { return o < *this; }
    bool operator==(const Value &o) const { return rdfequals(o) == 0; }
    bool operator!=(const Value &o) const { return rdfequals(o) != 0; }



    ////////////////////////////////////////////////////////////////////////////
    // Utility functions

    /**
     * Ensure this value has a non-null lexical. A new lexical will be created
     * if necessary.
     *
     * @pre lexical != NULL || isInterpreted == true
     */
    void ensureLexical();

    /**
     * Ensure this value is interpreted if it is a typed literal.
     * @pre lexical != NULL
     */
    void ensureInterpreted();

    /**
     * Compute the hashcode of this value
     * @pre the value must have a lexical form
     */
    uint32_t hash() const;

    /**
     * Apply numeric type promotion rules to make v1 and v2 the same type to
     * evaluate arithmetic operators.
     *
     * @note This also ensure that both values are interpreted.
     *
     * @param v1 first numeric value
     * @param v2 second numeric value
     */
    static void promoteNumericType(Value &v1, Value &v2);

private:
    /**
     * Check if the datatype is a known type and if so, update type accordingly,
     * cleaning the old typeUri if necessary.
     */
    void interpretDatatype();
};

std::ostream& operator<<(std::ostream &out, const Value &val);
std::ostream& operator<<(std::ostream &out, const Value *val);

/**
 * Small statement structure containing ids.
 */
struct Statement {
    Value::id_t subject;
    Value::id_t predicate;
    Value::id_t object;
};

}

#endif // CASTOR_MODEL_H
