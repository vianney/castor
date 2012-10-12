/* This file is part of Castor
 *
 * Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
 * Copyright (C) 2010-2012, Université catholique de Louvain
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
#ifndef CASTOR_MODEL_H
#define CASTOR_MODEL_H

#include <cstring>
#include <string>
#include <cstdint>
#include <iostream>

#include "util.h"
#include "xsddecimal.h"
#include "xsddatetime.h"

namespace castor {

class StringMapper;
class Store;

/**
 * A string that may be in a store.
 *
 * A string can be specified either by its id in the store (it is called
 * "resolved"), or by a real string and length (it is called "direct"), or
 * by both (it is then both direct and resolved).
 *
 * The serialized format of a string is
 * +----+------+--------+-----------------------------------------------+
 * | id | hash | length | string                                        |
 * +----+------+--------+-----------------------------------------------+
 *    4     4       4     length+1
 *
 * length is the length of string (NOT including terminal null)
 */
class String {
public:

    ////////////////////////////////////////////////////////////////////////////
    // Static definitions

    typedef unsigned id_t;
    static constexpr id_t UNKNOWN_ID = 0xffffffff;

    /**
     * @param id
     * @return whether id is a valid value from a store
     */
    static constexpr bool validId(id_t id) { return id > 0 && id != UNKNOWN_ID; }


    ////////////////////////////////////////////////////////////////////////////
    // Constructors, assignment and destructor

    /**
     * Construct a null string
     */
    String() : id_(0), str_(nullptr), length_(0), clean_(false) {}

    /**
     * Construct an indirect resolved string
     * @param id identifier of the string (must be valid)
     */
    String(id_t id) : id_(id), str_(nullptr), length_(0), clean_(false) {}

    /**
     * Construct a direct unresolved string
     *
     * @param s the string (!= nullptr)
     * @param len length of s or 0 if unknown
     * @param copy should we create a new string containing a copy of s?
     */
    String(const char* s, unsigned len=0, bool copy=false);

    /**
     * @see String(const char*, unsigned, bool)
     */
    String(const unsigned char* s, unsigned len=0, bool copy=false)
        : String(reinterpret_cast<const char*>(s), len, copy) {}

    /**
     * Create a direct unresolved string from a sprintf() format. Allocates a
     * new string.
     *
     * @see sprintf()
     */
    static String sprintf(const char* fmt, ...);

    /**
     * Construct a direct unresolved string form a Raptor URI. The string
     * will be copied to a new copy.
     *
     * @param uri the URI (!= nullptr)
     */
    String(raptor_uri* uri) : String(raptor_uri_as_string(uri), 0, true) {}

    /**
     * Deep copy constructor.
     *
     * @param o the string to copy
     * @param deep if true and o owns a direct string, allocate space for the
     *             direct string and copy it
     */
    String(const String& o, bool deep=true);

    /**
     * Move constructor.
     */
    String(String&& o) : clean_(false) { *this = std::move(o); }

    /**
     * Deep copy assignment.
     *
     * If o owns a direct string, this will allocate space and copy the direct
     * string.
     */
    String& operator=(const String& o);

    /**
     * Move assignment.
     */
    String& operator=(String&& o);

    ~String() { clean(); }


    ////////////////////////////////////////////////////////////////////////////
    // Serialization

    /**
     * Advance cur to skip the string underneath the pointer.
     */
    static void skip(Cursor& cur);

    /**
     * Deserialize a string.
     * @param cur the cursor pointing to the serialized string (will be moved)
     */
    explicit String(Cursor& cur);

    /**
     * @pre direct() && resolved() && !null()
     * @return the serialized string
     */
    Buffer serialize() const;


    ////////////////////////////////////////////////////////////////////////////
    // Tests and comparisons

    /**
     * @return whether this string has a valid id
     */
    bool validId() const { return validId(id_); }

    /**
     * @return whether this string is resolved
     */
    bool resolved() const { return id_ != UNKNOWN_ID; }

    /**
     * @return whether this string is direct
     */
    bool direct() const { return id_ == 0 || str_ != nullptr; }

