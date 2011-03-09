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
// Query data structure

/**
 * Variable structure
 */
typedef struct {
    /**
     * Index of the variable
     */
    int id;
    /**
     * Value bound to the variable or NULL if unbound
     */
    Value *value;
} Variable;

struct TQuery {
    /**
     * Store associated to the query
     */
    Store *store;
    /**
     * Type of query
     */
    QueryType type;
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
    Variable* vars;
    /**
     * Number of triple patterns
     */
    int nbPatterns;
    /**
     * Array of triple patterns
     */
    StatementPattern *patterns;
    /**
     * Number of filters
     */
    int nbFilters;
    /**
     * Array of filters
     */
    Expression** filters;
};

////////////////////////////////////////////////////////////////////////////////
// Helper functions for query construction

/**
 * @param self a query instance (store and variables should be initialized)
 * @param literal a rasqal literal
 * @return the positive id of the literal in the store, a negative value for
 *         a variable or 0 if unknown
 */
int get_value_id(Query* self, rasqal_literal* literal) {
    ValueType type;
    const char *typeUri, *lexical;
    int id;

    if(literal->type == RASQAL_LITERAL_VARIABLE) {
        return STMTPAT_VARTOID(
                ((Variable*) literal->value.variable->user_data)->id);
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
            return 0;
        }

        id = store_value_get_id(self->store, type, typeUri, lexical,
                                literal->language);
        if(id < 0)
            return 0;
        else
            return id;
    }
}

/**
 * Copy a raptor_uri into a new string
 */
char* convert_uri(raptor_uri *uri) {
    char *str, *result;

    str = (char*) raptor_uri_as_string(uri);
    result = (char*) malloc((strlen(str)+1) * sizeof(char));
    strcpy(result, str);
    return result;
}

/**
 * Create an Expression from a rasqal_expression.
 *
 * @param self a query instance (store and variables should be initialized)
 * @param expr the rasqal expression
 * @return the new expression
 */
