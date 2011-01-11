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
#include <math.h>
#include <string.h>

#include "expression.h"

////////////////////////////////////////////////////////////////////////////////
// Constructors and destructors

Expression* new_expression_value(Value* value, bool valueOwnership) {
    Expression* expr;

    expr = (Expression*) malloc(sizeof(Expression));
    expr->op = EXPR_OP_VALUE;
    expr->value = value;
    expr->valueOwnership = valueOwnership;
    return expr;
}

Expression* new_expression_variable(int variable) {
    Expression* expr;

    expr = (Expression*) malloc(sizeof(Expression));
    expr->op = EXPR_OP_VARIABLE;
    expr->variable = variable;
    return expr;
}

Expression* new_expression_unary(ExprOperator op, Expression* arg1) {
    Expression* expr;

    expr = (Expression*) malloc(sizeof(Expression));
    expr->op = op;
    expr->arg1 = arg1;
    expr->arg2 = NULL;
    expr->arg3 = NULL;
    return expr;
}

Expression* new_expression_binary(ExprOperator op, Expression* arg1,
                                  Expression* arg2) {
    Expression* expr;

    expr = (Expression*) malloc(sizeof(Expression));
    expr->op = op;
    expr->arg1 = arg1;
    expr->arg2 = arg2;
    expr->arg3 = NULL;
    return expr;
}

Expression* new_expression_trinary(ExprOperator op, Expression* arg1,
                                   Expression* arg2, Expression* arg3) {
    Expression* expr;

    expr = (Expression*) malloc(sizeof(Expression));
    expr->op = op;
    expr->arg1 = arg1;
    expr->arg2 = arg2;
    expr->arg3 = arg3;
    return expr;
}

Expression* new_expression_cast(ValueType destType, Expression* castArg) {
    Expression* expr;

    expr = (Expression*) malloc(sizeof(Expression));
    expr->op = EXPR_OP_CAST;
    expr->destType = destType;
    expr->castArg = castArg;
    return expr;
}

Expression* new_expression_call(int fn, int nbArgs, Expression* args[]) {
    Expression* expr;

    expr = (Expression*) malloc(sizeof(Expression));
    expr->op = EXPR_OP_CALL;
    expr->fn = fn;
    expr->nbArgs = nbArgs;
    expr->args = args;
    return expr;
}

void free_expression(Expression* expr) {
    int i;

    switch(expr->op) {
    case EXPR_OP_VALUE:
        if(expr->valueOwnership) {
            model_value_clean(expr->value);
            free(expr->value);
        }
        break;
    case EXPR_OP_VARIABLE:
        break;
    case EXPR_OP_CAST:
        free_expression(expr->castArg);
        break;
    case EXPR_OP_CALL:
        for(i = 0; i < expr->nbArgs; i++)
            free_expression(expr->args[i]);
        free(expr->args);
        break;
    default:
        if(expr->arg1 != NULL) free_expression(expr->arg1);
        if(expr->arg2 != NULL) free_expression(expr->arg2);
        if(expr->arg3 != NULL) free_expression(expr->arg3);
    }
    free(expr);
}

////////////////////////////////////////////////////////////////////////////////
// Manipulation

/**
 * Recursive function to gather all unique variables of an expression.
 *
 * @param expr the expression
 * @param vars output array or NULL if not needed
 * @param varIncluded is a variable already included in vars
 * @return the number of unique variables in expr that were not already
 *         included (varIncluded was false) and that have been added to vars.
 *         varIncluded is updated accordingly.
 */
int visit_get_variables(Expression* expr, int* vars, bool* varIncluded) {
    int i, count;

    count = 0;
    switch(expr->op) {
    case EXPR_OP_VALUE:
        return 0;
    case EXPR_OP_VARIABLE:
        if(varIncluded[expr->variable])
            return 0;
        if(vars != NULL)
            vars[0] = expr->variable;
        varIncluded[expr->variable] = true;
        return 1;
    case EXPR_OP_CAST:
        return visit_get_variables(expr->castArg, vars, varIncluded);
    case EXPR_OP_CALL:
        for(i = 0; i < expr->nbArgs; i++)
            count += visit_get_variables(expr->args[i], vars, varIncluded);
        break;
    default:
        if(expr->arg1 != NULL)
            count += visit_get_variables(expr->arg1, vars, varIncluded);
        if(expr->arg2 != NULL)
            count += visit_get_variables(expr->arg2, vars, varIncluded);
        if(expr->arg3 != NULL)
            count += visit_get_variables(expr->arg3, vars, varIncluded);
    }
    return count;
}