    /**
     * @return whether this object represent a null string
     */
    bool null() const { return id_ == 0 && str_ == nullptr; }

    /**
     * Compare this string with another.
     *
     * @pre (validId() && o.resolved()) ||
     *      (resolved() && o.validId()) ||
     *      (direct() && o.direct())
     * @param o second string
     * @return 0 if this == o, <0 if this < o, >0 if this > o
     */
    int compare(const String& o) const;

    bool operator< (const String& o) const { return compare(o) <  0; }
    bool operator<=(const String& o) const { return compare(o) <= 0; }
    bool operator> (const String& o) const { return compare(o) >  0; }
    bool operator>=(const String& o) const { return compare(o) >= 0; }

    /**
     * @pre mapped() && !isNull()
     * @param s
     * @param len
     * @return whether this and s are the same string
     */
    bool equals(const char* s, unsigned len) const {
        assert(direct());
        assert(!null());
        return length_ == len && memcmp(str_, s, len) == 0;
    }

    /**
     * @pre (validId() && o.resolved()) ||
     *      (resolved() && o.validId()) ||
     *      (direct() && o.direct())
     * @return whether this and o are the same string
     */
    bool operator==(const String& o) const;
    bool operator!=(const String& o) const { return !(*this == o); }


    ////////////////////////////////////////////////////////////////////////////
    // Getters and setters

    /**
     * @return the identifier of the string starting from 1, or 0 not part of
     *         the store or UNKNOWN_ID if unknown
     */
    id_t id() const { return id_; }

    /**
     * Set the identifier of the string
     * @param id
     */
    void id(id_t id) { id_ = id; }

    /**
     * @pre direct()
     * @return the string
     */
    const char* str() const { return str_; }

    /**
     * @pre direct()
     * @return length of the string if direct
     */
    unsigned length() const { return length_; }


    ////////////////////////////////////////////////////////////////////////////
    // Setters

    /**
     * Copy string o to this string, creating a new str copy.
     *
     * @param o
     */
    void copy(const String& o);


    ////////////////////////////////////////////////////////////////////////////
    // Utility functions

    /**
     * Compute the hashcode of this value
     * @pre the value must be explicit
     */
    Hash::hash_t hash() const { return Hash::hash(str_, length_); }


private:

    /**
     * Perform cleaning tasks if needed. The state of the string is undefined
     * after the call of this method. Callee shall thus overwrite every field
     * afterwars.
     */
    void clean();

    ////////////////////////////////////////////////////////////////////////////
    // Private fields

    /**
     * The identifier of the string starting from 1, or 0 not part of the store
     * or UNKNOWN_ID if unknown.
     */
    id_t id_;
    /**
     * The string or nullptr if indirect.
     */
    const char* str_;
    /**
     * Length of the string if str != nullptr
     */
    unsigned length_;
    /**
     * Do we own str_ and do we have to delete it?
     */
    bool clean_;
};

std::ostream& operator<<(std::ostream& out, const String& s);

/**
 * An RDF value
 *
 * The serialized format of a value is
 * +----+----------+-------------+------+-----+---------+
 * | id | category | numCategory | type | tag | lexical |
 * +----+----------+-------------+------+-----+---------+
 *    4       2           2          4     4       4
 *
 * Total length is always 20 bytes.
 */
class Value {
public:

    ////////////////////////////////////////////////////////////////////////////
    // Static definitions

    typedef unsigned id_t;
    static constexpr id_t UNKNOWN_ID = 0xffffffff;

    /**
     * @param id
     * @return whether id is a valid value from a store
     */
    static constexpr bool validId(id_t id) { return id > 0 && id != UNKNOWN_ID; }

    /**
     * Value category ids
     */
    enum Category {
        CAT_BLANK,          //!< blank nodes
        CAT_URI,            //!< URIref
        CAT_SIMPLE_LITERAL, //!< simple literal (without language tag)
        CAT_TYPED_STRING,   //!< typed xsd:string
        CAT_BOOLEAN,        //!< xsd:boolean
        CAT_NUMERIC,        //!< numeric literal
        CAT_DATETIME,       //!< xsd:datetime
        CAT_PLAIN_LANG,     //!< plain literals with language tag
        CAT_OTHER           //!< literal with non-recognized datatype
    };
    //! Number of categories in Category
    static constexpr int CATEGORIES = CAT_OTHER + 1;

