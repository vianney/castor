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

#include <stdlib.h>
#include <string.h>

#include "model.h"

char *VALUETYPE_URIS[] = {
    NULL,
    NULL,
    NULL,
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

char* model_value_string(Value* val) {
    char *result;
    int lexlen, typelen, langlen, len;

    lexlen = strlen(val->lexical);
    typelen = val->typeUri == NULL ? 0 : strlen(val->typeUri);
    langlen = val->language == 0 ? 0 : strlen(val->languageTag);

    switch(val->type) {
    case VALUE_TYPE_BLANK:
        len = lexlen + 1;
        result = (char*) malloc(len * sizeof(char));
        strcpy(result, val->lexical);
        return result;
    case VALUE_TYPE_IRI:
        len = lexlen + 3;
        result = (char*) malloc(len * sizeof(char));
        result[0] = '<';
        strcpy(result+1, val->lexical);
        result[lexlen+1] = '>';
        result[lexlen+2] = '\0';
        return result;
    default:
        len = lexlen + 3;
        if(langlen > 0) len += langlen + 1;
        if(typelen > 0) len += typelen + 2;
        result = (char*) malloc(len * sizeof(char));
        result[0] = '"';
        strcpy(result+1, val->lexical);
        result[lexlen+1] = '"';
        len = lexlen + 2;
        if(langlen > 0) {
            result[len++] = '@';
            strcpy(result+len, val->languageTag);
            len += langlen;
        }
        if(typelen > 0) {
            result[len++] = '^';
            result[len++] = '^';
            strcpy(result+len, val->typeUri);
            len += typelen;
        }
        result[len] = '\0';
        return result;
    }
}
