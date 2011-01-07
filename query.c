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

#include <rasqal.h>

#include "query.h"

////////////////////////////////////////////////////////////////////////////////
// Shared static information

rasqal_world *world = NULL;
int worldRefCount = 0;

inline void init_shared() {
    if(world == NULL) {
        world = rasqal_new_world();
        rasqal_world_open(world);
    }
    worldRefCount++;
}

inline void free_shared() {
    worldRefCount--;
    if(worldRefCount == 0) {
        rasqal_free_world(world);
        world = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Query interface implementation

struct TQuery {
    /**
     * Store associated to the query
     */
    Store *store;
    /**
     * rasqal query object
     */
    rasqal_query *query;
    /**
     * Number of variables
     */
    int nbVars;
    /**
     * Number of requested variables
     */
    int nbRequestedVars;
    /**
     * Array of variables. The requested variables come first.
     */
    rasqal_variable** variables;
};

////////////////////////////////////////////////////////////////////////////////
// Constructor and destructor

Query* new_query(Store* store, char* queryString) {
    Query *self;
    raptor_sequence *seqVars, *seqAnon, *seqBound;
    int nbReal, nbAnon;
    int i, x;
    rasqal_variable *var;

    init_shared();
    self = (Query*) malloc(sizeof(Query));
    self->store = store;

    self->query = rasqal_new_query(world, "sparql", NULL);
    if(rasqal_query_prepare(self->query, (unsigned char*) queryString, NULL))
        goto cleanquery;

    seqVars = rasqal_query_get_all_variable_sequence(self->query);
    nbReal = raptor_sequence_size(seqVars);

    seqAnon = rasqal_query_get_anonymous_variable_sequence(self->query);
    nbAnon = raptor_sequence_size(seqAnon);

    seqBound = rasqal_query_get_bound_variable_sequence(self->query);
    self->nbRequestedVars = raptor_sequence_size(seqBound);

    self->nbVars = nbReal + nbAnon;
    self->variables = (rasqal_variable**) malloc(self->nbVars *
                                                 sizeof(rasqal_variable*));
    for(x = 0; x < self->nbRequestedVars; x++) {
        var = (rasqal_variable*) raptor_sequence_get_at(seqBound, x);
        self->variables[x] = var;
        var->user_data = &self->variables[x];
    }

    for(i = 0; i < nbReal; i++) {
        var = (rasqal_variable*) raptor_sequence_get_at(seqVars, i);
        if(var->user_data == NULL) {
            self->variables[x] = var;
            var->user_data = &self->variables[x];
            x++;
        }
    }

    for(i = 0; i < nbAnon; i++) {
        var = (rasqal_variable*) raptor_sequence_get_at(seqAnon, i);
        self->variables[x] = var;
        var->user_data = &self->variables[x];
        x++;
    }

    return self;

cleanquery:
    rasqal_free_query(self->query);
    free(self);
    free_shared();
    return NULL;
}

void free_query(Query* self) {
    free(self->variables);
    rasqal_free_query(self->query);
    free(self);
    free_shared();
}

////////////////////////////////////////////////////////////////////////////////
// Variables

int query_variables_count(Query* self) {
    return self->nbVars;
}

int query_variables_requested(Query* self) {
    return self->nbRequestedVars;
}

void query_variable_set(Query* self, int x, Value* v) {
    rasqal_literal *literal;
    raptor_uri *uri;
    char *str, *lang;

    switch(v->type) {
    case VALUE_TYPE_BLANK:
        str = (char*) malloc((strlen(v->lexical)+1) * sizeof(char));
        strcpy(str, v->lexical);
        literal = rasqal_new_simple_literal(world, RASQAL_LITERAL_BLANK,
                                            (unsigned char*) str);
        break;
    case VALUE_TYPE_IRI:
        uri = raptor_new_uri((unsigned char*) v->lexical);
        literal = rasqal_new_uri_literal(world, uri);
        break;
    default:
        uri = v->typeUri == NULL ? NULL : raptor_new_uri((unsigned char*) v->typeUri);
        str = (char*) malloc((strlen(v->lexical)+1) * sizeof(char));
        strcpy(str, v->lexical);
        lang = (char*) malloc((strlen(v->languageTag)+1) * sizeof(char));
        strcpy(lang, v->languageTag);
        literal = rasqal_new_string_literal(world, (unsigned char*) str, lang,
                                            uri, NULL);
    }

    rasqal_variable_set_value(self->variables[x], literal);
}

////////////////////////////////////////////////////////////////////////////////
// Triple patterns

/**
 * @param self a query instance
 * @param literal a rasqal literal
 * @return the positive id of the literal in the store, a negative value for
 *         a variable or -self->nbVars - 1 if unknown
 */
int get_value_id(Query* self, rasqal_literal* literal) {
    ValueType type;
    const char *typeUri, *lexical;
    int id;

    if(literal->type == RASQAL_LITERAL_VARIABLE) {

        return -((rasqal_variable**) literal->value.variable->user_data -
                 self->variables) - 1;
    } else {
        lexical = (const char*) literal->string;
        typeUri = NULL;
        switch(literal->type) {
        case RASQAL_LITERAL_BLANK:
            type = VALUE_TYPE_BLANK;
            break;
        case RASQAL_LITERAL_URI:
            type = VALUE_TYPE_IRI;
            lexical = (const char*) raptor_uri_as_string(literal->value.uri);
            break;
        case RASQAL_LITERAL_STRING:
            type = VALUE_TYPE_PLAIN_STRING;
            break;
        case RASQAL_LITERAL_XSD_STRING:
            type = VALUE_TYPE_TYPED_STRING;
            break;
        case RASQAL_LITERAL_BOOLEAN:
            type = VALUE_TYPE_BOOLEAN;
            break;
        case RASQAL_LITERAL_FLOAT:
            type = VALUE_TYPE_FLOAT;
            break;
        case RASQAL_LITERAL_DOUBLE:
            type = VALUE_TYPE_DOUBLE;
            break;
        case RASQAL_LITERAL_DECIMAL:
            type = VALUE_TYPE_DECIMAL;
            break;
        case RASQAL_LITERAL_DATETIME:
            type = VALUE_TYPE_DATETIME;
            break;
        case RASQAL_LITERAL_INTEGER: // what kind of integer precisely?
        case RASQAL_LITERAL_UDT:
            type = VALUE_TYPE_UNKOWN;
            typeUri = (const char*) raptor_uri_as_string(literal->datatype);
            break;
        default:
            fprintf(stderr, "castor query: unknown rasqal literal type %d\n",
                    literal->type);
            return -self->nbVars - 1;
        }

        id = store_value_get_id(self->store, type, typeUri, lexical,
                                literal->language);
        if(id < 0)
            return -self->nbVars - 1;
        else
            return id;
    }
}

bool query_visit_triple_patterns(Query* self, query_triple_pattern_visit_fn fn,
                                 void* userData) {
    raptor_sequence *seq;
    rasqal_triple *triple;
    int i, n, unknown;
    Statement stmt;

    unknown = -self->nbVars - 1;

    seq = rasqal_query_get_triple_sequence(self->query);
    n = raptor_sequence_size(seq);
    for(i = 0; i < n; i++) {
        triple = (rasqal_triple*) raptor_sequence_get_at(seq, i);
        stmt.subject = get_value_id(self, triple->subject);
        stmt.predicate = get_value_id(self, triple->predicate);
        stmt.object = get_value_id(self, triple->object);
        if(stmt.subject == unknown || stmt.predicate == unknown ||
           stmt.object == unknown)
            return false;
        fn(stmt, userData);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Filters

/**
 * Structure used through the recursive calls when visiting a filter.
 */
typedef struct {
    /**
     * The query instance
     */
    Query *self;
    /**
     * Callback function
     */
    query_filter_visit_fn fn;
    /**
     * User-defined data
     */
    void *userData;
    /**
     * Number of variables in vars
     */
    int nbVars;
    /**
     * Array of size self->nbVars with the variables
     */
    int* vars;
    /**
     * Is a variable included in vars.
     * varIncluded[i] = true <=> ∃ j ∈ 0..nbVars-1 : vars[j] = i
     */
    bool* varIncluded;
} FilterVisitData;

/**
 * Get the variables of an expression, putting them in data->vars
 *
 * @param expr the expression
 * @param data visit data
 */
void fill_expr_vars(rasqal_expression *expr, FilterVisitData *data) {
    int i, n;

    switch(expr->op) {
    case RASQAL_EXPR_REGEX:
        fill_expr_vars(expr->arg3, data); // 3op
    case RASQAL_EXPR_AND:
    case RASQAL_EXPR_OR:
    case RASQAL_EXPR_EQ:
    case RASQAL_EXPR_NEQ:
    case RASQAL_EXPR_LT:
    case RASQAL_EXPR_GT:
    case RASQAL_EXPR_LE:
    case RASQAL_EXPR_GE:
    case RASQAL_EXPR_PLUS:
    case RASQAL_EXPR_MINUS:
    case RASQAL_EXPR_STAR:
    case RASQAL_EXPR_SLASH:
    case RASQAL_EXPR_LANGMATCHES:
    case RASQAL_EXPR_SAMETERM:
        fill_expr_vars(expr->arg2, data); // 2op
    case RASQAL_EXPR_BANG:
    case RASQAL_EXPR_UMINUS:
    case RASQAL_EXPR_BOUND:
    case RASQAL_EXPR_STR:
    case RASQAL_EXPR_LANG:
    case RASQAL_EXPR_DATATYPE:
    case RASQAL_EXPR_ISURI:
    case RASQAL_EXPR_ISBLANK:
    case RASQAL_EXPR_ISLITERAL:
    case RASQAL_EXPR_COUNT:
    case RASQAL_EXPR_SUM:
    case RASQAL_EXPR_AVG:
    case RASQAL_EXPR_MIN:
    case RASQAL_EXPR_MAX:
        fill_expr_vars(expr->arg1, data); // 1op
    case RASQAL_EXPR_VARSTAR:
        break;                           // 0op
    case RASQAL_EXPR_FUNCTION:
        n = raptor_sequence_size(expr->args);
        for(i = 0; i < n; i++)
            fill_expr_vars((rasqal_expression*)
                           raptor_sequence_get_at(expr->args, i), data);
        break;
    case RASQAL_EXPR_LITERAL:
        if(expr->literal->type == RASQAL_LITERAL_VARIABLE) {
            i = (rasqal_variable**) expr->literal->value.variable->user_data -
                data->self->variables;
            if(!data->varIncluded[i]) {
                data->vars[data->nbVars++] = i;
                data->varIncluded[i] = true;
            }
        }
        break;
    default:
        fprintf(stderr, "castor query: unknown rasqal expression op %d\n",
                expr->op);
        break;
    }
}

/**
 * Visit an expression breaking top-level AND-clauses down.
 *
 * @param expr the expression
 * @param data visit data
 */
void visit_expr(rasqal_expression *expr, FilterVisitData *data) {
    if(expr->op == RASQAL_EXPR_AND) {
        visit_expr(expr->arg1, data);
        visit_expr(expr->arg2, data);
    } else {
        data->nbVars = 0;
        memset(data->varIncluded, false, data->self->nbVars * sizeof(bool));
        fill_expr_vars(expr, data);
        data->fn((Filter) expr, data->nbVars, data->vars, data->userData);
    }
}

void query_visit_filters(Query* self, query_filter_visit_fn fn, void* userData) {
    raptor_sequence *seq;
    int i, n;
    rasqal_graph_pattern *gp;
    rasqal_expression *expr;
    FilterVisitData data;

    data.self = self;
    data.fn = fn;
    data.userData = userData;
    data.vars = (int*) malloc(self->nbVars * sizeof(int));
    data.varIncluded = (int*) malloc(self->nbVars * sizeof(int));

    seq = rasqal_query_get_graph_pattern_sequence(self->query);
    n = raptor_sequence_size(seq);
    for(i = 0; i < n; i++) {
        gp = (rasqal_graph_pattern*) raptor_sequence_get_at(seq, i);
        expr = rasqal_graph_pattern_get_filter_expression(gp);
        if(expr != NULL)
            visit_expr(expr, &data);
    }

    free(data.vars);
    free(data.varIncluded);
}

bool query_filter_evaluate(Query* UNUSED(self), Filter filter) {
    rasqal_literal *literal;
    bool result;

    literal = rasqal_expression_evaluate(world, NULL,
                                        (rasqal_expression*) filter, 0);
    if(literal == NULL)
        return false;
    result = literal->type == RASQAL_LITERAL_BOOLEAN ?
             literal->value.integer : false;
    rasqal_free_literal(literal);
    return result;
}