int expression_get_variables(Query* query, Expression* expr, int* vars) {
    int result;
    int* varIncluded;

    varIncluded = (int*) calloc(query_variable_count(query), sizeof(int));
    result = visit_get_variables(expr, vars, varIncluded);
    free(varIncluded);
    return result;
}

////////////////////////////////////////////////////////////////////////////////
// Evaluation

/**
 * Evaluate an expression and compute its effective boolean value (EBV).
 *
 * @param query a query instance
 * @param expr the expression to evaluate
 * @param val value buffer to be used during evaluation, should not be cleaned
 * @return 1 if true, 0 if false, -1 if type error
 */
int eval_ebv(Query* query, Expression* expr, Value* val) {
    int result;

    if(!expression_evaluate(query, expr, val))
        return -1;
    if(val->type == VALUE_TYPE_BOOLEAN)
        result = val->boolean ? 1 : 0;
    else if (val->type >= VALUE_TYPE_FIRST_INTEGER &&
             val->type <= VALUE_TYPE_LAST_INTEGER)
        result = val->integer ? 1 : 0;
    else if(val->type >= VALUE_TYPE_FIRST_FLOATING &&
            val->type <= VALUE_TYPE_LAST_FLOATING)
        result = isnan(val->floating) || val->floating == .0 ? 0 : 1;
    else if(val->type == VALUE_TYPE_PLAIN_STRING ||
            val->type == VALUE_TYPE_TYPED_STRING)
        result = val->lexical[0] == '\0' ? 0 : 1;
    else
        result = -1;

    model_value_clean(val);
    return result;
}

/**
 * Make a xsd:boolean
 *
 * @param value the value
 * @param result value to create
 */
void eval_make_boolean(bool value, Value* result) {
    result->id = -1;
    result->type = VALUE_TYPE_BOOLEAN;
    result->typeUri = VALUETYPE_URIS[VALUE_TYPE_BOOLEAN];
    result->language = 0;
    result->languageTag = NULL;
    result->lexical = NULL;
    result->cleanup = VALUE_CLEAN_NOTHING;
    result->boolean = value;
}

/**
 * Make a xsd:integer
 *
 * @param value the value
 * @param result value to create
 */
void eval_make_integer(long value, Value* result) {
    result->id = -1;
    result->type = VALUE_TYPE_INTEGER;
    result->typeUri = VALUETYPE_URIS[VALUE_TYPE_INTEGER];
    result->language = 0;
    result->languageTag = NULL;
    result->lexical = NULL;
    result->cleanup = VALUE_CLEAN_NOTHING;
    result->integer = value;
}

/**
 * Make a xsd:double
 *
 * @param value the value
 * @param result value to create
 */
void eval_make_double(double value, Value* result) {
    result->id = -1;
    result->type = VALUE_TYPE_DOUBLE;
    result->typeUri = VALUETYPE_URIS[VALUE_TYPE_DOUBLE];
    result->language = 0;
    result->languageTag = NULL;
    result->lexical = NULL;
    result->cleanup = VALUE_CLEAN_NOTHING;
    result->floating = value;
}

/**
 * Make a simple literal
 *
 * @param lexical the lexical form
 * @param freeLexical should the lexical form be freed by the caller?
 * @param result value to create
 */
void eval_make_simple_literal(char* lexical, bool freeLexical, Value* result) {
    result->id = -1;
    result->type = VALUE_TYPE_PLAIN_STRING;
    result->typeUri = NULL;
    result->language = 0;
    result->languageTag = NULL;
    result->lexical = lexical;
    result->cleanup = freeLexical ? VALUE_CLEAN_LEXICAL : VALUE_CLEAN_NOTHING;
}

/**
 * Make an IRI
 *
 * @param lexical the IRI
 * @param freeLexical should the lexical form be freed by the caller?
 * @param result value to create
 */
void eval_make_iri(char* lexical, bool freeLexical, Value* result) {
    result->id = -1;
    result->type = VALUE_TYPE_IRI;
    result->typeUri = NULL;
    result->language = 0;
    result->languageTag = NULL;
    result->lexical = lexical;
    result->cleanup = freeLexical ? VALUE_CLEAN_LEXICAL : VALUE_CLEAN_NOTHING;
}

