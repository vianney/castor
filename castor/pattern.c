/* This file is part of Castor
 *
 * Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
 * Copyright (C) 2010-2011, UCLouvain
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

#include "pattern.h"

////////////////////////////////////////////////////////////////////////////////
// Constructors and destructors

Pattern* new_pattern_false(Query *query) {
    Pattern *pat;

    pat = (Pattern*) malloc(sizeof(Pattern));
    pat->query = query;
    pat->type = PATTERN_TYPE_FALSE;
    pat->nbVars = 0;
    pat->vars = NULL;
    pat->varMap = (bool*) calloc(query->nbVars, sizeof(bool));
    return pat;
}

Pattern* new_pattern_basic(Query *query, StatementPattern *triples,
                           int nbTriples) {
    Pattern *pat;
    int nbVars, i, x;
    int *vars;
    bool *varMap;

    pat = (Pattern*) malloc(sizeof(Pattern));
    pat->query = query;
    pat->type = PATTERN_TYPE_BASIC;
    pat->nbTriples = nbTriples;
    pat->triples = triples;

    vars = (int*) malloc(query->nbVars * sizeof(int));
    varMap = (bool*) calloc(query->nbVars, sizeof(bool));
    nbVars = 0;
    for(i = 0; i < nbTriples; i++, triples++) {
#define CHECK(part) \
        if(STMTPAT_ISVAR(triples->part)) { \
            x = STMTPAT_IDTOVAR(triples->part); \
            if(!varMap[x]) { \
                vars[nbVars++] = x; \
                varMap[x] = true; \
            } \
        }
        CHECK(subject)
        CHECK(predicate)
        CHECK(object)
#undef CHECK
    }
    pat->nbVars = nbVars;
    pat->vars = (int*) realloc(vars, nbVars * sizeof(int));
    pat->varMap = varMap;

    return pat;
}

Pattern* new_pattern_compound(Query *query, PatternType type,
                              Pattern *left, Pattern *right, Expression *expr) {
    Pattern *pat;
    int nbVars, i, x;
    int *vars;
    bool *varMap;

    pat = (Pattern*) malloc(sizeof(Pattern));
    pat->query = query;
    pat->type = type;
    pat->left = left;
    pat->right = right;
    pat->expr = expr;

    if(type == PATTERN_TYPE_FILTER) {
        pat->vars = left->vars;
        pat->nbVars = left->nbVars;
        pat->varMap = left->varMap;
    } else {
        vars = (int*) malloc(query->nbVars * sizeof(int));
        varMap = (bool*) calloc(query->nbVars, sizeof(bool));
        nbVars = 0;
        for(i = 0; i < left->nbVars; i++) {
            x = left->vars[i];
            vars[nbVars++] = x;
            varMap[x] = true;
        }
        for(i = 0; i < right->nbVars; i++) {
            x = right->vars[i];
            if(!varMap[x]) {
                vars[nbVars++] = x;
                varMap[x] = true;
            }
        }
        pat->nbVars = nbVars;
        pat->vars = (int*) realloc(vars, nbVars * sizeof(int));
        pat->varMap = varMap;
    }

    return pat;
}

void free_pattern(Pattern* pat) {
    switch(pat->type) {
    case PATTERN_TYPE_FALSE:
        free(pat->varMap);
        break;
    case PATTERN_TYPE_BASIC:
        if(pat->triples != NULL)
            free(pat->triples);
        if(pat->vars != NULL)
            free(pat->vars);
        free(pat->varMap);
        break;
    case PATTERN_TYPE_FILTER:
        if(pat->left != NULL)
            free_pattern(pat->left);
        if(pat->expr != NULL)
            free_expression(pat->expr);
        // vars were copied from left, so already freed
        break;
    default:
        if(pat->left != NULL)
            free_pattern(pat->left);
        if(pat->right != NULL)
            free_pattern(pat->right);
        if(pat->expr != NULL)
            free_expression(pat->expr);
        if(pat->vars != NULL)
            free(pat->vars);
        free(pat->varMap);
    }
    free(pat);
}