    /**
     * Numerical subcategory for CAT_NUMERIC
     */
    enum NumCategory {
        NUM_INTEGER,
        NUM_FLOATING,
        NUM_DECIMAL
    };
    static constexpr NumCategory NUM_NONE = static_cast<NumCategory>(0);


    ////////////////////////////////////////////////////////////////////////////
    // Constructors and assignment

    /**
     * Create an uninitialized value
     */
    Value() : id_(UNKNOWN_ID), interpreted_(INTERPRETED_NONE) {}
    /**
     * Copy constructor
     */
    Value(const Value& val) : interpreted_(INTERPRETED_NONE) { fillCopy(val, true); }
    /**
     * Move constructor
     */
    Value(Value&& val) : interpreted_(INTERPRETED_NONE) { fillMove(val); }
    /**
     * Create a value from a raptor_term
     * @param term the term
     */
    Value(const raptor_term* term);
    /**
     * Create a value from a rasqal_literal
     * @param literal the literal
     * @pre literl->type != RASQAL_LITERAL_VARIABLE
     */
    Value(const rasqal_literal* literal);
    ~Value() { clean(); }

    /**
     * Assignment operator (copy semantics)
     */
    Value& operator=(const Value& val) { fillCopy(val, true); return *this; }
    /**
     * Assignment operator (move semantics)
     */
    Value& operator=(Value&& val) { fillMove(val); return *this; }


    ////////////////////////////////////////////////////////////////////////////
    // Serialization

    /**
     * Size of a serialized value in bytes.
     */
    static constexpr std::size_t SERIALIZED_SIZE = 20;

    /**
     * Advance cur to skip the value underneath the pointer.
     * @param cur
     */
    static void skip(Cursor& cur) { cur += SERIALIZED_SIZE; }

    /**
     * Deserialize a value.
     * @param cur the cursor pointing to the serialized value (will be moved)
     */
    explicit Value(Cursor& cur);

    /**
     * @return the serialized value
     */
    Buffer serialize() const;



    ////////////////////////////////////////////////////////////////////////////
    // Factory methods

    /**
     * Make a copy of a value
     *
     * @param value the value
     * @param deep should we perform a deep copy, i.e., recreate every owned
     *             pointers (unowned pointers will be left as is)?
     */
    void fillCopy(const Value& value, bool deep=true);
    /**
     * Make a copy of a value, taking over the ownership of the pointers.
     *
     * @param value the value
     */
    void fillMove(Value& value);
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
    void fillDecimal(XSDDecimal* value);
    /**
     * Make a simple literal
     *
     * @param lex the lexical form
     */
    void fillSimpleLiteral(String&& lex);
    /**
     * Make a URI
     *
     * @param lex the lexical form of the URI
     */
    void fillURI(String&& lex);



    ////////////////////////////////////////////////////////////////////////////
    // Tests and comparisons

    /**
     * @return whether this value has a valid id
     */
    bool validId() const { return validId(id_); }

