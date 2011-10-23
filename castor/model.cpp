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

#include <cstdio>
#include <cassert>
#include <sstream>
#include <algorithm>
#include "model.h"

namespace castor {

////////////////////////////////////////////////////////////////////////////////
// Static definitions

char *Value::TYPE_URIS[] = {
    NULL,
    NULL,
    NULL,
    const_cast<char*>("http://www.w3.org/2001/XMLSchema#string"),
    const_cast<char*>("http://www.w3.org/2001/XMLSchema#boolean"),
    const_cast<char*>("http://www.w3.org/2001/XMLSchema#integer"),
    const_cast<char*>("http://www.w3.org/2001/XMLSchema#positiveInteger"),
    const_cast<char*>("http://www.w3.org/2001/XMLSchema#nonPositiveInteger"),
    const_cast<char*>("http://www.w3.org/2001/XMLSchema#negativeInteger"),
    const_cast<char*>("http://www.w3.org/2001/XMLSchema#nonNegativeInteger"),
    const_cast<char*>("http://www.w3.org/2001/XMLSchema#byte"),
    const_cast<char*>("http://www.w3.org/2001/XMLSchema#short"),
    const_cast<char*>("http://www.w3.org/2001/XMLSchema#int"),
    const_cast<char*>("http://www.w3.org/2001/XMLSchema#long"),
    const_cast<char*>("http://www.w3.org/2001/XMLSchema#unsignedByte"),
    const_cast<char*>("http://www.w3.org/2001/XMLSchema#unsignedShort"),
    const_cast<char*>("http://www.w3.org/2001/XMLSchema#unsignedInt"),
    const_cast<char*>("http://www.w3.org/2001/XMLSchema#unsignedLong"),
    const_cast<char*>("http://www.w3.org/2001/XMLSchema#float"),
    const_cast<char*>("http://www.w3.org/2001/XMLSchema#double"),
    const_cast<char*>("http://www.w3.org/2001/XMLSchema#decimal"),
    const_cast<char*>("http://www.w3.org/2001/XMLSchema#dateTime")
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

#define XSD_PREFIX "http://www.w3.org/2001/XMLSchema#"
#define XSD_PREFIX_LEN static_cast<unsigned>(sizeof(XSD_PREFIX) - 1)



////////////////////////////////////////////////////////////////////////////
// Constructors

/**
 * @param uri a URI from raptor
 * @param[out] str a new string containing a copy of the URI (non-zero terminated)
 * @param[out] len the length of the URI
 */
static void convertURI(raptor_uri *uri, char* &str, unsigned &len) {
    char *s = reinterpret_cast<char*>(raptor_uri_as_string(uri));
    len = strlen(s);
    str = new char[len];
    memcpy(str, s, len);
}

Value::Value(const raptor_term *term) {
    id = 0;
    cleanup = CLEAN_NOTHING;
    switch(term->type) {
    case RAPTOR_TERM_TYPE_BLANK:
        type = TYPE_BLANK;
        typeUri = NULL;
        typeUriLen = 0;
        lexicalLen = term->value.blank.string_len;
        lexical = new char[lexicalLen + 1];
        strcpy(lexical, reinterpret_cast<char*>(term->value.blank.string));
        addCleanFlag(CLEAN_LEXICAL);
        break;
    case RAPTOR_TERM_TYPE_URI:
        type = TYPE_IRI;
        typeUri = NULL;
        typeUriLen = 0;
        convertURI(term->value.uri, lexical, lexicalLen);
        addCleanFlag(CLEAN_LEXICAL);
        break;
    case RAPTOR_TERM_TYPE_LITERAL:
        lexicalLen = term->value.literal.string_len;
        lexical = new char[lexicalLen + 1];
        strcpy(lexical, reinterpret_cast<char*>(term->value.literal.string));
        addCleanFlag(CLEAN_LEXICAL);
        if(term->value.literal.datatype == NULL) {
            type = TYPE_PLAIN_STRING;
            typeUri = NULL;
            if(term->value.literal.language == NULL ||
               term->value.literal.language_len == 0) {
                language = NULL;
                languageLen = 0;
            } else {
                languageLen = term->value.literal.language_len;
                language = new char[languageLen + 1];
                strcpy(language, reinterpret_cast<char*>(term->value.literal.language));
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

Value::Value(const rasqal_literal *literal) {
    id = 0;
    cleanup = CLEAN_NOTHING;
    if(literal->type == RASQAL_LITERAL_URI) {
        convertURI(literal->value.uri, lexical, lexicalLen);
    } else {
        lexicalLen = literal->string_len;
        lexical = new char[lexicalLen + 1];
        strcpy(lexical, reinterpret_cast<const char*>(literal->string));
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
        if(literal->language != NULL && literal->language[0] != '\0') {
            languageLen = strlen(literal->language);
            language = new char[languageLen + 1];
            strcpy(language, literal->language);
            addCleanFlag(CLEAN_DATA);
        } else {
            language = NULL;
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
        typeUri = NULL;
        typeUriLen = 0;
    }
    if(hasCleanFlag(CLEAN_LEXICAL)) {
        delete [] lexical;
        lexical = NULL;
        lexicalLen = 0;
    }
    if(hasCleanFlag(CLEAN_DATA)) {
        switch(type) {
        case TYPE_PLAIN_STRING:
            delete [] language;
            language = NULL;
            languageLen = 0;
            break;
        case TYPE_DECIMAL:
            delete decimal;
            decimal = NULL;
            break;
        case TYPE_DATETIME:
            // TODO  delete datetime;
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

void Value::fillCopy(const Value &value, bool deep)  {
    clean();
    memcpy(this, &value, sizeof(Value));
    cleanup = CLEAN_NOTHING;
    if(deep) {
        if(lexical) {
            lexical = new char[lexicalLen + 1];
            memcpy(lexical, value.lexical, lexicalLen);
            lexical[lexicalLen] = '\0';
            addCleanFlag(CLEAN_LEXICAL);
        }
        if(type == TYPE_CUSTOM) {
            typeUri = new char[typeUriLen + 1];
            memcpy(typeUri, value.typeUri, typeUriLen);
            typeUri[typeUriLen] = '\0';
            addCleanFlag(CLEAN_TYPE_URI);
        }
        if(type == TYPE_PLAIN_STRING && language) {
            language = new char[languageLen + 1];
            memcpy(language, value.language, languageLen);
            language[languageLen] = '\0';
            addCleanFlag(CLEAN_DATA);
        }
        if(type == TYPE_DECIMAL && isInterpreted) {
            decimal = new XSDDecimal(*value.decimal);
            addCleanFlag(CLEAN_DATA);
        }
    }
}

void Value::fillBoolean(bool value) {
    clean();
    id = 0;
    type = TYPE_BOOLEAN;
    typeUri = TYPE_URIS[TYPE_BOOLEAN];
    typeUriLen = TYPE_URIS_LEN[TYPE_BOOLEAN];
    lexical = NULL;
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
    lexical = NULL;
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
    lexical = NULL;
    lexicalLen = 0;
    isInterpreted = true;
    floating = value;
}

void Value::fillDecimal(XSDDecimal *value) {
    clean();
    id = 0;
    type = TYPE_DECIMAL;
    typeUri = TYPE_URIS[TYPE_DECIMAL];
    typeUriLen = TYPE_URIS_LEN[TYPE_DECIMAL];
    lexical = NULL;
    lexicalLen = 0;
    isInterpreted = true;
    decimal = value;
    cleanup = CLEAN_DATA;
}

void Value::fillSimpleLiteral(char *lexical, unsigned len, bool freeLexical) {
    clean();
    id = 0;
    type = TYPE_PLAIN_STRING;
    typeUri = NULL;
    typeUriLen = 0;
    this->lexical = lexical;
    lexicalLen = len;
    if(freeLexical)
        cleanup = CLEAN_LEXICAL;
    isInterpreted = true;
}

void Value::fillIRI(char *lexical, unsigned len, bool freeLexical) {
    clean();
    id = 0;
    type = TYPE_IRI;
    typeUri = NULL;
    typeUriLen = 0;
    this->lexical = lexical;
    lexicalLen = len;
    if(freeLexical)
        cleanup = CLEAN_LEXICAL;
    isInterpreted = true;
}

void Value::fillBlank(char *lexical, unsigned len, bool freeLexical) {
    clean();
    id = 0;
    type = TYPE_BLANK;
    typeUri = NULL;
    typeUriLen = 0;
    this->lexical = lexical;
    lexicalLen = len;
    if(freeLexical)
        cleanup = CLEAN_LEXICAL;
    isInterpreted = true;
}



////////////////////////////////////////////////////////////////////////////////
// Comparisons

int Value::compare(const Value &o) const {
    if(isNumeric() && o.isNumeric()) {
        if(isInteger() && o.isInteger()) {
            int diff = integer - o.integer;
            if(diff < 0) return -1;
            else if(diff > 0) return 1;
            else return 0;
        } else if(isDecimal() && o.isDecimal()) {
            // FIXME compare decimal with integer??
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
    } else {
        return -2; // TODO datetime
    }
}

bool Value::operator<(const Value &o) const {
    if(id > 0 && o.id > 0)
        return id < o.id;
    Class cls = getClass();
    Class ocls = o.getClass();
    int cmp;
    if(cls < ocls) {
        return true;
    } else if(cls > ocls) {
        return false;
    } else {
        switch(cls) {
        case CLASS_BLANK:
        case CLASS_IRI:
        case CLASS_SIMPLE_LITERAL:
        case CLASS_TYPED_STRING:
            return cmpstr(lexical, lexicalLen, o.lexical, o.lexicalLen) < 0;
        case CLASS_BOOLEAN:
            if(boolean == o.boolean)
                return cmpstr(lexical, lexicalLen, o.lexical, o.lexicalLen) < 0;
            else
                return !boolean && o.boolean;
        case CLASS_NUMERIC:
            cmp = compare(o);
            if(cmp == -1)
                return true;
            else if(cmp == 1)
                return false;
            else if(type == o.type)
                return cmpstr(lexical, lexicalLen, o.lexical, o.lexicalLen) < 0;
            else
                return type < o.type;
        case CLASS_DATETIME:
            // TODO
            return cmpstr(lexical, lexicalLen, o.lexical, o.lexicalLen) < 0;
        case CLASS_OTHER:
            if(isPlain() && o.isPlain()) {
                // plain literals with language tags
                cmp = cmpstr(language, languageLen, o.language, o.languageLen);
                if(cmp == 0)
                    return cmpstr(lexical, lexicalLen, o.lexical, o.lexicalLen) < 0;
                else
                    return cmp < 0;
            } else {
                const char *uri1 = typeUri == NULL ? "" : typeUri;
                const char *uri2 = o.typeUri == NULL ? "" : o.typeUri;
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

int Value::rdfequals(const Value &o) const {
    if(id > 0 && id == o.id)
        return 0;
    if(type == TYPE_UNKOWN || o.type == TYPE_UNKOWN) {
        if(typeUri == NULL || o.typeUri == NULL)
            return 1; // FIXME not sure
        if(typeUriLen != o.typeUriLen ||
                cmpstr(typeUri, typeUriLen, o.typeUri, o.typeUriLen) != 0)
            return -1;
    } else if(type != o.type) {
        if(type >= TYPE_PLAIN_STRING || o.type >= TYPE_PLAIN_STRING)
            return -1;
        else
            return 1;
    }
    if(isPlain()) {
        // FIXME
        if(language != NULL && o.language != NULL) {
            if(languageLen != o.languageLen ||
                    cmpstr(language, languageLen, o.language, o.languageLen) != 0)
                return 1;
        } else if(language != NULL || o.language != NULL) {
            return 1;
        }
    }
    if(lexicalLen != o.lexicalLen ||
            cmpstr(lexical, lexicalLen, o.lexical, o.lexicalLen) != 0)
        return typeUri == NULL ? 1 : -1;

    return 0;
}



////////////////////////////////////////////////////////////////////////////////
// Utility functions

void Value::ensureLexical() {
    if(lexical)
        return;

    if(isBoolean()) {
        if(boolean) {
            lexical = const_cast<char*>("true");
            lexicalLen = 4;
        } else {
            lexical = const_cast<char*>("false");
            lexicalLen = 5;
        }
    } else if(isInteger()) {
        lexicalLen = snprintf(NULL, 0, "%ld", integer);
        lexical = new char[lexicalLen + 1];
        sprintf(lexical, "%ld", integer);
        addCleanFlag(CLEAN_LEXICAL);
    } else if(isFloating()) {
        lexicalLen = snprintf(NULL, 0, "%f", floating);
        lexical = new char[lexicalLen + 1];
        sprintf(lexical, "%f", floating);
        addCleanFlag(CLEAN_LEXICAL);
    } else if(isDecimal()) {
        std::string str = decimal->getString();
        lexicalLen = str.size();
        lexical = new char[lexicalLen + 1];
        memcpy(lexical, str.c_str(), lexicalLen);
        lexical[lexicalLen] = '\0';
        addCleanFlag(CLEAN_LEXICAL);
    } else if(isDateTime()) {
        // TODO
        lexical = const_cast<char*>("");
    } else {
        lexical = const_cast<char*>("");
    }
}

void Value::ensureInterpreted() {
    if(isInterpreted)
        return;
    if(isBoolean()) {
        boolean = (lexicalLen == 1 && memcmp(lexical, "1", 1) == 0) ||
                  (lexicalLen == 4 && memcmp(lexical, "true", 4) == 0);
    } else if(isInteger()) {
        integer = atoi(lexical); // FIXME non-null terminated
    } else if(isFloating()) {
        floating = atof(lexical); // FIXME non-null terminated
    } else if(isDecimal()) {
        decimal = new XSDDecimal(lexical); // FIXME non-null terminated
        addCleanFlag(CLEAN_DATA);
    } else if(isDateTime()) {
        // TODO
    }
    isInterpreted = true;
}

uint32_t Value::hash() const {
    uint32_t hash = type;
    if(type == TYPE_CUSTOM)
        hash = Hash::hash(typeUri, typeUriLen, hash);
    return Hash::hash(lexical, lexicalLen, hash);
}

std::ostream& operator<<(std::ostream &out, const Value &val) {
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
        if(val.language != NULL) {
            out << '@';
            out.write(val.language, val.languageLen);
        }
        break;
    default:
        out << '"';
        if(val.lexical) {
            // FIXME escape quotes inside lexical
            out.write(val.lexical, val.lexicalLen);
        } else {
            if(val.isBoolean())
                out << (val.boolean ? "true" : "false");
            else if(val.isInteger())
                out << val.integer;
            else if(val.isFloating())
                out << val.floating;
            else if(val.isDecimal()) {
                out << val.decimal->getString();
            }
            // TODO datetime
        }
        out << "\"^^<";
        out.write(val.typeUri, val.typeUriLen);
        out << '>';
    }
    return out;
}

std::ostream& operator<<(std::ostream &out, const Value *val) {
    if(val)
        return out << *val;
    else
        return out;
}

void Value::promoteNumericType(Value &v1, Value &v2) {
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

    if(typeUriLen < XSD_PREFIX_LEN + 1)
        return;

    if(memcmp(typeUri, XSD_PREFIX, XSD_PREFIX_LEN) != 0)
        return;

    char *fragment = &typeUri[XSD_PREFIX_LEN];
    unsigned fragmentLen = typeUriLen - XSD_PREFIX_LEN;
    for(Type t = TYPE_FIRST_XSD; t <= TYPE_LAST_XSD;
        t = static_cast<Type>(t+1)) {
        if(memcmp(fragment, &TYPE_URIS[t][XSD_PREFIX_LEN],
                  std::min(fragmentLen, TYPE_URIS_LEN[t] - XSD_PREFIX_LEN)) == 0) {
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