Expression* convert_expression(Query* self, rasqal_expression* expr) {
    int i;
    rasqal_literal *lit;
    Value *val;

    switch(expr->op) {
    case RASQAL_EXPR_LITERAL:
        lit = expr->literal;
        if(lit->type == RASQAL_LITERAL_VARIABLE) {
            return new_expression_variable(self,
                    ((Variable*) lit->value.variable->user_data)->id);
        } else {
            i = get_value_id(self, lit);
            if(i > 0)
                return new_expression_value(self,
                                            store_value_get(self->store, i),
                                            false);
            val = (Value*) malloc(sizeof(Value));
            val->id = 0;
            val->language = 0;
            val->languageTag = NULL;
            val->lexical = NULL;
            val->cleanup = VALUE_CLEAN_NOTHING;
            switch(lit->type) {
            case RASQAL_LITERAL_BLANK:
                val->type = VALUE_TYPE_BLANK;
                break;
            case RASQAL_LITERAL_URI:
                val->type = VALUE_TYPE_IRI;
                val->lexical = convert_uri(lit->value.uri);
                val->cleanup |= VALUE_CLEAN_LEXICAL;
                break;
            case RASQAL_LITERAL_STRING:
                val->type = VALUE_TYPE_PLAIN_STRING;
                if(lit->language == NULL || lit->language[0] == '\0') {
                    val->language = 0;
                    val->languageTag = "";
                } else {
                    val->language = -1;
                    val->languageTag = (char*) malloc((strlen(lit->language)+1)
                                                      * sizeof(char));
                    strcpy(val->languageTag, lit->language);
                    val->cleanup |= VALUE_CLEAN_LANGUAGE_TAG;
                }
                break;
            case RASQAL_LITERAL_XSD_STRING:
                val->type = VALUE_TYPE_TYPED_STRING;
                break;
            case RASQAL_LITERAL_BOOLEAN:
                val->type = VALUE_TYPE_BOOLEAN;
                val->boolean = lit->value.integer;
                val->lexical = val->boolean ? "true" : "false";
                break;
            case RASQAL_LITERAL_INTEGER:
                val->type = VALUE_TYPE_INTEGER;
                val->integer = lit->value.integer;
                break;
            case RASQAL_LITERAL_FLOAT:
                val->type = VALUE_TYPE_FLOAT;
                val->floating = lit->value.floating;
                break;
            case RASQAL_LITERAL_DOUBLE:
                val->type = VALUE_TYPE_DOUBLE;
                val->floating = lit->value.floating;
                break;
            case RASQAL_LITERAL_DECIMAL:
                val->type = VALUE_TYPE_DECIMAL;
                val->decimal = new_xsddecimal();
                xsddecimal_set_string(val->decimal, (const char*) lit->string);
                val->cleanup |= VALUE_CLEAN_DECIMAL;
                break;
            case RASQAL_LITERAL_DATETIME:
                val->type = VALUE_TYPE_DATETIME;
                fprintf(stderr, "castor query: datetime unimplemented\n");
                break;
            case RASQAL_LITERAL_UDT:
                val->type = VALUE_TYPE_UNKOWN;
                val->typeUri = convert_uri(lit->datatype);
                val->cleanup |= VALUE_CLEAN_TYPE_URI;
                break;
            default:
                fprintf(stderr, "castor query: unknown rasqal literal type %d\n",
                        lit->type);
                free(val);
                return NULL;

            }
            if(val->type != VALUE_TYPE_UNKOWN &&
               val->type < VALUE_TYPE_FIRST_CUSTOM)
                val->typeUri = VALUETYPE_URIS[val->type];
            if(val->lexical == NULL) {
                val->lexical = (char*) malloc((lit->string_len+1) * sizeof(char));
                strcpy(val->lexical, (const char*) lit->string);
                val->cleanup |= VALUE_CLEAN_LEXICAL;
            }
            return new_expression_value(self, val, true);
        }
    case RASQAL_EXPR_BANG:
        return new_expression_unary(self, EXPR_OP_BANG,
                                    convert_expression(self, expr->arg1));
    case RASQAL_EXPR_UMINUS:
        return new_expression_unary(self, EXPR_OP_UMINUS,
                                    convert_expression(self, expr->arg1));
    case RASQAL_EXPR_BOUND:
        return new_expression_unary(self, EXPR_OP_BOUND,
                                    convert_expression(self, expr->arg1));
    case RASQAL_EXPR_ISURI:
        return new_expression_unary(self, EXPR_OP_ISIRI,
                                    convert_expression(self, expr->arg1));
    case RASQAL_EXPR_ISBLANK:
        return new_expression_unary(self, EXPR_OP_ISBLANK,
                                    convert_expression(self, expr->arg1));
    case RASQAL_EXPR_ISLITERAL:
        return new_expression_unary(self, EXPR_OP_ISLITERAL,
                                    convert_expression(self, expr->arg1));
    case RASQAL_EXPR_STR:
        return new_expression_unary(self, EXPR_OP_STR,
                                    convert_expression(self, expr->arg1));
    case RASQAL_EXPR_LANG:
        return new_expression_unary(self, EXPR_OP_LANG,
                                    convert_expression(self, expr->arg1));
    case RASQAL_EXPR_DATATYPE:
        return new_expression_unary(self, EXPR_OP_DATATYPE,
                                    convert_expression(self, expr->arg1));
    case RASQAL_EXPR_OR:
        return new_expression_binary(self, EXPR_OP_OR,
                                     convert_expression(self, expr->arg1),
                                     convert_expression(self, expr->arg2));
    case RASQAL_EXPR_AND:
        return new_expression_binary(self, EXPR_OP_AND,
                                     convert_expression(self, expr->arg1),
                                     convert_expression(self, expr->arg2));
    case RASQAL_EXPR_EQ:
        return new_expression_binary(self, EXPR_OP_EQ,
                                     convert_expression(self, expr->arg1),
                                     convert_expression(self, expr->arg2));
    case RASQAL_EXPR_NEQ:
        return new_expression_binary(self, EXPR_OP_NEQ,
                                     convert_expression(self, expr->arg1),
                                     convert_expression(self, expr->arg2));
    case RASQAL_EXPR_LT:
        return new_expression_binary(self, EXPR_OP_LT,
                                     convert_expression(self, expr->arg1),
                                     convert_expression(self, expr->arg2));
    case RASQAL_EXPR_GT:
        return new_expression_binary(self, EXPR_OP_GT,
                                     convert_expression(self, expr->arg1),
                                     convert_expression(self, expr->arg2));
    case RASQAL_EXPR_LE:
        return new_expression_binary(self, EXPR_OP_LE,
                                     convert_expression(self, expr->arg1),
                                     convert_expression(self, expr->arg2));
    case RASQAL_EXPR_GE:
        return new_expression_binary(self, EXPR_OP_GE,
                                     convert_expression(self, expr->arg1),
                                     convert_expression(self, expr->arg2));
    case RASQAL_EXPR_STAR:
        return new_expression_binary(self, EXPR_OP_STAR,
                                     convert_expression(self, expr->arg1),
                                     convert_expression(self, expr->arg2));
    case RASQAL_EXPR_SLASH:
        return new_expression_binary(self, EXPR_OP_SLASH,
                                     convert_expression(self, expr->arg1),
                                     convert_expression(self, expr->arg2));
    case RASQAL_EXPR_PLUS:
        return new_expression_binary(self, EXPR_OP_PLUS,
                                     convert_expression(self, expr->arg1),
                                     convert_expression(self, expr->arg2));
    case RASQAL_EXPR_MINUS:
        return new_expression_binary(self, EXPR_OP_MINUS,
                                     convert_expression(self, expr->arg1),
                                     convert_expression(self, expr->arg2));
    case RASQAL_EXPR_SAMETERM:
        return new_expression_binary(self, EXPR_OP_SAMETERM,
                                     convert_expression(self, expr->arg1),
                                     convert_expression(self, expr->arg2));
    case RASQAL_EXPR_LANGMATCHES:
        return new_expression_binary(self, EXPR_OP_LANGMATCHES,
                                     convert_expression(self, expr->arg1),
                                     convert_expression(self, expr->arg2));
    case RASQAL_EXPR_REGEX:
        return new_expression_trinary(self, EXPR_OP_REGEX,
                                     convert_expression(self, expr->arg1),
                                     convert_expression(self, expr->arg2),
                                     convert_expression(self, expr->arg3));
    default:
        fprintf(stderr, "castor query: unsupported rasqal expression op %d\n",
                expr->op);
        return NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Constructor and destructor

Query* new_query(Store* store, char* queryString) {
    Query *self;
    rasqal_world *world;
    rasqal_query *query;
    raptor_sequence *seq, *seqVars, *seqAnon;
    int nbReal, nbAnon;
    int i, x, n;
    rasqal_variable *var;
    rasqal_triple *triple;
    StatementPattern *stmt;
    rasqal_graph_pattern *gp;
    rasqal_expression *expr;

    world = rasqal_new_world();
    rasqal_world_open(world);
    query = rasqal_new_query(world, "sparql", NULL);
    if(rasqal_query_prepare(query, (unsigned char*) queryString, NULL))
        goto cleanquery;

    self = (Query*) malloc(sizeof(Query));
    self->store = store;

    // information
    switch(rasqal_query_get_verb(query)) {
    case RASQAL_QUERY_VERB_SELECT:
        self->type = QUERY_TYPE_SELECT;
        break;
    case RASQAL_QUERY_VERB_ASK:
        self->type = QUERY_TYPE_ASK;
        break;
    default:
        fprintf(stderr, "castor query: unsupported rasqal verb %d\n",
                rasqal_query_get_verb(query));
        goto cleanself;
    }

    // variables
    if(self->type == QUERY_TYPE_SELECT) {
        seq = rasqal_query_get_bound_variable_sequence(query);
        self->nbRequestedVars = raptor_sequence_size(seq);
    } else {
        self->nbRequestedVars = 0;
    }
    seqVars = rasqal_query_get_all_variable_sequence(query);
    nbReal = raptor_sequence_size(seqVars);
    seqAnon = rasqal_query_get_anonymous_variable_sequence(query);
    nbAnon = raptor_sequence_size(seqAnon);

    self->nbVars = nbReal + nbAnon;
    self->vars = (Variable*) malloc(self->nbVars * sizeof(Variable));
    for(x = 0; x < self->nbRequestedVars; x++) {
        var = (rasqal_variable*) raptor_sequence_get_at(seq, x);
        var->user_data = &self->vars[x];
        self->vars[x].id = x;
        self->vars[x].value = NULL;
    }
    for(i = 0; i < nbReal; i++) {
        var = (rasqal_variable*) raptor_sequence_get_at(seqVars, i);
        if(var->user_data == NULL) {
            var->user_data = &self->vars[x];
            self->vars[x].id = x;
            self->vars[x].value = NULL;
            x++;
        }
    }
    for(i = 0; i < nbAnon; i++) {
        var = (rasqal_variable*) raptor_sequence_get_at(seqAnon, i);
        var->user_data = &self->vars[x];
        self->vars[x].id = x;
        self->vars[x].value = NULL;
        x++;
    }

    // triple patterns
    seq = rasqal_query_get_triple_sequence(query);
    self->nbPatterns = raptor_sequence_size(seq);
    self->patterns = (StatementPattern*) malloc(self->nbPatterns *
                                                sizeof(StatementPattern));
    for(i = 0; i < self->nbPatterns; i++) {
        triple = (rasqal_triple*) raptor_sequence_get_at(seq, i);
        stmt = &self->patterns[i];
        stmt->subject = get_value_id(self, triple->subject);
        stmt->predicate = get_value_id(self, triple->predicate);
        stmt->object = get_value_id(self, triple->object);
    }

    // filters
    seq = rasqal_query_get_graph_pattern_sequence(query);
    self->nbFilters = 0;
    if(seq != NULL) {
        n = raptor_sequence_size(seq);
        self->nbFilters = 0;
        for(i = 0; i < n; i++) {
            gp = (rasqal_graph_pattern*) raptor_sequence_get_at(seq, i);
            if(rasqal_graph_pattern_get_filter_expression(gp) != NULL)
                self->nbFilters++;
        }
    }
    if(self->nbFilters > 0) {
        self->filters = (Expression**) malloc(self->nbFilters *
                                              sizeof(Expression*));
        x = 0;
        for(i = 0; i < n; i++) {
            gp = (rasqal_graph_pattern*) raptor_sequence_get_at(seq, i);
            expr = rasqal_graph_pattern_get_filter_expression(gp);
            if(expr != NULL)
                self->filters[x++] = convert_expression(self, expr);
        }
    } else {
        self->filters = NULL;
    }

    rasqal_free_query(query);
    rasqal_free_world(world);
    return self;

cleanself:
    free(self);
cleanquery:
    rasqal_free_query(query);
    rasqal_free_world(world);
    return NULL;
}

void free_query(Query* self) {
    int i;

    for(i = 0; i < self->nbFilters; i++)
        free_expression(self->filters[i]);
    if(self->filters != NULL)
        free(self->filters);
    free(self->patterns);
    free(self->vars);
    free(self);
}

////////////////////////////////////////////////////////////////////////////////
// Information

QueryType query_get_type(Query* self) {
    return self->type;
}

////////////////////////////////////////////////////////////////////////////////
// Variables

int query_variable_count(Query* self) {
    return self->nbVars;
}

int query_variable_requested(Query* self) {
    return self->nbRequestedVars;
}

Value* query_variable_get(Query* self, int x) {
    return self->vars[x].value;
}

void query_variable_bind(Query* self, int x, Value* v) {
    self->vars[x].value = v;
}

////////////////////////////////////////////////////////////////////////////////
// Triple patterns

int query_triple_pattern_count(Query* self) {
    return self->nbPatterns;
}

StatementPattern* query_triple_pattern_get(Query* self, int i) {
    return &self->patterns[i];
}

////////////////////////////////////////////////////////////////////////////////
// Filters

int query_filter_count(Query* self) {
    return self->nbFilters;
}

Expression* query_filter_get(Query* self, int i) {
    return self->filters[i];
}