    /**
     * @return whether this value is a blank node
     */
    bool isBlank() const { return category_ == CAT_BLANK; }
    /**
     * @return whether this value is a URI
     */
    bool isURI() const { return category_ == CAT_URI; }
    /**
     * @return whether this value is a literal
     */
    bool isLiteral() const { return category_ > CAT_URI; }
    /**
     * @return whether this value is a plain literal
     */
    bool isPlain() const { return category_ == CAT_SIMPLE_LITERAL ||
                                  category_ == CAT_PLAIN_LANG; }
    /**
     * @return whether this value is a typed literal
     */
    bool isTyped() const { return category_ > CAT_SIMPLE_LITERAL &&
                                  category_ != CAT_PLAIN_LANG; }
    /**
     * @return whether this value is a simple literal
     */
    bool isSimple() const { return category_ == CAT_SIMPLE_LITERAL; }
    /**
     * @return whether this value is a plain literal with language tag
     */
    bool isPlainWithLang() const { return category_ == CAT_PLAIN_LANG; }
    /**
     * @return whether this value is a typed string
     */
    bool isXSDString() const { return category_ == CAT_TYPED_STRING; }
    /**
     * @return whether this value is a boolean literal
     */
    bool isBoolean() const { return category_ == CAT_BOOLEAN; }
    /**
     * @return whether this value is a numeric literal
     */
    bool isNumeric() const { return category_ == CAT_NUMERIC; }
    /**
     * @return whether this value is an integer literal
     */
    bool isInteger() const { return category_ == CAT_NUMERIC &&
                                    numCategory_ == NUM_INTEGER; }
    /**
     * @return whether this value is a floating literal
     */
    bool isFloating() const { return category_ == CAT_NUMERIC &&
                                     numCategory_ == NUM_FLOATING; }
    /**
     * @return whether this value is a decimal literal
     */
    bool isDecimal() const { return category_ == CAT_NUMERIC &&
                                    numCategory_ == NUM_DECIMAL; }
    /**
     * @return whether this value is a dateTime literal
     */
    bool isDateTime() const { return category_ == CAT_DATETIME; }
    /**
     * @return whether this value is a literal with unrecognized datatype
     */
    bool isOtherLiteral() const { return category_ == CAT_OTHER; }

    /**
     * @return whether this value can appear in a SPARQL comparison
     */
    bool isComparable() const {
        return isSimple() || isXSDString() || isBoolean() || isNumeric() ||
               isDateTime();
    }

    /**
     * Compare this value with another.
     *
     * @pre (id > 0 && o.id > 0) || ensureInterpreted()
     * @param o second value
     * @return 0 if this == o, -1 if this < o, 1 if this > o,
     *         -2 if a type error occured
     */
    int compare(const Value& o) const;

    /**
     * Test for the equality of this value with another.
     * XPath equality will be used if possible, otherwise resort to RDFterm
     * equality as defined in SPARQL 1.0, section 11.4.10.
     *
     * @pre ensureLexical
     * @param o second value
     * @return 0 if this = o, 1 if this != o,
     *         -1 if a type error occured
     */
    int equals(const Value& o) const;

    /**
     * Compare using a total order compatible with compare().
     */
    bool operator<(const Value& o) const;
    bool operator>(const Value& o) const { return o < *this; }

    /**
     * Test if this and o are the same RDF term as defined in SPARQL 1.0,
     * section 11.4.11.
     *
     * @param o
     * @return true if sameTerm(this, o), false otherwise
     */
    bool operator==(const Value& o) const;
    bool operator!=(const Value& o) const { return !(*this == o); }



    ////////////////////////////////////////////////////////////////////////////
    // Getters and setters

    /**
     * @return the ID of the value, starting from 1. Or 0 if not part of the
     *         store. Or UNKNOWN_ID if unknown.
     */
    id_t id() const { return id_; }

    /**
     * Set the ID of the value.
     * @param id
     */
    void id(id_t id) { id_ = id; }

    /**
     * @return the category of the value
     */
    Category category() const { return category_; }

    /**
     * @pre isNumeric()
     * @return the numerical subcategory of the value
     */
    NumCategory numCategory() const { assert(isNumeric());
                                      return numCategory_; }

    /**
     * Lexical form. May be null if interpreted().
     */
    const String& lexical() const { return lexical_; }
          String& lexical()       { return lexical_; }

    /**
     * Set the lexical form
     * @param lexical
     */
    void lexical(String&& lexical) { lexical_ = std::move(lexical); }

    /**
     * @pre isTyped()
     * @return the datatype of the value as the id of an IRI value or
     *         UNKNOWN_ID if the datatype is solely specified by
     *         datatypeLex()
     */
    id_t datatypeId() const { assert(isTyped());
                              return datatype_; }

    /**
     * Set the datatype as the id of an IRI value
     * @pre isTyped()
     * @param id
     */
    void datatypeId(id_t id) { assert(isTyped()); datatype_ = id; }

