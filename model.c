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
#include <stdio.h>
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

void model_value_clean(Value* val) {
    if(val->cleanup & VALUE_CLEAN_TYPE_URI) {
        free(val->typeUri);
        val->typeUri = NULL;
    }
    if(val->cleanup & VALUE_CLEAN_LANGUAGE_TAG) {
        free(val->languageTag);
        val->languageTag = NULL;
    }
    if(val->cleanup & VALUE_CLEAN_LEXICAL) {
        free(val->lexical);
        val->lexical = NULL;
    }
    if(val->cleanup & VALUE_CLEAN_DECIMAL) {
        free_xsddecimal(val->decimal);
        val->decimal = NULL;
    }
    val->cleanup = VALUE_CLEAN_NOTHING;
}

int model_value_compare(Value* arg1, Value* arg2) {
    register int i;
    register double d, d1, d2;

    if(IS_VALUE_TYPE_NUMERIC(arg1->type) && IS_VALUE_TYPE_NUMERIC(arg2->type)) {
        if(IS_VALUE_TYPE_INTEGER(arg1->type) &&
           IS_VALUE_TYPE_INTEGER(arg2->type)) {
            i = arg1->integer - arg2->integer;
            if(i < 0) return -1;
            else if(i > 0) return 1;
            else return 0;
        } else if(arg1->type == VALUE_TYPE_DECIMAL &&
                  arg2->type == VALUE_TYPE_DECIMAL) {
            return xsddecimal_compare(arg1->decimal, arg2->decimal);
        } else {
            d1 = IS_VALUE_TYPE_FLOATING(arg1->type) ?
                 arg1->floating :
                 (arg1->type == VALUE_TYPE_DECIMAL ?
                  xsddecimal_get_floating(arg1->decimal) :
                  (double) arg1->integer);
            d2 = IS_VALUE_TYPE_FLOATING(arg2->type) ?
                 arg2->floating :
                 (arg2->type == VALUE_TYPE_DECIMAL ?
                  xsddecimal_get_floating(arg2->decimal) :
                  (double) arg2->integer);
            d = d1 - d2;
            if(d < .0) return -1;
            else if(d > .0) return 1;
            else return 0;
        }
    } else if((arg1->type == VALUE_TYPE_PLAIN_STRING && arg1->language == 0 &&
               arg2->type == VALUE_TYPE_PLAIN_STRING && arg2->language == 0) ||
              (arg1->type == VALUE_TYPE_TYPED_STRING &&
               arg2->type == VALUE_TYPE_TYPED_STRING)) {
        i = strcmp(arg1->lexical, arg2->lexical);
        if(i < 0) return -1;
        else if(i > 0) return 1;
        else return 0;
    } else if(arg1->type == VALUE_TYPE_BOOLEAN &&
              arg2->type == VALUE_TYPE_BOOLEAN) {
        return (arg1->boolean ? 1 : 0) - (arg2->boolean ? 1 : 0);
    } else {
        return -2; // TODO datetime
    }
}

int model_value_equal(Value* arg1, Value* arg2) {
    if(arg1->id == arg2->id)
        return 1;
    if(arg1->type == VALUE_TYPE_UNKOWN || arg2->type == VALUE_TYPE_UNKOWN) {
        if(arg1->typeUri == NULL || arg2->typeUri == NULL)
            return 0; // FIXME not sure
        if(strcmp(arg1->typeUri, arg2->typeUri) != 0)
            return -1;
    } else if(arg1->type != arg2->type) {
        if(arg1->type >= VALUE_TYPE_PLAIN_STRING ||
           arg2->type >= VALUE_TYPE_PLAIN_STRING)
            return -1;
        else
            return 0;
    }
    if(arg1->language < 0 || arg2->language < 0) {
        if(strcmp(arg1->languageTag, arg2->languageTag) != 0)
            return 0;
    } else if(arg1->language != arg2->language) {
        return 0;
    }
    if(strcmp(arg1->lexical, arg2->lexical) != 0)
        return arg1->typeUri == NULL ? 0 : -1;

    return 1;
}

