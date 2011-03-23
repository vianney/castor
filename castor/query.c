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
                                (Variable*) lit->value.variable->user_data);
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

/**
 * Create a graph pattern from a rasqal_graph_pattern
 *
 * @param self a query instance (store and variables should be initialized)
 * @param gp a rasqal_graph_pattern
 * @return the new pattern
 */
Pattern* convert_pattern(Query* self, rasqal_graph_pattern* gp) {
    Pattern *pat, *subpat;
    int i, n, m;
    raptor_sequence *seq;
    rasqal_graph_pattern *subgp;
    rasqal_triple *triple;
    StatementPattern *stmts;
    Expression *expr, *subexpr;

    switch(rasqal_graph_pattern_get_operator(gp)) {
    case RASQAL_GRAPH_PATTERN_OPERATOR_BASIC:
        // FIXME better way of getting the number of triples?
        for(n = 0; rasqal_graph_pattern_get_triple(gp, n) != NULL; n++);
        stmts = (StatementPattern*) malloc(n * sizeof(StatementPattern));
        for(i = 0; i < n; i++) {
            triple = rasqal_graph_pattern_get_triple(gp, i);
            stmts[i].subject = get_value_id(self, triple->subject);
            stmts[i].predicate = get_value_id(self, triple->predicate);
            stmts[i].object = get_value_id(self, triple->object);
            if(stmts[i].subject == 0 ||
               stmts[i].predicate == 0 ||
               stmts[i].object == 0) {
                // We have an unknown value, this BGP will never match
                free(stmts);
                return new_pattern_false(self);
            }
        }
        return new_pattern_basic(self, stmts, n);
    case RASQAL_GRAPH_PATTERN_OPERATOR_UNION:
        pat = NULL;
        seq = rasqal_graph_pattern_get_sub_graph_pattern_sequence(gp);
        n = raptor_sequence_size(seq);
        for(i = 0; i < n; i++) {
            subgp = raptor_sequence_get_at(seq, i);
            subpat = convert_pattern(self, subgp);
            if(subpat->type == PATTERN_TYPE_FALSE) {
                free_pattern(subpat);
                continue;
            }
            if(pat == NULL)
                pat = subpat;
            else
                pat = new_pattern_compound(self, PATTERN_TYPE_UNION,
                                           pat, subpat, NULL);
        }
        if(pat == NULL)
            pat = new_pattern_false(self);
        return pat;
    case RASQAL_GRAPH_PATTERN_OPERATOR_GROUP:
        expr = NULL;
        pat = NULL;
        seq = rasqal_graph_pattern_get_sub_graph_pattern_sequence(gp);
        n = raptor_sequence_size(seq);
        for(i = 0; i < n; i++) {
            subgp = raptor_sequence_get_at(seq, i);
            switch(rasqal_graph_pattern_get_operator(subgp)) {
            case RASQAL_GRAPH_PATTERN_OPERATOR_FILTER:
                subexpr = convert_expression(self,
                            rasqal_graph_pattern_get_filter_expression(subgp));
                if(expr == NULL)
                    expr = subexpr;
                else
                    expr = new_expression_binary(self, EXPR_OP_AND,
                                                 expr, subexpr);
                break;
            case RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL:
                m = raptor_sequence_size(
                        rasqal_graph_pattern_get_sub_graph_pattern_sequence(subgp));
                if(m != 1) {
                    fprintf(stderr,
                            "castor query: ignoring OPTIONAL pattern with %d subpatterns\n",
                            m);
                    break;
                }
                subpat = convert_pattern(self,
                        rasqal_graph_pattern_get_sub_graph_pattern(subgp, 0));
                if(subpat->type == PATTERN_TYPE_FALSE) {
                    free_pattern(subpat);
                    break;
                }
                if(pat == NULL)
                    pat = new_pattern_basic(self, NULL, 0); // empty pattern
//                if(subpat->type == PATTERN_TYPE_FILTER) {
//                    pat = new_pattern_compound(self, PATTERN_TYPE_LEFTJOIN,
//                                               pat, subpat->left, subpat->expr);
//                    subpat->left = NULL; // do not free these
//                    subpat->expr = NULL;
//                    free_pattern(subpat);
//                } else {
                    pat = new_pattern_compound(self, PATTERN_TYPE_LEFTJOIN,
                                               pat, subpat, NULL);
//                }
                break;
            default:
                subpat = convert_pattern(self, subgp);
                if(subpat->type == PATTERN_TYPE_FALSE) {
                    // one false pattern in a join makes the whole group false
                    if(pat != NULL)
                        free_pattern(pat);
                    if(expr != NULL)
                        free_expression(expr);
                    return subpat;
                }
                if(pat == NULL)
                    pat = subpat;
                else
                    pat = new_pattern_compound(self, PATTERN_TYPE_JOIN,
                                               pat, subpat, NULL);
            }
        }
        if(pat == NULL)
            pat = new_pattern_basic(self, NULL, 0); // empty pattern
        if(expr != NULL) {
            if(pat->type == PATTERN_TYPE_LEFTJOIN &&
               expr->op == EXPR_OP_BANG &&
               expr->arg1->op == EXPR_OP_BOUND &&
               expr->arg1->arg1->op == EXPR_OP_VARIABLE &&
               !pat->left->varMap[expr->arg1->arg1->variable->id] &&
               pat->right->varMap[expr->arg1->arg1->variable->id]) {
                subpat = new_pattern_compound(self, PATTERN_TYPE_MINUS,
                                              pat->left, pat->right, pat->expr);
                pat->left = NULL; // do not free these
                pat->right = NULL;
                pat->expr = NULL;
                free_pattern(pat);
                pat = subpat;
                free_expression(expr);
            } else {
                pat = new_pattern_compound(self, PATTERN_TYPE_FILTER,
                                           pat, NULL, expr);
            }
        }
        return pat;
    case RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL:
        // lone optional pattern
        n = raptor_sequence_size(
                rasqal_graph_pattern_get_sub_graph_pattern_sequence(gp));
        if(n != 1) {
            fprintf(stderr,
                    "castor query: ignoring OPTIONAL pattern with %d subpatterns\n",
                    n);
            break;
        }
        pat = new_pattern_basic(self, NULL, 0); // empty pattern
        subpat = convert_pattern(self,
                rasqal_graph_pattern_get_sub_graph_pattern(gp, 0));
        if(subpat->type == PATTERN_TYPE_FALSE) {
            free_pattern(subpat);
            return pat;
        }
        if(subpat->type == PATTERN_TYPE_FILTER) {
            pat = new_pattern_compound(self, PATTERN_TYPE_LEFTJOIN,
                                       pat, subpat->left, subpat->expr);
            subpat->left = NULL; // do not free these
            subpat->expr = NULL;
            free_pattern(subpat);
        } else {
            pat = new_pattern_compound(self, PATTERN_TYPE_LEFTJOIN,
                                       pat, subpat, NULL);
        }
        return pat;
    case RASQAL_GRAPH_PATTERN_OPERATOR_FILTER:
        // lone filter pattern
        expr = convert_expression(self,
                            rasqal_graph_pattern_get_filter_expression(gp));
        return new_pattern_compound(self, PATTERN_TYPE_FILTER,
                                    new_pattern_basic(self, NULL, 0), // empty pattern
                                    NULL, expr);
    default:
        fprintf(stderr, "castor query: unsupported rasqal graph pattern op %d\n",
                rasqal_graph_pattern_get_operator(gp));
    }
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Constructor and destructor

Query* new_query(Store* store, char* queryString) {
    Query *self;
    rasqal_world *world;
    rasqal_query *query;
    raptor_sequence *seq, *seqVars, *seqAnon;
    int nbReal, nbAnon;
    int i, x;
    rasqal_variable *var;

    world = rasqal_new_world();
    rasqal_world_open(world);
    query = rasqal_new_query(world, "sparql", NULL);
    if(rasqal_query_prepare(query, (unsigned char*) queryString, NULL))
        goto cleanquery;

    self = (Query*) malloc(sizeof(Query));
    self->store = store;

    // variables
    switch(rasqal_query_get_verb(query)) {
    case RASQAL_QUERY_VERB_SELECT:
        self->limit = rasqal_query_get_limit(query); // TODO check if no limit
        seq = rasqal_query_get_bound_variable_sequence(query);
        self->nbRequestedVars = raptor_sequence_size(seq);
        break;
    case RASQAL_QUERY_VERB_ASK:
        self->limit = 1;
        self->nbRequestedVars = 0;
        break;
    default:
        fprintf(stderr, "castor query: unsupported rasqal verb %d\n",
                rasqal_query_get_verb(query));
        goto cleanself;
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
        self->vars[x].name = (char*) malloc((strlen((const char*)var->name)+1)*sizeof(char));
        strcpy(self->vars[x].name, (const char*) var->name);
        self->vars[x].value = NULL;
    }
    for(i = 0; i < nbReal; i++) {
        var = (rasqal_variable*) raptor_sequence_get_at(seqVars, i);
        if(var->user_data == NULL) {
            var->user_data = &self->vars[x];
            self->vars[x].id = x;
            self->vars[x].name = (char*) malloc((strlen((const char*)var->name)+1)*sizeof(char));
            strcpy(self->vars[x].name, (const char*) var->name);
            self->vars[x].value = NULL;
            x++;
        }
    }
    for(i = 0; i < nbAnon; i++) {
        var = (rasqal_variable*) raptor_sequence_get_at(seqAnon, i);
        var->user_data = &self->vars[x];
        self->vars[x].id = x;
        self->vars[x].name = NULL;
        self->vars[x].value = NULL;
        x++;
    }

    // graph pattern
    self->pattern = convert_pattern(self,
                                    rasqal_query_get_query_graph_pattern(query));

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
    int x;

    free_pattern(self->pattern);
    for(x = 0; x < self->nbVars; x++) {
        if(self->vars[x].name != NULL)
            free(self->vars[x].name);
    }
    free(self->vars);
    free(self);
}

////////////////////////////////////////////////////////////////////////////////
// Debugging

/**
 * Print the pattern to f
 *
 * @param pat the pattern
 * @param f output file
 * @param indent number of spaces to add in front of each line
 */
void print_pattern(Pattern* pat, FILE* f, int indent) {
    switch(pat->type) {
    case PATTERN_TYPE_FALSE:
        fprintf(f, "%*sFalse\n", indent, "");
        break;
    case PATTERN_TYPE_BASIC:
        fprintf(f, "%*sBasic(%d triples)\n", indent, "", pat->nbTriples);
        break;
    case PATTERN_TYPE_FILTER:
        fprintf(f, "%*sFilter(%d variables)\n", indent, "", pat->expr->nbVars);
        print_pattern(pat->left, f, indent+2);
        break;
    case PATTERN_TYPE_JOIN:
        fprintf(f, "%*sJoin\n", indent, "");
        print_pattern(pat->left, f, indent+2);
        print_pattern(pat->right, f, indent+2);
        break;
    case PATTERN_TYPE_LEFTJOIN:
        fprintf(f, "%*sLeftJoin", indent, "");
        if(pat->expr != NULL)
            fprintf(f, "(condition on %d variables)", pat->expr->nbVars);
        fprintf(f, "\n");
        print_pattern(pat->left, f, indent+2);
        print_pattern(pat->right, f, indent+2);
        break;
    case PATTERN_TYPE_MINUS:
        fprintf(f, "%*sMinus", indent, "");
        if(pat->expr != NULL)
            fprintf(f, "(condition on %d variables)", pat->expr->nbVars);
        fprintf(f, "\n");
        print_pattern(pat->left, f, indent+2);
        print_pattern(pat->right, f, indent+2);
        break;
    case PATTERN_TYPE_UNION:
        fprintf(f, "%*sUnion\n", indent, "");
        print_pattern(pat->left, f, indent+2);
        print_pattern(pat->right, f, indent+2);
        break;
    }
}

void query_print(Query* self, FILE* f) {
    // TODO print other info
    print_pattern(self->pattern, f, 0);
}
