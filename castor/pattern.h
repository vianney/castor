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
#ifndef PATTERN_H
#define PATTERN_H

typedef struct TPattern Pattern;

#include "defs.h"
#include "model.h"

/**
 * A statement pattern has the same structure as a statement, but allows
 * negative ids for designing variables. The variable number is retrieved with
 * -id - 1.
 */
typedef Statement StatementPattern;

#include "query.h"

/**
 * @param id an id of a statement pattern
 * @return whether id refers to a variable
 */
#define STMTPAT_ISVAR(id) ((id) < 0)

/**
 * @param id the negative id of a statement pattern
 * @return the corresponding variable number
 */
#define STMTPAT_IDTOVAR(id) (-(id)-1)

/**
 * @param x a variable number
 * @return the corresponding negative id to be used in a statement pattern
 */
#define STMTPAT_VARTOID(x) (-(x)-1)

/**
 * Pattern type enumeration
 */
typedef enum {
    PATTERN_TYPE_BASIC,
    PATTERN_TYPE_FILTER,
    PATTERN_TYPE_JOIN,
    PATTERN_TYPE_LEFTJOIN,
    PATTERN_TYPE_UNION
} PatternType;

/**
 * Structure for a SPARQL graph pattern
 */
struct TPattern {
    /**
     * Parent query
     */
    Query *query;
    /**
     * Type
     */
    PatternType type;
    /**
     * Number of variables used in this pattern
     */
    int nbVars;
    /**
     * Variables used in this pattern
     */
    int *vars;
    /**
     * Map of variables used in this pattern
     * varMap[x] <=> x in vars
     */
    bool *varMap;
    /**
     * Operands dependent on the type
     */
    union {
        /**
         * type == PATTERN_TYPE_BASIC
         */
        struct {
            int nbTriples;
            StatementPattern *triples;
        };
        /**
         * compound types
         */
        struct {
            Pattern *left;
            Pattern *right; // not for PATTERN_TYPE_FILTER
            Expression *expr; // only for PATTERN_TYPE_FILTER and PATTERN_TYPE_LEFTJOIN
        };
    };
};

////////////////////////////////////////////////////////////////////////////////
// Constructors and destructors

/**
 * @param query parent query
 * @param triples array of triple patterns (ownership is taken)
 * @param nbTriples number of triple patterns
 * @return a new basic graph pattern
 */
Pattern* new_pattern_basic(Query *query, StatementPattern *triples, int nbTriples);

/**
 * @param query parent query
 * @param type pattern type
 * @param left left subpattern
 * @param right right subpattern (NULL for PATTERN_TYPE_FILTER)
 * @param expr expression (NULL if not relevant)
 * @return a new compound pattern
 */
Pattern* new_pattern_compound(Query *query, PatternType type,
                              Pattern *left, Pattern *right, Expression *expr);

/**
 * Free a pattern and recursively all its subpatterns. Expressions will be freed
 * too.
 */
void free_pattern(Pattern* pat);

#endif // PATTERN_H
