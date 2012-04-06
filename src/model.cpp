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
#include "model.h"

#include <cstdio>
#include <cassert>
#include <sstream>
#include <algorithm>

namespace castor {

////////////////////////////////////////////////////////////////////////////////
// Static definitions

const char* Value::TYPE_URIS[] = {
    nullptr,
    nullptr,
    nullptr,
    "http://www.w3.org/2001/XMLSchema#string",
    "http://www.w3.org/2001/XMLSchema#boolean",
    "http://www.w3.org/2001/XMLSchema#integer",
    "http://www.w3.org/2001/XMLSchema#positiveInteger",
    "http://www.w3.org/2001/XMLSchema#nonPositiveInteger",
    "http://www.w3.org/2001/XMLSchema#negativeInteger",
    "http://www.w3.org/2001/XMLSchema#nonNegativeInteger",
    "http://www.w3.org/2001/XMLSchema#byte",
    "http://www.w3.org/2001/XMLSchema#short",
    "http://www.w3.org/2001/XMLSchema#int",
    "http://www.w3.org/2001/XMLSchema#long",
    "http://www.w3.org/2001/XMLSchema#unsignedByte",
    "http://www.w3.org/2001/XMLSchema#unsignedShort",
    "http://www.w3.org/2001/XMLSchema#unsignedInt",
    "http://www.w3.org/2001/XMLSchema#unsignedLong",
    "http://www.w3.org/2001/XMLSchema#float",
    "http://www.w3.org/2001/XMLSchema#double",
    "http://www.w3.org/2001/XMLSchema#decimal",
    "http://www.w3.org/2001/XMLSchema#dateTime"
};

unsigned Value::TYPE_URIS_LEN[] = {
    0,
    0,
    0,
    sizeof("http://www.w3.org/2001/XMLSchema#string") - 1,
    sizeof("http://www.w3.org/2001/XMLSchema#boolean") - 1,
    sizeof("http://www.w3.org/2001/XMLSchema#integer") - 1,
    sizeof("http://www.w3.org/2001/XMLSchema#positiveInteger") - 1,
    sizeof("http://www.w3.org/2001/XMLSchema#nonPositiveInteger") - 1,
    sizeof("http://www.w3.org/2001/XMLSchema#negativeInteger") - 1,
    sizeof("http://www.w3.org/2001/XMLSchema#nonNegativeInteger") - 1,
    sizeof("http://www.w3.org/2001/XMLSchema#byte") - 1,
    sizeof("http://www.w3.org/2001/XMLSchema#short") - 1,
    sizeof("http://www.w3.org/2001/XMLSchema#int") - 1,
    sizeof("http://www.w3.org/2001/XMLSchema#long") - 1,
    sizeof("http://www.w3.org/2001/XMLSchema#unsignedByte") - 1,
    sizeof("http://www.w3.org/2001/XMLSchema#unsignedShort") - 1,
    sizeof("http://www.w3.org/2001/XMLSchema#unsignedInt") - 1,
    sizeof("http://www.w3.org/2001/XMLSchema#unsignedLong") - 1,
    sizeof("http://www.w3.org/2001/XMLSchema#float") - 1,
    sizeof("http://www.w3.org/2001/XMLSchema#double") - 1,
    sizeof("http://www.w3.org/2001/XMLSchema#decimal") - 1,
    sizeof("http://www.w3.org/2001/XMLSchema#dateTime") - 1
};

static constexpr char XSD_PREFIX[] = "http://www.w3.org/2001/XMLSchema#";



////////////////////////////////////////////////////////////////////////////
// Constructors

/**
 * @param uri a URI from raptor
 * @param[out] str a new string containing a copy of the URI
 * @param[out] len the length of the URI
 */
static void convertURI(raptor_uri* uri, const char*& str, unsigned& len) {
    char* uristr = reinterpret_cast<char*>(raptor_uri_as_string(uri));
    len = strlen(uristr);
    char* s = new char[len + 1];
    memcpy(s, uristr, len + 1);
    str = s;
}

Value::Value(const raptor_term* term) {
    char* s;
    id = 0;
    cleanup = CLEAN_NOTHING;
    switch(term->type) {
    case RAPTOR_TERM_TYPE_BLANK:
        type = TYPE_BLANK;
        typeUri = nullptr;
        typeUriLen = 0;
        lexicalLen = term->value.blank.string_len;
        s = new char[lexicalLen + 1];
        memcpy(s, reinterpret_cast<char*>(term->value.blank.string),
               lexicalLen + 1);
        lexical = s;
        addCleanFlag(CLEAN_LEXICAL);
        break;
    case RAPTOR_TERM_TYPE_URI:
        type = TYPE_IRI;
        typeUri = nullptr;
        typeUriLen = 0;
        convertURI(term->value.uri, lexical, lexicalLen);
        addCleanFlag(CLEAN_LEXICAL);
        break;
    case RAPTOR_TERM_TYPE_LITERAL:
        lexicalLen = term->value.literal.string_len;
        s = new char[lexicalLen + 1];
        memcpy(s, reinterpret_cast<char*>(term->value.literal.string),
               lexicalLen + 1);
        lexical = s;
        addCleanFlag(CLEAN_LEXICAL);
        if(term->value.literal.datatype == nullptr) {
            type = TYPE_PLAIN_STRING;
            typeUri = nullptr;
            if(term->value.literal.language == nullptr ||
               term->value.literal.language_len == 0) {
                language = nullptr;
                languageLen = 0;
            } else {
                languageLen = term->value.literal.language_len;
                s = new char[languageLen + 1];
                memcpy(s, reinterpret_cast<char*>(term->value.literal.language),
                       languageLen + 1);
                language = s;
                addCleanFlag(CLEAN_DATA);
            }
        } else {
            type = TYPE_CUSTOM;
            convertURI(term->value.literal.datatype, typeUri, typeUriLen);
            addCleanFlag(CLEAN_TYPE_URI);
            interpretDatatype();
        }
        break;
    default:
        assert(false);
    }
}

Value::Value(const rasqal_literal* literal) {
    char* s;
    id = 0;
    cleanup = CLEAN_NOTHING;
    if(literal->type == RASQAL_LITERAL_URI) {
        convertURI(literal->value.uri, lexical, lexicalLen);
    } else {
        lexicalLen = literal->string_len;
        s = new char[lexicalLen + 1];
        memcpy(s, reinterpret_cast<const char*>(literal->string),
               lexicalLen + 1);
        lexical = s;
    }
    addCleanFlag(CLEAN_LEXICAL);
    switch(literal->type) {
    case RASQAL_LITERAL_BLANK:
        type = TYPE_BLANK;
        break;
    case RASQAL_LITERAL_URI:
        type = TYPE_IRI;
        break;
    case RASQAL_LITERAL_STRING:
        type = TYPE_PLAIN_STRING;
        if(literal->language != nullptr && literal->language[0] != '\0') {
            languageLen = strlen(literal->language);
            s = new char[languageLen + 1];
            memcpy(s, literal->language, languageLen + 1);
            language = s;
            addCleanFlag(CLEAN_DATA);
        } else {
            language = nullptr;
        }
        break;
    case RASQAL_LITERAL_XSD_STRING:
        type = TYPE_TYPED_STRING;
        break;
    case RASQAL_LITERAL_BOOLEAN:
        type = TYPE_BOOLEAN;
        break;
    case RASQAL_LITERAL_FLOAT:
        type = TYPE_FLOAT;
        break;
    case RASQAL_LITERAL_DOUBLE:
        type = TYPE_DOUBLE;
        break;
    case RASQAL_LITERAL_DECIMAL:
        type = TYPE_DECIMAL;
        break;
    case RASQAL_LITERAL_DATETIME:
        type = TYPE_DATETIME;
        break;
    case RASQAL_LITERAL_INTEGER: // what kind of integer precisely?
    case RASQAL_LITERAL_UDT:
        type = TYPE_CUSTOM;
        convertURI(literal->datatype, typeUri, typeUriLen);
        addCleanFlag(CLEAN_TYPE_URI);
        interpretDatatype();
        break;
    default:
        assert(false);
    }
}



////////////////////////////////////////////////////////////////////////////////
// Cleanup

void Value::clean() {
    if(hasCleanFlag(CLEAN_TYPE_URI)) {
        delete [] typeUri;
        typeUri = nullptr;
        typeUriLen = 0;
    }
    if(hasCleanFlag(CLEAN_LEXICAL)) {
        delete [] lexical;
        lexical = nullptr;
        lexicalLen = 0;
    }
    if(hasCleanFlag(CLEAN_DATA)) {
        switch(type) {
        case TYPE_PLAIN_STRING:
            delete [] language;
            language = nullptr;
            languageLen = 0;
            break;
        case TYPE_DECIMAL:
            delete decimal;
            decimal = nullptr;
            break;
        case TYPE_DATETIME:
            delete datetime;
            datetime = nullptr;
            break;
        default:
            // do nothing
            break;
        }
    }
    cleanup = CLEAN_NOTHING;
}



////////////////////////////////////////////////////////////////////////////////
// Fill methods

void Value::fillCopy(const Value& value, bool deep)  {
    clean();
    memcpy(this, &value, sizeof(Value));
    cleanup = CLEAN_NOTHING;
    if(deep) {
        if(lexical && value.hasCleanFlag(CLEAN_LEXICAL)) {
            char* s = new char[lexicalLen + 1];
            memcpy(s, value.lexical, lexicalLen + 1);
            s[lexicalLen] = '\0';
            lexical = s;
            addCleanFlag(CLEAN_LEXICAL);
        }
        if(type == TYPE_CUSTOM && value.hasCleanFlag(CLEAN_TYPE_URI)) {
            char* s = new char[typeUriLen + 1];
            memcpy(s, value.typeUri, typeUriLen + 1);
            s[typeUriLen] = '\0';
            typeUri = s;
            addCleanFlag(CLEAN_TYPE_URI);
        }
        if(type == TYPE_PLAIN_STRING && language && value.hasCleanFlag(CLEAN_DATA)) {
            char* s = new char[languageLen + 1];
            memcpy(s, value.language, languageLen + 1);
            s[languageLen] = '\0';
            language = s;
            addCleanFlag(CLEAN_DATA);
        }
        if(type == TYPE_DECIMAL && isInterpreted && value.hasCleanFlag(CLEAN_DATA)) {
            decimal = new XSDDecimal(*value.decimal);
            addCleanFlag(CLEAN_DATA);
        }
    }
}

void Value::fillMove(Value& value)  {
    clean();
    memcpy(this, &value, sizeof(Value));
    value.cleanup = CLEAN_NOTHING;
}

void Value::fillBoolean(bool value) {
    clean();
    id = 0;
    type = TYPE_BOOLEAN;
    typeUri = TYPE_URIS[TYPE_BOOLEAN];
    typeUriLen = TYPE_URIS_LEN[TYPE_BOOLEAN];
    lexical = nullptr;
    lexicalLen = 0;
    isInterpreted = true;
    boolean = value;
}

void Value::fillInteger(long value) {
    clean();
    id = 0;
    type = TYPE_INTEGER;
    typeUri = TYPE_URIS[TYPE_INTEGER];
    typeUriLen = TYPE_URIS_LEN[TYPE_INTEGER];
    lexical = nullptr;
    lexicalLen = 0;
    isInterpreted = true;
    integer = value;
}

void Value::fillFloating(double value) {
    clean();
    id = 0;
    type = TYPE_DOUBLE;
    typeUri = TYPE_URIS[TYPE_DOUBLE];
    typeUriLen = TYPE_URIS_LEN[TYPE_DOUBLE];
    lexical = nullptr;
    lexicalLen = 0;
    isInterpreted = true;
    floating = value;
}

void Value::fillDecimal(XSDDecimal* value) {
    clean();
    id = 0;
    type = TYPE_DECIMAL;
    typeUri = TYPE_URIS[TYPE_DECIMAL];
    typeUriLen = TYPE_URIS_LEN[TYPE_DECIMAL];
    lexical = nullptr;
    lexicalLen = 0;
    isInterpreted = true;
    decimal = value;
    cleanup = CLEAN_DATA;
}

void Value::fillSimpleLiteral(const char* lexical, unsigned len, bool freeLexical) {
    clean();
    id = 0;
    type = TYPE_PLAIN_STRING;
    typeUri = nullptr;
    typeUriLen = 0;
    this->lexical = lexical;
    lexicalLen = len;
    if(freeLexical)
        cleanup = CLEAN_LEXICAL;
    isInterpreted = true;
}

void Value::fillIRI(const char* lexical, unsigned len, bool freeLexical) {
    clean();
    id = 0;
    type = TYPE_IRI;
    typeUri = nullptr;
    typeUriLen = 0;
    this->lexical = lexical;
    lexicalLen = len;
    if(freeLexical)
        cleanup = CLEAN_LEXICAL;
    isInterpreted = true;
}

void Value::fillBlank(const char* lexical, unsigned len, bool freeLexical) {
    clean();
    id = 0;
    type = TYPE_BLANK;
    typeUri = nullptr;
    typeUriLen = 0;
    this->lexical = lexical;
    lexicalLen = len;
    if(freeLexical)
        cleanup = CLEAN_LEXICAL;
    isInterpreted = true;
}



////////////////////////////////////////////////////////////////////////////////
// Comparisons

int Value::compare(const Value& o) const {
    if(isNumeric() && o.isNumeric()) {
        if(isInteger() && o.isInteger()) {
            int diff = integer - o.integer;
            if(diff < 0) return -1;
            else if(diff > 0) return 1;
            else return 0;
        } else if(isDecimal() && o.isDecimal()) {
            // FIXME: compare decimal with integer??
            return decimal->compare(*o.decimal);
        } else {
            double d1 = isFloating() ? floating :
                        (isDecimal() ? decimal->getFloat() : integer);
            double d2 = o.isFloating() ? o.floating :
                        (o.isDecimal() ? o.decimal->getFloat() : o.integer);
            double diff = d1 - d2;
            if(diff < .0) return -1;
            else if(diff > .0) return 1;
            else return 0;
        }
    } else if((isSimple() && o.isSimple()) ||
              (isXSDString() && o.isXSDString())) {
        int i = cmpstr(lexical, lexicalLen, o.lexical, o.lexicalLen);
        if(i < 0) return -1;
        else if(i > 0) return 1;
        else return 0;
    } else if(isBoolean() && o.isBoolean()) {
        return (boolean ? 1 : 0) - (o.boolean ? 1 : 0);
    } else if(isDateTime() && o.isDateTime()) {
        return datetime->compare(*o.datetime);
    } else {
        return -2;
    }
}

bool Value::operator<(const Value& o) const {
    if(id > 0 && o.id > 0)
        return id < o.id;
    Category cat = category();
    Category ocat = o.category();
    int cmp;
    if(cat < ocat) {
        return true;
    } else if(cat > ocat) {
        return false;
    } else {
        switch(cat) {
        case CAT_BLANK:
        case CAT_IRI:
        case CAT_SIMPLE_LITERAL:
        case CAT_TYPED_STRING:
            return cmpstr(lexical, lexicalLen, o.lexical, o.lexicalLen) < 0;
        case CAT_BOOLEAN:
            if(boolean == o.boolean)
                return cmpstr(lexical, lexicalLen, o.lexical, o.lexicalLen) < 0;
            else
                return !boolean && o.boolean;
        case CAT_NUMERIC:
        case CAT_DATETIME:
            cmp = compare(o);
            if(cmp == -1)
                return true;
            else if(cmp == 1)
                return false;
            else if(type == o.type)
                return cmpstr(lexical, lexicalLen, o.lexical, o.lexicalLen) < 0;
            else
                return type < o.type;
        case CAT_OTHER:
            if(isPlain() && o.isPlain()) {
                // plain literals with language tags
                cmp = cmpstr(language, languageLen, o.language, o.languageLen);
                if(cmp == 0)
                    return cmpstr(lexical, lexicalLen, o.lexical, o.lexicalLen) < 0;
                else
                    return cmp < 0;
            } else {
                const char* uri1 = typeUri == nullptr ? "" : typeUri;
                const char* uri2 = o.typeUri == nullptr ? "" : o.typeUri;
                cmp = cmpstr(uri1, typeUriLen, uri2, o.typeUriLen);
                if(cmp == 0)
                    return cmpstr(lexical, lexicalLen, o.lexical, o.lexicalLen) < 0;
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

int Value::rdfequals(const Value& o) const {
    if(id > 0 && id == o.id)
        return 0;
    int falseret = isLiteral() && o.isLiteral() ? -1 : 1;
    if(type == TYPE_CUSTOM || o.type == TYPE_CUSTOM) {
        if(!eqstr(typeUri, typeUriLen, o.typeUri, o.typeUriLen))
            return falseret;
    } else if(type != o.type) {
        return falseret;
    }
    if(isPlain()) {
        if(language != nullptr && o.language != nullptr) {
            if(!eqstr(language, languageLen, o.language, o.languageLen))
                return falseret;
        } else if(language != nullptr || o.language != nullptr) {
            return falseret;
        }
    }
    if(!eqstr(lexical, lexicalLen, o.lexical, o.lexicalLen))
        return falseret;
    return 0;
}



////////////////////////////////////////////////////////////////////////////////
// Utility functions

void Value::ensureLexical() {
    if(lexical)
        return;

    if(isBoolean()) {
        if(boolean) {
            lexical = "true";
            lexicalLen = 4;
        } else {
            lexical = "false";
            lexicalLen = 5;
        }
    } else if(isInteger()) {
        lexicalLen = snprintf(nullptr, 0, "%ld", integer);
        char* s = new char[lexicalLen + 1];
        sprintf(s, "%ld", integer);
        lexical = s;
        addCleanFlag(CLEAN_LEXICAL);
    } else if(isFloating()) {
        lexicalLen = snprintf(nullptr, 0, "%f", floating);
        char* s = new char[lexicalLen + 1];
        sprintf(s, "%f", floating);
        lexical = s;
        addCleanFlag(CLEAN_LEXICAL);
    } else if(isDecimal()) {
        std::string str = decimal->getString();
        lexicalLen = str.size();
        char* s = new char[lexicalLen + 1];
        memcpy(s, str.c_str(), lexicalLen);
        s[lexicalLen] = '\0';
        lexical = s;
        addCleanFlag(CLEAN_LEXICAL);
    } else if(isDateTime()) {
        std::string str = datetime->getString();
        lexicalLen = str.size();
        char* s = new char[lexicalLen + 1];
        memcpy(s, str.c_str(), lexicalLen);
        s[lexicalLen] = '\0';
        lexical = s;
        addCleanFlag(CLEAN_LEXICAL);
    } else {
        lexical = "";
        lexicalLen = 0;
    }
}

void Value::ensureInterpreted() {
    if(isInterpreted)
        return;
    if(isBoolean()) {
        boolean = (lexicalLen == 1 && memcmp(lexical, "1", 1) == 0) ||
                  (lexicalLen == 4 && memcmp(lexical, "true", 4) == 0);
    } else if(isInteger()) {
        integer = atoi(lexical);
    } else if(isFloating()) {
        floating = atof(lexical);
    } else if(isDecimal()) {
        decimal = new XSDDecimal(lexical);
        addCleanFlag(CLEAN_DATA);
    } else if(isDateTime()) {
        datetime = new XSDDateTime(lexical);
        addCleanFlag(CLEAN_DATA);
    }
    isInterpreted = true;
}

Hash::hash_t Value::hash() const {
    Hash::hash_t hash = type;
    if(type == TYPE_CUSTOM)
        hash = Hash::hash(typeUri, typeUriLen, hash);
    return Hash::hash(lexical, lexicalLen, hash);
}

std::ostream& operator<<(std::ostream& out, const Value& val) {
    switch(val.type) {
    case Value::TYPE_BLANK:
        out << "_:";
        if(val.lexical)
            out.write(val.lexical, val.lexicalLen);
        break;
    case Value::TYPE_IRI:
        out << '<';
        out.write(val.lexical, val.lexicalLen);
        out << '>';
        break;
    case Value::TYPE_PLAIN_STRING:
        out << '"';
        out.write(val.lexical, val.lexicalLen);
        out << '"';
        if(val.language != nullptr) {
            out << '@';
            out.write(val.language, val.languageLen);
        }
        break;
    default:
        out << '"';
        if(val.lexical) {
            // FIXME: escape quotes inside lexical
            out.write(val.lexical, val.lexicalLen);
        } else {
            if(val.isBoolean())
                out << (val.boolean ? "true" : "false");
            else if(val.isInteger())
                out << val.integer;
            else if(val.isFloating())
                out << val.floating;
            else if(val.isDecimal())
                out << val.decimal->getString();
            else if(val.isDateTime())
                out << val.datetime->getString();
        }
        out << "\"^^<";
        out.write(val.typeUri, val.typeUriLen);
        out << '>';
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
    v1.ensureInterpreted();
    v2.ensureInterpreted();
    if(v1.isDecimal() && v2.isInteger())
        // convert v2 to xsd:decimal
        v2.fillDecimal(new XSDDecimal(v2.integer));
    else if(v2.isDecimal() && v1.isInteger())
        // convert v1 to xsd:decimal
        v1.fillDecimal(new XSDDecimal(v1.integer));
    else if(v1.isFloating() && v2.isInteger())
        // convert v2 to xsd:double
        v2.fillFloating(v2.integer);
    else if(v1.isFloating() && v2.isDecimal())
        // convert v2 to xsd:double
        v2.fillFloating(v2.decimal->getFloat());
    else if(v2.isFloating() && v1.isInteger())
        // convert v1 to xsd:double
        v1.fillFloating(v1.integer);
    else if(v2.isFloating() && v1.isDecimal())
        v1.fillFloating(v1.decimal->getFloat());
}

void Value::interpretDatatype() {
    if(type != TYPE_CUSTOM)
        return;

    constexpr unsigned PREFIXLEN = sizeof(XSD_PREFIX) - 1;
    if(typeUriLen < PREFIXLEN + 1)
        return;

    if(memcmp(typeUri, XSD_PREFIX, PREFIXLEN) != 0)
        return;

    const char* fragment = &typeUri[PREFIXLEN];
    unsigned fragmentLen = typeUriLen - PREFIXLEN;
    for(Type t = TYPE_FIRST_XSD; t <= TYPE_LAST_XSD;
        t = static_cast<Type>(t+1)) {
        if(typeUriLen == TYPE_URIS_LEN[t] &&
           memcmp(fragment, &TYPE_URIS[t][PREFIXLEN], fragmentLen) == 0) {
            type = t;
            if(hasCleanFlag(CLEAN_TYPE_URI)) {
                delete [] typeUri;
                removeCleanFlag(CLEAN_TYPE_URI);
            }
            typeUri = TYPE_URIS[type];
            typeUriLen = TYPE_URIS_LEN[type];
            return;
        }
    }
}

}
