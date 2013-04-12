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
#include "model.h"

#include <cstdio>
#include <cstdarg>
#include <cassert>
#include <sstream>
#include <algorithm>

#include "store.h"

namespace castor {

////////////////////////////////////////////////////////////////////////////////
// String utils

String::String(const char *s, unsigned len, bool copy) {
    assert(s != nullptr);
    id_ = UNKNOWN_ID;
    length_ = (len > 0 ? len : strlen(s));
    if(copy) {
        char* ns = new char[length_ + 1];
        memcpy(ns, s, length_ + 1);
        str_ = ns;
        clean_ = true;
    } else {
        str_ = s;
        clean_ = false;
    }
}

String String::sprintf(const char *fmt, ...) {
    String result;
    result.id_ = UNKNOWN_ID;
    va_list ap;
    va_start(ap, fmt);
    result.length_ = vsnprintf(nullptr, 0, fmt, ap);
    va_end(ap);
    char* s = new char[result.length_ + 1];
    va_start(ap, fmt);
    vsprintf(s, fmt, ap);
    va_end(ap);
    result.str_ = s;
    result.clean_ = true;
    return result;
}

String::String(const String &o, bool deep) {
    if(deep) {
        clean_ = false;
        *this = o;
    } else {
        id_ = o.id_;
        length_ = o.length_;
        str_ = o.str_;
        clean_ = false;
    }
}

String& String::operator=(const String& o) {
    clean();
    id_ = o.id_;
    length_ = o.length_;
    if(o.clean_) {
        assert(o.str_ != nullptr);
        char* s = new char[length_ + 1];
        memcpy(s, o.str_, length_ + 1);
        str_ = s;
        clean_ = true;
    } else {
        str_ = o.str_;
        clean_ = false;
    }
    return *this;
}

String& String::operator=(String&& o) {
    clean();
    id_ = o.id_;
    str_ = o.str_;
    length_ = o.length_;
    clean_ = o.clean_;
    o.clean_ = false;
    return *this;
}

void String::clean() {
    if(clean_)
        delete [] str_;
}

void String::skip(Cursor &cur) {
    cur += cur.peekInt(8) + 13;
}

String::String(Cursor &cur) {
    id_ = cur.readInt();
    cur.skipInt(); // skip hash
    length_ = cur.readInt();
    str_ = reinterpret_cast<const char*>(cur.get());
    clean_ = false;
    cur += length_ + 1;
}

Buffer String::serialize() const {
    assert(direct() && !null());
    Buffer buf(length_ + 13);
    buf.writeInt(id_);
    buf.writeInt(hash());
    buf.writeInt(length_);
    buf.write(reinterpret_cast<const unsigned char*>(str_), length_ + 1);
    return buf;
}


int String::compare(const String &o) const  {
    if(null() && o.null()) {
        return 0;
    } else if(null()) {
        return -1;
    } else if(o.null()) {
        return 1;
    } else if(resolved() && o.resolved() && (validId() || o.validId())) {
        if(id() == o.id()) return 0;
        else if(id() < o.id()) return -1;
        else return 1;
    } else {
        assert(direct());
        assert(o.direct());
        int cmp = memcmp(str(), o.str(), std::min(length(), o.length()));
        if(cmp) return cmp;
        else if(length() < o.length()) return -1;
        else if(length() > o.length()) return 1;
        else return 0;
    }
}

bool String::operator ==(const String &o) const {
    if(null() || o.null()) {
        return null() && o.null();
    } else if(resolved() && o.resolved() && (validId() || o.validId())) {
        return id() == o.id();
    } else {
        assert(o.direct());
        return equals(o.str(), o.length());
    }
}

void String::copy(const String &o) {
    assert(o.str_ != nullptr);
    id_ = o.id_;
    length_ = o.length_;
    char* s = new char[length_ + 1];
    memcpy(s, o.str_, o.length_ + 1);
    str_ = s;
}

std::ostream& operator<<(std::ostream& out, const String& s) {
    if(!s.null()) {
        assert(s.direct());
        out.write(s.str(), s.length());
    }
    return out;
}


////////////////////////////////////////////////////////////////////////////
// Constructors

namespace {

constexpr char XSD_PREFIX[] = "http://www.w3.org/2001/XMLSchema#";
constexpr unsigned XSD_PREFIX_LEN = sizeof(XSD_PREFIX) - 1;

struct XSDSuffix {
    const char* str;
    unsigned len;
    Value::Category category;
    Value::NumCategory numCategory;
};

static const XSDSuffix XSD_SUFFIXES[] = {
    { "string",             sizeof("string")-1,             Value::CAT_TYPED_STRING, Value::NUM_NONE },
    { "boolean",            sizeof("boolean")-1,            Value::CAT_BOOLEAN,      Value::NUM_NONE },
    { "integer",            sizeof("integer")-1,            Value::CAT_NUMERIC,      Value::NUM_INTEGER },
    { "positiveInteger",    sizeof("positiveInteger")-1,    Value::CAT_NUMERIC,      Value::NUM_INTEGER },
    { "nonPositiveInteger", sizeof("nonPositiveInteger")-1, Value::CAT_NUMERIC,      Value::NUM_INTEGER },
    { "negativeInteger",    sizeof("negativeInteger")-1,    Value::CAT_NUMERIC,      Value::NUM_INTEGER },
    { "nonNegativeInteger", sizeof("nonNegativeInteger")-1, Value::CAT_NUMERIC,      Value::NUM_INTEGER },
    { "byte",               sizeof("byte")-1,               Value::CAT_NUMERIC,      Value::NUM_INTEGER },
    { "short",              sizeof("short")-1,              Value::CAT_NUMERIC,      Value::NUM_INTEGER },
    { "int",                sizeof("int")-1,                Value::CAT_NUMERIC,      Value::NUM_INTEGER },
    { "long",               sizeof("long")-1,               Value::CAT_NUMERIC,      Value::NUM_INTEGER },
    { "unsignedByte",       sizeof("unsignedByte")-1,       Value::CAT_NUMERIC,      Value::NUM_INTEGER },
    { "unsignedShort",      sizeof("unsignedShort")-1,      Value::CAT_NUMERIC,      Value::NUM_INTEGER },
    { "unsignedInt",        sizeof("unsignedInt")-1,        Value::CAT_NUMERIC,      Value::NUM_INTEGER },
    { "unsignedLong",       sizeof("unsignedLong")-1,       Value::CAT_NUMERIC,      Value::NUM_INTEGER },
    { "float",              sizeof("float")-1,              Value::CAT_NUMERIC,      Value::NUM_FLOATING },
    { "double",             sizeof("double")-1,             Value::CAT_NUMERIC,      Value::NUM_FLOATING },
    { "decimal",            sizeof("decimal")-1,            Value::CAT_NUMERIC,      Value::NUM_DECIMAL },
    { "dateTime",           sizeof("dateTime")-1,           Value::CAT_DATETIME,     Value::NUM_NONE }
};

constexpr unsigned XSD_SUFFIX_MINLENGTH = 3;

}

Value::Value(const raptor_term* term) {
    id_ = UNKNOWN_ID;
    numCategory_ = NUM_NONE;
    interpreted_ = INTERPRETED_NONE;
    switch(term->type) {
    case RAPTOR_TERM_TYPE_BLANK:
        category_ = CAT_BLANK;
        lexical_ = String(term->value.blank.string,
                          term->value.blank.string_len, true);
        datatype_ = 0;
        assert(tag_.null());
        break;
    case RAPTOR_TERM_TYPE_URI:
        category_ = CAT_URI;
        lexical_ = String(term->value.uri);
        datatype_ = 0;
        assert(tag_.null());
        break;
    case RAPTOR_TERM_TYPE_LITERAL:
        lexical_ = String(term->value.literal.string,
                          term->value.literal.string_len, true);
        if(term->value.literal.datatype == nullptr) {
            datatype_ = 0;
            if(term->value.literal.language == nullptr ||
               term->value.literal.language_len == 0) {
                category_ = CAT_SIMPLE_LITERAL;
                assert(tag_.null());
            } else {
                category_ = CAT_PLAIN_LANG;
                tag_ = String(term->value.literal.language,
                              term->value.literal.language_len, true);
            }
        } else {
            category_ = CAT_OTHER;
            datatype_ = UNKNOWN_ID;
            tag_ = String(term->value.literal.datatype);
            if(tag_.length() >= XSD_PREFIX_LEN + XSD_SUFFIX_MINLENGTH &&
                    memcmp(tag_.str(), XSD_PREFIX, XSD_PREFIX_LEN) == 0) {
                const char* fragment = tag_.str() + XSD_PREFIX_LEN;
                unsigned fragmentLen = tag_.length() - XSD_PREFIX_LEN;
                for(const XSDSuffix& suffix : XSD_SUFFIXES) {
                    if(fragmentLen == suffix.len &&
                            memcmp(fragment, suffix.str, fragmentLen) == 0) {
                        category_ = suffix.category;
                        numCategory_ = suffix.numCategory;
                        break;
                    }
                }
            }
        }
        break;
    default:
        assert(false);
    }
}

Value::Value(const rasqal_literal* literal) {
    id_ = UNKNOWN_ID;
    numCategory_ = NUM_NONE;
    interpreted_ = INTERPRETED_NONE;
    if(literal->type == RASQAL_LITERAL_URI) {
        lexical_ = String(literal->value.uri);
    } else {
        lexical_ = String(literal->string, literal->string_len, true);
    }
    switch(literal->type) {
    case RASQAL_LITERAL_BLANK:
        category_ = CAT_BLANK;
        datatype_ = 0;
        assert(tag_.null());
        break;
    case RASQAL_LITERAL_URI:
        category_ = CAT_URI;
        datatype_ = 0;
        assert(tag_.null());
        break;
    case RASQAL_LITERAL_STRING:
        datatype_ = 0;
        if(literal->language != nullptr && literal->language[0] != '\0') {
            category_ = CAT_PLAIN_LANG;
            tag_ = String(literal->language, 0, true);
        } else {
            category_ = CAT_SIMPLE_LITERAL;
            assert(tag_.null());
        }
        break;
    case RASQAL_LITERAL_XSD_STRING:
        category_ = CAT_TYPED_STRING;
        datatype_ = UNKNOWN_ID;
        tag_ = String(       "http://www.w3.org/2001/XMLSchema#string",
                      sizeof("http://www.w3.org/2001/XMLSchema#string") - 1);
        break;
    case RASQAL_LITERAL_BOOLEAN:
        category_ = CAT_BOOLEAN;
        datatype_ = UNKNOWN_ID;
        tag_ = String(       "http://www.w3.org/2001/XMLSchema#boolean",
                      sizeof("http://www.w3.org/2001/XMLSchema#boolean") - 1);
        break;
    case RASQAL_LITERAL_INTEGER:
        category_ = CAT_NUMERIC;
        numCategory_ = NUM_INTEGER;
        datatype_ = UNKNOWN_ID;
        tag_ = String(literal->datatype); // we do not know which integer type
        break;
    case RASQAL_LITERAL_FLOAT:
        category_ = CAT_NUMERIC;
        numCategory_ = NUM_FLOATING;
        datatype_ = UNKNOWN_ID;
        tag_ = String(       "http://www.w3.org/2001/XMLSchema#float",
                      sizeof("http://www.w3.org/2001/XMLSchema#float") - 1);
        break;
    case RASQAL_LITERAL_DOUBLE:
        category_ = CAT_NUMERIC;
        numCategory_ = NUM_FLOATING;
        datatype_ = UNKNOWN_ID;
        tag_ = String(       "http://www.w3.org/2001/XMLSchema#double",
                      sizeof("http://www.w3.org/2001/XMLSchema#double") - 1);
        break;
    case RASQAL_LITERAL_DECIMAL:
        category_ = CAT_NUMERIC;
        numCategory_ = NUM_DECIMAL;
        datatype_ = UNKNOWN_ID;
        tag_ = String(       "http://www.w3.org/2001/XMLSchema#decimal",
                      sizeof("http://www.w3.org/2001/XMLSchema#decimal") - 1);
        break;
    case RASQAL_LITERAL_DATETIME:
        category_ = CAT_DATETIME;
        datatype_ = UNKNOWN_ID;
        tag_ = String(       "http://www.w3.org/2001/XMLSchema#dateTime",
                      sizeof("http://www.w3.org/2001/XMLSchema#dateTime") - 1);
        break;
    case RASQAL_LITERAL_UDT:
        category_ = CAT_OTHER;
        datatype_ = UNKNOWN_ID;
        tag_ = String(literal->datatype);
        break;
    default:
        assert(false);
    }
}


////////////////////////////////////////////////////////////////////////////////
// Serialization

Value::Value(Cursor& cur) {
    id_ = cur.readInt();
    category_ = static_cast<Category>(cur.readShort());
    numCategory_ = static_cast<NumCategory>(cur.readShort());
    datatype_ = cur.readInt();
    tag_ = String(cur.readInt());
    assert(isTyped() || isPlainWithLang() || tag_.null());
    lexical_ = String(cur.readInt());
    long n = cur.readSignedLong();
    if(isNumeric())
        numapprox_ = NumRange(n);
    interpreted_ = INTERPRETED_NONE;
}

Buffer Value::serialize() const {
    Buffer buf(SERIALIZED_SIZE);
    buf.writeInt(id_);
    buf.writeShort(category_);
    buf.writeShort(numCategory_);
    buf.writeInt(datatype_);
    buf.writeInt(tag_.id());
    buf.writeInt(lexical_.id());
    buf.writeSignedLong(isNumeric() ? numapprox().lower() : 0);
    return buf;
}



////////////////////////////////////////////////////////////////////////////////
// Cleanup

void Value::clean() {
    if(interpreted_ == INTERPRETED_OWNED) {
        if(isDecimal())
            delete decimal_;
        if(isDateTime())
            delete datetime_;
        interpreted_ = INTERPRETED_NONE;
    }
}



////////////////////////////////////////////////////////////////////////////////
// Fill methods

void Value::fillCopy(const Value& value, bool deep)  {
    clean();
    id_ = value.id_;
    category_ = value.category_;
    numCategory_ = value.numCategory_;
    lexical_ = String(value.lexical_, deep);
    datatype_ = value.datatype_;
    tag_ = String(value.tag_, deep);
    numapprox_ = value.numapprox_;
    interpreted_ = value.interpreted_;
    if(interpreted()) {
        if(isBoolean()) {
            boolean_ = value.boolean_;
        } else if(isInteger()) {
            integer_ = value.integer_;
        } else if(isFloating()) {
            floating_ = value.floating_;
        } else if(isDecimal()) {
            if(deep && interpreted_ == INTERPRETED_OWNED) {
                decimal_ = new XSDDecimal(*value.decimal_);
            } else {
                decimal_ = value.decimal_;
                interpreted_ = INTERPRETED_UNOWNED;
            }
        } else if(isDateTime()) {
            if(deep && interpreted_ == INTERPRETED_OWNED) {
                datetime_ = new XSDDateTime(*value.datetime_);
            } else {
                datetime_ = value.datetime_;
                interpreted_ = INTERPRETED_UNOWNED;
            }
        }
    }
}

void Value::fillMove(Value& value)  {
    clean();
    id_ = value.id_;
    category_ = value.category_;
    numCategory_ = value.numCategory_;
    lexical_ = std::move(value.lexical_);
    datatype_ = value.datatype_;
    tag_ = std::move(value.tag_);
    numapprox_ = value.numapprox_;
    interpreted_ = value.interpreted_;
    if(interpreted()) {
        if(isBoolean())
            boolean_ = value.boolean_;
        else if(isInteger())
            integer_ = value.integer_;
        else if(isFloating())
            floating_ = value.floating_;
        else if(isDecimal())
            decimal_ = value.decimal_;
        else if(isDateTime())
            datetime_ = value.datetime_;
        value.interpreted_ = INTERPRETED_UNOWNED;
    }
}

void Value::fillBoolean(bool value) {
    clean();
    id_ = UNKNOWN_ID;
    category_ = CAT_BOOLEAN;
    numCategory_ = NUM_NONE;
    lexical_ = String();
    datatype_ = UNKNOWN_ID;
    tag_ = String(       "http://www.w3.org/2001/XMLSchema#boolean",
                  sizeof("http://www.w3.org/2001/XMLSchema#boolean") - 1);
    numapprox_ = NumRange();
    interpreted_ = INTERPRETED_UNOWNED;
    boolean_ = value;
}

void Value::fillInteger(long value) {
    clean();
    id_ = UNKNOWN_ID;
    category_ = CAT_NUMERIC;
    numCategory_ = NUM_INTEGER;
    lexical_ = String();
    datatype_ = UNKNOWN_ID;
    tag_ = String(       "http://www.w3.org/2001/XMLSchema#integer",
                  sizeof("http://www.w3.org/2001/XMLSchema#integer") - 1);
    numapprox_ = NumRange(value);
    interpreted_ = INTERPRETED_UNOWNED;
    integer_ = value;
}

void Value::fillFloating(double value) {
    clean();
    id_ = UNKNOWN_ID;
    category_ = CAT_NUMERIC;
    numCategory_ = NUM_FLOATING;
    lexical_ = String();
    datatype_ = UNKNOWN_ID;
    tag_ = String(       "http://www.w3.org/2001/XMLSchema#double",
                  sizeof("http://www.w3.org/2001/XMLSchema#double") - 1);
    numapprox_ = NumRange(value);
    interpreted_ = INTERPRETED_UNOWNED;
    floating_ = value;
}

void Value::fillDecimal(XSDDecimal* value) {
    clean();
    id_ = UNKNOWN_ID;
    category_ = CAT_NUMERIC;
    numCategory_ = NUM_DECIMAL;
    lexical_ = String();
    datatype_ = UNKNOWN_ID;
    tag_ = String(       "http://www.w3.org/2001/XMLSchema#decimal",
                  sizeof("http://www.w3.org/2001/XMLSchema#decimal") - 1);
    numapprox_ = NumRange(*value);
    interpreted_ = INTERPRETED_OWNED;
    decimal_ = value;
}

void Value::fillSimpleLiteral(String&& lex) {
    clean();
    id_ = UNKNOWN_ID;
    category_ = CAT_SIMPLE_LITERAL;
    numCategory_ = NUM_NONE;
    lexical_ = std::move(lex);
    datatype_ = 0;
    tag_ = String();
    numapprox_ = NumRange();
    interpreted_ = INTERPRETED_UNOWNED;
}

void Value::fillURI(String&& lex) {
    clean();
    id_ = UNKNOWN_ID;
    category_ = CAT_URI;
    numCategory_ = NUM_NONE;
    lexical_ = std::move(lex);
    datatype_ = 0;
    tag_ = String();
    numapprox_ = NumRange();
    interpreted_ = INTERPRETED_UNOWNED;
}



////////////////////////////////////////////////////////////////////////////////
// Comparisons

int Value::compare(const Value& o) const {
    if(isNumeric() && o.isNumeric()) {
        if(!numapprox().empty() && !o.numapprox().empty()) {
            // fast path
            if(numapprox() < o.numapprox()) return -1;
            if(numapprox() > o.numapprox()) return 1;
        }
        if(isInteger() && o.isInteger()) {
            long diff = integer() - o.integer();
            if(diff < 0) return -1;
            else if(diff > 0) return 1;
            else return 0;
        } else if(isDecimal() && o.isDecimal()) {
            // FIXME: compare decimal with integer??
            return decimal().compare(o.decimal());
        } else {
            double d1 = isFloating() ? floating() :
                        (isDecimal() ? decimal().getFloat() : integer());
            double d2 = o.isFloating() ? o.floating() :
                        (o.isDecimal() ? o.decimal().getFloat() : o.integer());
            double diff = d1 - d2;
            if(diff < .0) return -1;
            else if(diff > .0) return 1;
            else return 0;
        }
    } else if((isSimple() && o.isSimple()) ||
              (isXSDString() && o.isXSDString())) {
        int i = lexical().compare(o.lexical());
        if(i < 0) return -1;
        else if(i > 0) return 1;
        else return 0;
    } else if(isBoolean() && o.isBoolean()) {
        return (boolean() ? 1 : 0) - (o.boolean() ? 1 : 0);
    } else if(isDateTime() && o.isDateTime()) {
        return datetime().compare(o.datetime());
    } else {
        return -2;
    }
}

int Value::equals(const Value& o) const {
    if(isNumeric() && o.isNumeric()) {
        if(validId() && o.validId())
            return id() == o.id() ? 0 : 1;
        if(!numapprox().empty() && !o.numapprox().empty() &&
                numapprox() != o.numapprox())
            return 0;
        if(isInteger() && o.isInteger()) {
            return integer() == o.integer() ? 0 : 1;
        } else if(isDecimal() && o.isDecimal()) {
            // FIXME: compare decimal with integer??
            return decimal() == o.decimal() ? 0 : 1;
        } else {
            double d1 = isFloating() ? floating() :
                        (isDecimal() ? decimal().getFloat() : integer());
            double d2 = o.isFloating() ? o.floating() :
                        (o.isDecimal() ? o.decimal().getFloat() : o.integer());
            return d1 == d2 ? 0 : 1;
        }
    } else if((isSimple() && o.isSimple()) ||
              (isXSDString() && o.isXSDString())) {
        if(validId() && o.validId())
            return id() == o.id() ? 0 : 1;
        return lexical() == o.lexical() ? 0 : 1;
    } else if(isBoolean() && o.isBoolean()) {
        if(validId() && o.validId())
            return id() == o.id() ? 0 : 1;
        return boolean() == o.boolean() ? 0 : 1;
    } else if(isDateTime() && o.isDateTime()) {
        if(validId() && o.validId())
            return id() == o.id() ? 0 : 1;
        return datetime() == o.datetime() ? 0 : 1;
    } else {
        // fallback to RDFterm equality, which is sameTerm with type error
        if(*this == o)
            return 0;
        else if(isLiteral() && o.isLiteral())
            return -1;
        else
            return 1;
    }
}

bool Value::operator<(const Value& o) const {
    if(validId() && o.validId())
        return id() < o.id();
    int cmp;
    if(category() < o.category()) {
        return true;
    } else if(category() > o.category()) {
        return false;
    } else {
        switch(category()) {
        case CAT_BLANK:
        case CAT_URI:
        case CAT_SIMPLE_LITERAL:
        case CAT_TYPED_STRING:
            return lexical() < o.lexical();
        case CAT_BOOLEAN:
            if(boolean() == o.boolean())
                return lexical() < o.lexical();
            else
                return !boolean() && o.boolean();
        case CAT_NUMERIC:
            cmp = compare(o);
            if(cmp == -1) {
                return true;
            } else if(cmp == 1) {
                return false;
            } else if(validId(datatypeId()) && validId(o.datatypeId())) {
                if(datatypeId() == o.datatypeId())
                    return lexical() < o.lexical();
                else
                    return datatypeId() < o.datatypeId();
            } else {
                assert(!datatypeLex().null() && !o.datatypeLex().null());
                cmp = datatypeLex().compare(o.datatypeLex());
                if(cmp == 0)
                    return lexical() < o.lexical();
                else
                    return cmp < 0;
            }
        case CAT_DATETIME:
            cmp = compare(o);
            if(cmp == -1)
                return true;
            else if(cmp == 1)
                return false;
            else
                return lexical() < o.lexical();
        case CAT_PLAIN_LANG:
            cmp = language() < o.language();
            if(cmp == 0)
                return lexical() < o.lexical();
            else
                return cmp < 0;
        case CAT_OTHER:
            if(validId(datatypeId()) && validId(o.datatypeId())) {
                if(datatypeId() == o.datatypeId())
                    return lexical() < o.lexical();
                else
                    return datatypeId() < o.datatypeId();
            } else {
                assert(!datatypeLex().null() && !o.datatypeLex().null());
                cmp = datatypeLex().compare(o.datatypeLex());
                if(cmp == 0)
                    return lexical() < o.lexical();
                else
                    return cmp < 0;
            }
        default:
            assert(false); // should not happen
        }
    }
    assert(false); // should not happen
    return false;
}

bool Value::operator ==(const Value &o) const {
    if(validId() && o.validId())
        return id() == o.id();
    if(category() != o.category())
        return false;
    if(isNumeric() && numCategory() != o.numCategory())
        return false;
    if(isOtherLiteral() || isInteger() || isFloating()) {
        if(validId(datatypeId()) && validId(o.datatypeId())) {
            if(datatypeId() != o.datatypeId())
                return false;
        } else {
            assert(!datatypeLex().null() && !o.datatypeLex().null());
            if(datatypeLex() != o.datatypeLex())
                return false;
        }
    }
    if(isPlainWithLang() && language() != o.language())
        return false;
    assert(!lexical().null() && !o.lexical().null());
    return lexical() == o.lexical();
}



////////////////////////////////////////////////////////////////////////////////
// Utility functions

Value& Value::ensureLexical() {
    if(!lexical_.null())
        return *this;

    assert(interpreted());
    if(isBoolean()) {
        if(boolean())
            lexical_ = String("true", sizeof("true") - 1);
        else
            lexical_ = String("false", sizeof("false") - 1);
    } else if(isInteger()) {
        lexical_ = String::sprintf("%ld", integer());
    } else if(isFloating()) {
        lexical_ = String::sprintf("%f", floating());
    } else if(isDecimal()) {
        std::string str = decimal().getString();
        lexical_ = String(str.c_str(), str.size(), true);
    } else if(isDateTime()) {
        std::string str = datetime().getString();
        lexical_ = String(str.c_str(), str.size(), true);
    } else {
        // FIXME: shouldn't this be an error?
        lexical_ = String("");
    }
    return *this;
}

Value& Value::ensureInterpreted(const StringMapper& mapper) {
    if(interpreted())
        return *this;
    if(!lexical_.direct())
        lexical_ = mapper.lookupString(lexical_.id());
    interpreted_ = INTERPRETED_UNOWNED;
    if(isBoolean()) {
        boolean_ = lexical_.equals("1", sizeof("1")-1) ||
                   lexical_.equals("true", sizeof("true")-1);
    } else if(isInteger()) {
        integer_ = atoi(lexical_.str());
        if(numapprox_.empty())
            numapprox_ = NumRange(integer_);
    } else if(isFloating()) {
        floating_ = atof(lexical_.str());
        if(numapprox_.empty())
            numapprox_ = NumRange(floating_);
    } else if(isDecimal()) {
        decimal_ = new XSDDecimal(lexical_.str());
        interpreted_ = INTERPRETED_OWNED;
        if(numapprox_.empty())
            numapprox_ = NumRange(*decimal_);
    } else if(isDateTime()) {
        datetime_ = new XSDDateTime(lexical_.str());
        interpreted_ = INTERPRETED_OWNED;
    }
    return *this;
}

Value& Value::ensureDirectStrings(const StringMapper &mapper) {
    if(!lexical_.direct())
        lexical_ = mapper.lookupString(lexical_.id());
    if(!tag_.direct())
        tag_ = mapper.lookupString(tag_.id());
    return *this;
}

Value& Value::ensureResolvedStrings(const Store &store) {
    store.resolve(lexical_);
    store.resolve(tag_);
    return *this;
}

Hash::hash_t Value::hash() const {
    Hash::hash_t hash = category() << 16;
    if(isNumeric()) {
        hash |= numCategory();
    } else if(isPlainWithLang()) {
        assert(!language().null() && language().direct());
        hash = Hash::hash(language().str(), language().length(), hash);
    } else if(isTyped()) {
        assert(!datatypeLex().null() && datatypeLex().direct());
        hash = Hash::hash(datatypeLex().str(), datatypeLex().length(), hash);
    }
    assert(!lexical().null() && lexical().direct());
    return Hash::hash(lexical().str(), lexical().length(), hash);
}

std::ostream& operator<<(std::ostream& out, const Value& val) {
    // FIXME: escape quotes inside lexical
    switch(val.category()) {
    case Value::CAT_BLANK:
        out << "_:" << val.lexical();
        break;
    case Value::CAT_URI:
        out << '<' << val.lexical() << '>';
        break;
    case Value::CAT_SIMPLE_LITERAL:
        out << '"' << val.lexical() << '"';
        break;
    case Value::CAT_PLAIN_LANG:
        out << '"' << val.lexical() << "\"@" << val.language();
        break;
    default:
        out << '"';
        if(!val.lexical().null()) {
            out << val.lexical();
        } else {
            assert(val.interpreted());
            if(val.isBoolean())
                out << (val.boolean() ? "true" : "false");
            else if(val.isInteger())
                out << val.integer();
            else if(val.isFloating())
                out << val.floating();
            else if(val.isDecimal())
                out << val.decimal().getString();
            else if(val.isDateTime())
                out << val.datetime().getString();
        }
        assert(!val.datatypeLex().null());
        out << "\"^^<" << val.datatypeLex() << '>';
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, const Value* val) {
    if(val)
        return out << *val;
    else
        return out;
}

void Value::promoteNumericType(Value& v1, Value& v2) {
    if(v1.isDecimal() && v2.isInteger())
        // convert v2 to xsd:decimal
        v2.fillDecimal(new XSDDecimal(v2.integer()));
    else if(v2.isDecimal() && v1.isInteger())
        // convert v1 to xsd:decimal
        v1.fillDecimal(new XSDDecimal(v1.integer()));
    else if(v1.isFloating() && v2.isInteger())
        // convert v2 to xsd:double
        v2.fillFloating(v2.integer());
    else if(v1.isFloating() && v2.isDecimal())
        // convert v2 to xsd:double
        v2.fillFloating(v2.decimal().getFloat());
    else if(v2.isFloating() && v1.isInteger())
        // convert v1 to xsd:double
        v1.fillFloating(v1.integer());
    else if(v2.isFloating() && v1.isDecimal())
        v1.fillFloating(v1.decimal().getFloat());
}

}