    /**
     * @pre isTyped()
     * @return the datatype URI lexical. May be null if
     *         datatypeId() != UNKNOWN_ID.
     */
    const String& datatypeLex() const { assert(isTyped());
                                        return tag_; }
          String& datatypeLex()       { assert(isTyped());
                                        return tag_; }

    /**
     * Set the datatype URI lexical
     * @pre isTyped()
     * @param lex
     */
    void datatypeLex(String&& lex) { assert(isTyped());
                                     tag_ = std::move(lex); }

    /**
     * @pre isPlainWithLang()
     * @return language tag
     */
    const String& language() const { assert(isPlainWithLang());
                                     return tag_;}
          String& language()       { assert(isPlainWithLang());
                                     return tag_;}

    /**
     * Set the language tag
     * @pre isPlainWithLang()
     * @param tag
     */
    void language(String&& tag) { assert(isPlainWithLang());
                                  tag_ = std::move(tag); }

    /**
     * @return whether the literal been interpreted
     */
    bool interpreted() const { return interpreted_ != INTERPRETED_NONE; }

    /**
     * @pre isBoolean() && interpreted()
     * @return boolean representation of the lexical
     */
    bool boolean() const { assert(isBoolean() && interpreted());
                           return boolean_; }

    /**
     * @pre isInteger() && interpreted()
     * @return integer representation of the lexical
     */
    long integer() const { assert(isInteger() && interpreted());
                           return integer_; }

    /**
     * @pre isFloating() && interpreted()
     * @return floating point representation of the lexical
     */
    double floating() const { assert(isFloating() && interpreted());
                              return floating_; }

    /**
     * @pre isDecimal() && interpreted()
     * @return xsd:decimal representation of the lexical
     */
    const XSDDecimal& decimal() const { assert(isDecimal() && interpreted());
                                        return *decimal_; }

    /**
     * @pre isDateTime() && interpreted()
     * @return date and time representation of the lexical
     */
    const XSDDateTime& datetime() const { assert(isDateTime() && interpreted());
                                          return *datetime_; }



    ////////////////////////////////////////////////////////////////////////////
    // Utility functions

    /**
     * Ensure this value has a non-null lexical. A new lexical will be created
     * if necessary.
     *
     * @pre !lexical.null() || isInterpreted
     * @return this
     */
    Value& ensureLexical();

    /**
     * Ensure this value is interpreted if it is a typed literal.
     * @param mapper string mapper for the lexical
     * @return this
     */
    Value& ensureInterpreted(const StringMapper& mapper);

    /**
     * Ensure every string is direct.
     * @param mapper string mapper
     * @return this
     */
    Value& ensureDirectStrings(const StringMapper& mapper);

    /**
     * Ensure every string is resolved.
     * @param mapper string mapper
     * @return this
     */
    Value& ensureResolvedStrings(const Store& store);

    /**
     * Compute the hashcode of this value
     * @pre ensureLexical() && ensureDirectStrings()
     */
    Hash::hash_t hash() const;

    /**
     * Apply numeric type promotion rules to make v1 and v2 the same type to
     * evaluate arithmetic operators.
     *
     * @note This also ensure that both values are interpreted.
     *
     * @pre v1.isInterpreted && v2.isInterpreted
     * @param v1 first numeric value
     * @param v2 second numeric value
     */
    static void promoteNumericType(Value& v1, Value& v2);

private:
    /**
     * Perform cleanup. The state is undefined after this call, so this value
     * shall be reinitialized before reuse.
     */
    void clean();


    ////////////////////////////////////////////////////////////////////////////
    // Private fields