typedef bool (*eval_fn)(Query* query, Expression* expr, Value* result);

bool eval_value(Query* UNUSED(query), Expression* expr, Value* result) {
    memcpy(result, expr->value, sizeof(Value));
    result->cleanup = VALUE_CLEAN_NOTHING;
    return true;
}

bool eval_variable(Query* query, Expression* expr, Value* result) {
    Value *val;

    val = query_variable_get(query, expr->variable);
    if(val == NULL)
        return false;
    memcpy(result, val, sizeof(Value));
    result->cleanup = VALUE_CLEAN_NOTHING;
    return true;
}

bool eval_bang(Query* query, Expression* expr, Value* result) {
    int b;

    b = eval_ebv(query, expr->arg1, result);
    if(b == -1)
        return false;
    eval_make_boolean(b, result);
    return true;
}

bool eval_uplus(Query* query, Expression* expr, Value* result) {
    if(!expression_evaluate(query, expr->arg1, result))
        return false;
    if(result->type < VALUE_TYPE_FIRST_NUMERIC ||
       result->type > VALUE_TYPE_LAST_NUMERIC) {
        model_value_clean(result);
        return false;
    }
    return true;
}

bool eval_uminus(Query* query, Expression* expr, Value* result) {
    long i;
    double d;

    if(!expression_evaluate(query, expr->arg1, result))
        return false;

    if(result->type >= VALUE_TYPE_FIRST_INTEGER &&
       result->type <= VALUE_TYPE_LAST_INTEGER) {
        i = -result->integer;
        model_value_clean(result);
        eval_make_integer(i, result);
        result->integer = -result->integer;
    } else if(result->type >= VALUE_TYPE_FIRST_FLOATING &&
            result->type <= VALUE_TYPE_LAST_FLOATING) {
        d = -result->floating;
        model_value_clean(result);
        eval_make_double(d, result);
    } else {
        model_value_clean(result);
        return false;
    }
    return true;
}

bool eval_bound(Query* query, Expression* expr, Value* result) {
    if(expr->arg1->op != EXPR_OP_VARIABLE)
        return false;
    eval_make_boolean(query_variable_get(query, expr->arg1->variable) != NULL,
                      result);
    return true;
}

bool eval_isiri(Query* query, Expression* expr, Value* result) {
    bool b;

    if(!expression_evaluate(query, expr->arg1, result))
        return false;
    b = (result->type == VALUE_TYPE_IRI);
    model_value_clean(result);
    eval_make_boolean(b, result);
    return true;
}

bool eval_isblank(Query* query, Expression* expr, Value* result) {
    bool b;

    if(!expression_evaluate(query, expr->arg1, result))
        return false;
    b = (result->type == VALUE_TYPE_BLANK);
    model_value_clean(result);
    eval_make_boolean(b, result);
    return true;
}

bool eval_isliteral(Query* query, Expression* expr, Value* result) {
    bool b;

    if(!expression_evaluate(query, expr->arg1, result))
        return false;
    b = (result->type != VALUE_TYPE_BLANK && result->type != VALUE_TYPE_IRI);
    model_value_clean(result);
    eval_make_boolean(b, result);
    return true;
}

bool eval_str(Query* query, Expression* expr, Value* result) {
    char *lex;
    bool freeLex;

    if(!expression_evaluate(query, expr->arg1, result))
        return false;
    if(result->type == VALUE_TYPE_BLANK) {
        model_value_clean(result);
        return false;
    }
    lex = result->lexical;
    freeLex = result->cleanup & VALUE_CLEAN_LEXICAL,
    result->cleanup &= ~VALUE_CLEAN_LEXICAL;
    model_value_clean(result);
    eval_make_simple_literal(lex, freeLex, result);
    return true;
}

bool eval_lang(Query* query, Expression* expr, Value* result) {
    char *lang;
    bool freeLang;

    if(!expression_evaluate(query, expr->arg1, result))
        return false;
    if(result->type == VALUE_TYPE_BLANK || result->type == VALUE_TYPE_IRI) {
        model_value_clean(result);
        return false;
    }
    lang = result->languageTag;
    freeLang = result->cleanup & VALUE_CLEAN_LANGUAGE_TAG;
    result->cleanup &= ~VALUE_CLEAN_LANGUAGE_TAG;
    model_value_clean(result);
    eval_make_simple_literal(lang, freeLang, result);
    return true;
}