void model_value_ensure_lexical(Value* val) {
    int len;

    if(val->lexical == NULL) {
        if(val->type == VALUE_TYPE_BOOLEAN) {
            val->lexical = val->integer ? "true" : "false";
        } else if(IS_VALUE_TYPE_INTEGER(val->type)) {
            len = snprintf(NULL, 0, "%ld", val->integer);
            val->lexical = (char*) malloc(len * sizeof(char));
            sprintf(val->lexical, "%ld", val->integer);
            val->cleanup |= VALUE_CLEAN_LEXICAL;
        } else if(IS_VALUE_TYPE_FLOATING(val->type)) {
            len = snprintf(NULL, 0, "%f", val->floating);
            val->lexical = (char*) malloc(len * sizeof(char));
            sprintf(val->lexical, "%f", val->floating);
            val->cleanup |= VALUE_CLEAN_LEXICAL;
        } else if(val->type == VALUE_TYPE_DECIMAL) {
            val->lexical = xsddecimal_get_string(val->decimal);
            val->cleanup |= VALUE_CLEAN_LEXICAL;
        } else if(val->type == VALUE_TYPE_DATETIME) {
            // TODO
            val->lexical = "";
        } else {
            val->lexical = "";
        }
    }
}

char* model_value_string(Value* val) {
    char *result, *tmp;
    int lexlen, len;

    lexlen = val->lexical == NULL ? 0 : strlen(val->lexical);

    switch(val->type) {
    case VALUE_TYPE_BLANK:
        len = lexlen + 1;
        result = (char*) malloc(len * sizeof(char));
        strcpy(result, val->lexical == NULL ? "" : val->lexical);
        return result;
    case VALUE_TYPE_IRI:
        len = lexlen + 3;
        result = (char*) malloc(len * sizeof(char));
        result[0] = '<';
        strcpy(result+1, val->lexical == NULL ? "" : val->lexical);
        result[lexlen+1] = '>';
        result[lexlen+2] = '\0';
        return result;
    case VALUE_TYPE_PLAIN_STRING:
        len = lexlen + 3;
        if(val->language > 0) len += strlen(val->languageTag) + 1;
        result = (char*) malloc(len * sizeof(char));
        result[0] = '"';
        strcpy(result+1, val->lexical == NULL ? "" : val->lexical);
        result[lexlen+1] = '"';
        if(val->language > 0) {
            result[lexlen+2] = '@';
            strcpy(result+lexlen+3, val->languageTag);
        }
        result[len-1] = '\0';
        return result;
    default:
        if(val->lexical == NULL) {
            if(val->type == VALUE_TYPE_BOOLEAN)
                lexlen = val->integer ? 4 : 5;
            else if(IS_VALUE_TYPE_INTEGER(val->type))
                lexlen = snprintf(NULL, 0, "%ld", val->integer);
            else if(IS_VALUE_TYPE_FLOATING(val->type))
                lexlen = snprintf(NULL, 0, "%f", val->floating);
            else if(val->type == VALUE_TYPE_DECIMAL) {
                tmp = xsddecimal_get_string(val->decimal);
                lexlen = strlen(tmp);
            }
            // TODO datetime
        }
        len = lexlen + strlen(val->typeUri) + 5;
        result = (char*) malloc(len * sizeof(char));
        result[0] = '"';
        if(val->lexical == NULL) {
            if(val->type == VALUE_TYPE_BOOLEAN)
                strcpy(result+1, val->integer ? "true" : "false");
            else if(IS_VALUE_TYPE_INTEGER(val->type))
                sprintf(result+1, "%ld", val->integer);
            else if(IS_VALUE_TYPE_FLOATING(val->type))
                sprintf(result+1, "%f", val->floating);
            else if(val->type == VALUE_TYPE_DECIMAL) {
                strcpy(result+1, tmp);
                free(tmp);
            }
            // TODO datetime
        } else {
            strcpy(result+1, val->lexical);
        }
        strcpy(result+lexlen+1, "\"^^");
        strcpy(result+lexlen+4, val->typeUri);
        result[len-1] = '\0';
        return result;
    }
}