    /**
     * ID of the value, starting from 1. Or 0 if not part of the store.
     * Or UNKNOWN_ID if unknown.
     */
    id_t id_;
    /**
     * Category of the value
     */
    Category category_;
    /**
     * Numerical subcategory of the value if category == CAT_NUMERIC
     */
    NumCategory numCategory_;
    /**
     * Lexical form. May be null if interpreted_ is true.
     */
    String lexical_;
    /**
     * Datatype of the value as the id of an IRI value. 0 if not part of the
     * store. UNKNOWN_ID if the datatype is solely specified by tag_
     * (which must then not be null).
     */
    id_t datatype_;
    /**
     * Datatype URI lexical (if isTyped()) or language tag (if
     * isPlainWithLang()).
     *
     * If isTyped() and datatype_ != UNKNOWN_ID, this string may be null,
     * indicating the lexical should be retrieved by looking up the value
     * corresponding to datatype_.
     */
    String tag_;

    enum Interpreted {
        INTERPRETED_NONE,    //!< not interpreted
        INTERPRETED_UNOWNED, //!< interpreted, pointer unowned
        INTERPRETED_OWNED    //!< interpreted, pointer owned
    };

    /**
     * Has the types literal been interpreted?
     */
    Interpreted interpreted_;

    union {
        /**
         * Boolean representation of the lexical. Unspecified if !isBoolean().
         */
        bool boolean_;
        /**
         * Integer representation of the lexical. Unspecified if !isInteger().
         */
        long integer_;
        /**
         * Floating point representation of the lexical. Unspecified if
         * !isFloating().
         */
        double floating_;
        /**
         * xsd:decimal representation of the lexical. Unspecified if
         * !isDecimal().
         */
        XSDDecimal* decimal_;
        /**
         * Date and time representation of the lexical. Unspecified if
         * !isDateTime().
         */
        XSDDateTime* datetime_;
    };
};

std::ostream& operator<<(std::ostream& out, const Value& val);
std::ostream& operator<<(std::ostream& out, const Value* val);

static inline Value::Category& operator++(Value::Category& cat) {
    cat = static_cast<Value::Category>(cat + 1);
    return cat;
}
static inline Value::Category& operator--(Value::Category& cat) {
    cat = static_cast<Value::Category>(cat - 1);
    return cat;
}

/**
 * Range of value identifiers.
 */
struct ValueRange {
    Value::id_t from;
    Value::id_t to;

    bool empty() { return to < from; }
    bool contains(Value::id_t id) { return id >= from && id <= to; }

    // support C++11 iterators
    struct Iterator {
        Value::id_t id;

        Iterator(Value::id_t id) : id(id) {}
        Iterator(const Iterator& o) : id(o.id) {}
        Value::id_t operator*() const { return id; }
        bool operator!=(const Iterator& o) const { return id != o.id; }
        Iterator& operator++() { ++id; return *this; }
    };
    Iterator begin() { return from; }
    Iterator end() { return to + 1; }
};

/**
 * Small triple structure.
 */
template<class T>
struct BasicTriple {
    static constexpr int COMPONENTS = 3; //!< number of components
    T c[COMPONENTS]; //!< statement components

    // Constructors
    BasicTriple() = default;
    BasicTriple(T s, T p, T o) : c{s, p, o} {}

    // Copyable
    BasicTriple(const BasicTriple<T>&) = default;
    BasicTriple<T>& operator=(const BasicTriple<T>&) = default;

    // Reordering
    template<int C1, int C2, int C3>
    BasicTriple<T> reorder() const { return {c[C1], c[C2], c[C3]}; }

    // Accessors
    const T& operator[](unsigned i) const { return c[i]; }
          T& operator[](unsigned i)       { return c[i]; }

    // C++11 Iterator
    const T* begin() const { return c; }
          T* begin()       { return c; }
    const T* end()   const { return c + COMPONENTS; }
          T* end()         { return c + COMPONENTS; }

    // Comparator
    bool operator<(const BasicTriple<T>& o) const {
        return c[0] < o.c[0] ||
                (c[0] == o.c[0] && (c[1] < o.c[1] ||
                                    (c[1] == o.c[1] && c[2] < o.c[2])));
    }
};

template<class T>
std::ostream& operator<<(std::ostream& out, const BasicTriple<T>& t) {
    for(int i = 0; i < t.COMPONENTS; i++)
        out << (i == 0 ? "(" : ", ") << t[i];
    out << ")";
    return out;
}

}

#endif // CASTOR_MODEL_H