bool eval_datatype(Query* query, Expression* expr, Value* result) {
    char* iri;
    bool freeIRI;

    if(!expression_evaluate(query, expr->arg1, result))
        return false;
    if(result->type == VALUE_TYPE_BLANK || result->type == VALUE_TYPE_IRI) {
        model_value_clean(result);
        return false;
    }
    if(result->type == VALUE_TYPE_PLAIN_STRING) {
        if(result->language == 0) {
            iri = VALUETYPE_URIS[VALUE_TYPE_TYPED_STRING];
            freeIRI = false;
        } else {
            model_value_clean(result);
            return false;
        }
    } else {
        iri = result->typeUri;
        freeIRI = result->cleanup & VALUE_CLEAN_TYPE_URI;
        result->cleanup &= ~VALUE_CLEAN_TYPE_URI;
    }
    model_value_clean(result);
    eval_make_iri(iri, freeIRI, result);
    return true;
}

bool eval_or(Query* query, Expression* expr, Value* result) {
    int left, right;
    left = eval_ebv(query, expr->arg1, result);
    right = eval_ebv(query, expr->arg2, result);
    if(left == 1 || right == 1)
        eval_make_boolean(true, result);
    else if(left == 0 && right == 0)
        eval_make_boolean(false, result);
    else
        return false;
    return true;
}

bool eval_and(Query* query, Expression* expr, Value* result) {
    int left, right;
    left = eval_ebv(query, expr->arg1, result);
    right = eval_ebv(query, expr->arg2, result);
    if(left == 0 || right == 0)
        eval_make_boolean(false, result);
    else if(left == 1 && right == 1)
        eval_make_boolean(true, result);
    else
        return false;
    return true;
}

#define EVAL_EQ(fn, op) \
    bool eval_ ## fn (Query* query, Expression* expr, Value* result) { \
        Value right; \
        int cmp; \
         \
        if(!expression_evaluate(query, expr->arg1, result)) \
            return false; \
        if(!expression_evaluate(query, expr->arg2, &right)) { \
            model_value_clean(result); \
            return false; \
        } \
         \
        cmp = model_value_compare(result, &right); \
        if(cmp == -2) \
            cmp = model_value_equal(result, &right) - 1; \
        model_value_clean(result); \
        model_value_clean(&right); \
        if(cmp == -2) \
            return false; \
        else if(cmp op 0) \
            eval_make_boolean(true, result); \
        else \
            eval_make_boolean(false, result); \
        return true; \
    }

EVAL_EQ(eq, ==)
EVAL_EQ(neq, !=)
#undef EVAL_EQ

#define EVAL_CMP(fn, op) \
    bool eval_ ## fn (Query* query, Expression* expr, Value* result) { \
        Value right; \
        int cmp; \
         \
        if(!expression_evaluate(query, expr->arg1, result)) \
            return false; \
        if(!expression_evaluate(query, expr->arg2, &right)) { \
            model_value_clean(result); \
            return false; \
        } \
         \
        cmp = model_value_compare(result, &right); \
        model_value_clean(result); \
        model_value_clean(&right); \
        if(cmp == -2) \
            return false; \
        else if(cmp op 0) \
            eval_make_boolean(true, result); \
        else \
            eval_make_boolean(false, result); \
        return true; \
    }

EVAL_CMP(lt, <)
EVAL_CMP(gt, >)
EVAL_CMP(le, <=)
EVAL_CMP(ge, >=)
#undef EVAL_CMP

#define EVAL_ARITHMETIC(fn, op) \
    bool eval_ ## fn (Query* query, Expression* expr, Value* result) { \
        Value right; \
        long i1, i2; \
        double d1, d2; \
         \
        if(!expression_evaluate(query, expr->arg1, result)) \
            return false; \
        if(!expression_evaluate(query, expr->arg2, &right)) { \
            model_value_clean(result); \
            return false; \
        } \
        if(!IS_VALUE_TYPE_NUMERIC(result->type) || \
           !IS_VALUE_TYPE_NUMERIC(right.type)) { \
            model_value_clean(result); \
            model_value_clean(&right); \
            return false; \
        } \
         \
        if(IS_VALUE_TYPE_INTEGER(result->type) && \
           IS_VALUE_TYPE_INTEGER(right.type)) { \
            i1 = result->integer; \
            i2 = right.integer; \
            model_value_clean(result); \
            model_value_clean(&right); \
            eval_make_integer(i1 op i2, result); \
        } else { \
            d1 = IS_VALUE_TYPE_FLOATING(result->type) ? \
                 result->floating : (double) result->integer; \
            d2 = IS_VALUE_TYPE_FLOATING(right.type) ? \
                 right.floating : (double) right.integer; \
            model_value_clean(result); \
            model_value_clean(&right); \
            eval_make_double(d1 op d2, result); \
        } \
        return true; \
    }

