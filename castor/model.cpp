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
#include "model.h"
#include "store.h"

namespace castor {

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

void Value::clean() {
    if(hasCleanFlag(CLEAN_TYPE_URI)) {
        delete [] typeUri;
        typeUri = NULL;
    }
    if(hasCleanFlag(CLEAN_LEXICAL)) {
        delete [] lexical;
        lexical = NULL;
    }
    if(hasCleanFlag(CLEAN_DATA)) {
        switch(type) {
        case TYPE_PLAIN_STRING:
            delete [] languageTag;
            languageTag = NULL;
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

void Value::fillBoolean(bool value) {
    clean();
    id = 0;
    type = TYPE_BOOLEAN;
    typeUri = TYPE_URIS[TYPE_BOOLEAN];
    lexical = NULL;
    boolean = value;
}

void Value::fillInteger(long value) {
    clean();
    id = 0;
    type = TYPE_INTEGER;
    typeUri = TYPE_URIS[TYPE_INTEGER];
    lexical = NULL;
    integer = value;
}

void Value::fillFloating(double value) {
    clean();
    id = 0;
    type = TYPE_DOUBLE;
    typeUri = TYPE_URIS[TYPE_DOUBLE];
    lexical = NULL;
    floating = value;
}

void Value::fillDecimal(XSDDecimal *value) {
    clean();
    id = 0;
    type = TYPE_DECIMAL;
    typeUri = TYPE_URIS[TYPE_DECIMAL];
    lexical = NULL;
    decimal = value;
    cleanup = CLEAN_DATA;
}

void Value::fillSimpleLiteral(char *lexical, bool freeLexical) {
    clean();
    id = 0;
    type = TYPE_PLAIN_STRING;
    typeUri = NULL;
    this->lexical = lexical;
    if(freeLexical)
        cleanup = CLEAN_LEXICAL;
}

void Value::fillIRI(char *lexical, bool freeLexical) {
    clean();
    id = 0;
    type = TYPE_IRI;
    typeUri = NULL;
    this->lexical = lexical;
    if(freeLexical)
        cleanup = CLEAN_LEXICAL;
}

void Value::fillId(Store *store) {
    if(id > 0)
        return;
    id = store->getValueId(type, typeUri, lexical,
                           isPlain() ? languageTag : NULL);
}

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
    } else if((isPlain() && language == 0 && o.isPlain() && o.language == 0) ||
              (isXSDString() && o.isXSDString())) {
        int i = strcmp(lexical, o.lexical);
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
            return strcmp(lexical, o.lexical) < 0;
        case CLASS_BOOLEAN:
            if(boolean == o.boolean)
                return strcmp(lexical, o.lexical);
            else
                return !boolean && o.boolean;
        case CLASS_NUMERIC:
            cmp = compare(o);
            if(cmp == -1)
                return true;
            else if(cmp == 1)
                return false;
            else if(type == o.type)
                return strcmp(lexical, o.lexical) < 0;
            else
                return type < o.type;
        case CLASS_DATETIME:
            // TODO
            return strcmp(lexical, o.lexical) < 0;
        case CLASS_OTHER:
            if(isPlain() && o.isPlain()) {
                // plain literals with language tags
                cmp = strcmp(languageTag, o.languageTag);
                if(cmp == 0)
                    return strcmp(lexical, o.lexical) < 0;
                else
                    return cmp < 0;
            } else {
                const char *uri1 = typeUri == NULL ? "" : typeUri;
                const char *uri2 = o.typeUri == NULL ? "" : o.typeUri;
                cmp = strcmp(uri1, uri2);
                if(cmp == 0)
                    return strcmp(lexical, o.lexical) < 0;
                else
                    return cmp < 0;
            }
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
        if(strcmp(typeUri, o.typeUri) != 0)
            return -1;
    } else if(type != o.type) {
        if(type >= TYPE_PLAIN_STRING || o.type >= TYPE_PLAIN_STRING)
            return -1;
        else
            return 1;
    }
    if(isPlain()) {
        if(language < 0 || o.language < 0) {
            if(strcmp(languageTag, o.languageTag) != 0)
                return 1;
        } else if(language != o.language) {
            return 1;
        }
    }
    if(strcmp(lexical, o.lexical) != 0)
        return typeUri == NULL ? 1 : -1;

    return 0;
}

void Value::ensureLexical() {
    if(lexical)
        return;

    if(isBoolean()) {
        lexical = boolean ? const_cast<char*>("true") :
                            const_cast<char*>("false");
    } else if(isInteger()) {
        int len = snprintf(NULL, 0, "%ld", integer);
        lexical = new char[len+1];
        sprintf(lexical, "%ld", integer);
        addCleanFlag(CLEAN_LEXICAL);
    } else if(isFloating()) {
        int len = snprintf(NULL, 0, "%f", floating);
        lexical = new char[len+1];
        sprintf(lexical, "%f", floating);
        addCleanFlag(CLEAN_LEXICAL);
    } else if(isDecimal()) {
        std::string str = decimal->getString();
        lexical = new char[str.size()+1];
        strcpy(lexical, str.c_str());
        addCleanFlag(CLEAN_LEXICAL);
    } else if(isDateTime()) {
        // TODO
        lexical = const_cast<char*>("");
    } else {
        lexical = const_cast<char*>("");
    }
}

std::string Value::getString() const {
    std::ostringstream str;

    switch(type) {
    case TYPE_BLANK:
        str << "_:";
        if(lexical)
            str << lexical;
        break;
    case TYPE_IRI:
        str << '<' << lexical << '>';
        break;
    case TYPE_PLAIN_STRING:
        str << '"' << lexical << '"';
        if(language > 0)
            str << '@' << languageTag;
        break;
    default:
        str << '"';
        if(lexical) {
            str << lexical;
        } else {
            if(isBoolean())
                str << (boolean ? "true" : "false");
            else if(isInteger())
                str << integer;
            else if(isFloating())
                str << floating;
            else if(isDecimal()) {
                str << decimal->getString();
            }
            // TODO datetime
        }
        str << "\"^^<" << typeUri << '>';
    }

    return str.str();
}

std::ostream& operator<<(std::ostream &out, const Value &val) {
    return out << val.getString();
}

std::ostream& operator<<(std::ostream &out, const Value *val) {
    if(val)
        return out << val->getString();
    else
        return out;
}

void Value::promoteNumericType(Value &v1, Value &v2) {
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

}