EVAL_ARITHMETIC(star, *)
EVAL_ARITHMETIC(plus, +)
EVAL_ARITHMETIC(minus, -)
#undef EVAL_ARITHMETIC

bool eval_slash(Query* query, Expression* expr, Value* result) {
    Value right;
    long i1, i2;
    double d1, d2;

    if(!expression_evaluate(query, expr->arg1, result))
        return false;
    if(!expression_evaluate(query, expr->arg2, &right)) {
        model_value_clean(result);
        return false;
    }
    if(!IS_VALUE_TYPE_NUMERIC(result->type) ||
       !IS_VALUE_TYPE_NUMERIC(right.type)) {
        model_value_clean(result);
        model_value_clean(&right);
        return false;
    }

    if(IS_VALUE_TYPE_INTEGER(result->type) &&
       IS_VALUE_TYPE_INTEGER(right.type)) {
        i1 = result->integer;
        i2 = right.integer;
        model_value_clean(result);
        model_value_clean(&right);
        if(i2 == 0)
            return false;
        eval_make_double((double) i1 / (double) i2, result); // FIXME should be decimal
    } else {
        d1 = IS_VALUE_TYPE_FLOATING(result->type) ?
             result->floating : (double) result->integer;
        d2 = IS_VALUE_TYPE_FLOATING(right.type) ?
             right.floating : (double) right.integer;
        model_value_clean(result);
        model_value_clean(&right);
        eval_make_double(d1 / d2, result);
    }
    return true;
}

bool eval_sameterm(Query* query, Expression* expr, Value* result) {
    Value right;
    bool b;

    if(!expression_evaluate(query, expr->arg1, result))
        return false;
    if(!expression_evaluate(query, expr->arg2, &right)) {
        model_value_clean(result);
        return false;
    }

    b = model_value_equal(result, &right) == 1;
    model_value_clean(result);
    model_value_clean(&right);
    eval_make_boolean(b, result);
    return true;
}

bool eval_langmatches(Query* UNUSED(query), Expression* UNUSED(expr),
                      Value* UNUSED(result)) {
    // TODO
    fprintf(stderr, "castor expression: Unimplemented operator LANGMATCHES\n");
    return false;
}

bool eval_regex(Query* UNUSED(query), Expression* UNUSED(expr),
                Value* UNUSED(result)) {
    // TODO
    fprintf(stderr, "castor expression: Unimplemented operator REGEX\n");
    return false;
}

bool eval_cast(Query* UNUSED(query), Expression* UNUSED(expr),
               Value* UNUSED(result)) {
    // TODO
    fprintf(stderr, "castor expression: Casting operators unimplemented\n");
    return false;
}

bool eval_call(Query* UNUSED(query), Expression* UNUSED(expr),
               Value* UNUSED(result)) {
    // TODO
    fprintf(stderr, "castor expression: Function support unimplemented\n");
    return false;
}

eval_fn EVAL_FUNCTIONS[] = {
    eval_value,
    eval_variable,
    eval_bang,
    eval_uplus,
    eval_uminus,
    eval_bound,
    eval_isiri,
    eval_isblank,
    eval_isliteral,
    eval_str,
    eval_lang,
    eval_datatype,
    eval_or,
    eval_and,
    eval_eq,
    eval_neq,
    eval_lt,
    eval_gt,
    eval_le,
    eval_ge,
    eval_star,
    eval_slash,
    eval_plus,
    eval_minus,
    eval_sameterm,
    eval_langmatches,
    eval_regex,
    eval_cast,
    eval_call
};

bool expression_evaluate(Query* query, Expression* expr, Value* result) {
    return EVAL_FUNCTIONS[expr->op](query, expr, result);
}

bool expression_is_true(Query* query, Expression* expr) {
    Value val;

    return eval_ebv(query, expr, &val) == 1;
}
