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
#ifndef EXPRESSION_H
#define EXPRESSION_H

typedef struct TExpression Expression;

#include "defs.h"
#include "model.h"
#include "query.h"

/**
 * Operator enumeration
 */
typedef enum {
    EXPR_OP_VALUE,        // literal value
    EXPR_OP_VARIABLE,     // variable
    EXPR_OP_BANG,         // ! arg1
    EXPR_OP_UPLUS,        // + arg1
    EXPR_OP_UMINUS,       // - arg1
    EXPR_OP_BOUND,        // BOUND(arg1)
    EXPR_OP_ISIRI,        // ISIRI(arg1)
    EXPR_OP_ISBLANK,      // ISBLANK(arg1)
    EXPR_OP_ISLITERAL,    // ISLITERAL(arg1)
    EXPR_OP_STR,          // STR(arg1)
    EXPR_OP_LANG,         // LANG(arg1)
    EXPR_OP_DATATYPE,     // DATATYPE(arg1)
    EXPR_OP_OR,           // arg1 || arg2
    EXPR_OP_AND,          // arg1 && arg2
    EXPR_OP_EQ,           // arg1 = arg2
    EXPR_OP_NEQ,          // arg1 != arg2
    EXPR_OP_LT,           // arg1 < arg2
    EXPR_OP_GT,           // arg1 > arg2
    EXPR_OP_LE,           // arg1 <= arg2
    EXPR_OP_GE,           // arg1 >= arg2
    EXPR_OP_STAR,         // arg1 * arg2
    EXPR_OP_SLASH,        // arg1 / arg2
    EXPR_OP_PLUS,         // arg1 + arg2
    EXPR_OP_MINUS,        // arg1 - arg2
    EXPR_OP_SAMETERM,     // SAMETERM(arg1, arg2)
    EXPR_OP_LANGMATCHES,  // LANGMATCHES(arg1, arg2)
    EXPR_OP_REGEX,        // REGEX(arg1, arg2, arg3)
    EXPR_OP_CAST,         // cast
    EXPR_OP_CALL,         // function call

    EXPR_OP_FIRST_UNARY = EXPR_OP_BANG,
    EXPR_OP_FIRST_BINARY = EXPR_OP_OR,
    EXPR_OP_FIRST_TRINARY = EXPR_OP_REGEX,
    EXPR_OP_LAST_TRINARY = EXPR_OP_REGEX
} ExprOperator;

/**
 * Structure for a SPARQL expression
 */
struct TExpression {
    /**
     * Parent query
     */
    Query *query;
    /**
     * Operator
     */
    ExprOperator op;
    /**
     * Number of variables occuring in this expression
     */
    int nbVars;
    /**
     * Variables occuring in this expression
     */
    int *vars;
    /**
     * Map of variables occuring in this expression
     * varMap[x] <=> x in vars
     */
    bool *varMap;
    /**
     * Operands dependent on the operator
     */
    union {
        /**
         * op == EXPR_OP_VALUE
         */
        struct {
            Value *value;
            bool valueOwnership;
        };
        /**
         * op == EXPR_OP_VARIABLE
         * Id of the variable
         */
        int variable;
        /**
         * EXPR_OP_FIRST_UNARY <= op <= EXPR_OP_LAST_TRINARY
         */
        struct {
            Expression *arg1, *arg2, *arg3;
        };
        /**
         * op == EXPR_OP_CAST
         * Cast castArg into a destType
         */
        struct {
            ValueType destType;
            Expression *castArg;
        };
        /**
         * op == EXPR_OP_CALL
         * Call function fn with nbArgs arguments stored in array args.
         */
        struct {
            int fn;
            int nbArgs;
            Expression **args;
        };
    };
};

////////////////////////////////////////////////////////////////////////////////
// Constructors and destructors

/**
 * @param query parent query
 * @param value a value
 * @param valueOwnership take ownership of the value
 * @return a new expression representing the value
 */
Expression* new_expression_value(Query *query, Value* value, bool valueOwnership);

/**
 * @param query parent query
 * @param variable variable id
 * @return a new expression representing the variable
 */
Expression* new_expression_variable(Query *query, int variable);

/**
 * @param query parent query
 * @param op unary operator (EXPR_OP_FIRST_UNARY <= op < EXPR_OP_FIRST_BINARY)
 * @param arg1 first argument
 * @return a new expression
 */
Expression* new_expression_unary(Query *query, ExprOperator op, Expression* arg1);

/**
 * @param query parent query
 * @param op binary operator (EXPR_OP_FIRST_BINARY <= op < EXPR_OP_FIRST_TRINARY)
 * @param arg1 first argument
 * @param arg2 second argument
 * @return a new expression
 */
Expression* new_expression_binary(Query *query, ExprOperator op,
                                  Expression* arg1, Expression* arg2);

/**
 * @param query parent query
 * @param op trinary operator (EXPR_OP_FIRST_TRINARY <= op < EXPR_OP_LAST_TRINARY)
 * @param arg1 first argument
 * @param arg2 second argument
 * @param arg3 third argument
 * @return a new expression
 */
Expression* new_expression_trinary(Query *query, ExprOperator op,
                                   Expression* arg1, Expression* arg2,
                                   Expression* arg3);

/**
 * @param query parent query
 * @param destType type to which to cast
 * @param castArg expression to cast
 * @return a new expression
 */
Expression* new_expression_cast(Query *query, ValueType destType,
                                Expression* castArg);

/**
 * @param query parent query
 * @param fn function id
 * @param nbArgs number of arguments
 * @param args array of arguments, ownership is taken
 * @return a new expression
 */
Expression* new_expression_call(Query *query, int fn, int nbArgs,
                                Expression* args[]);

/**
 * Free an expression and recursively all its children expressions. Values for
 * which the ownership has been taken and args arrays will be freed too.
 */
void free_expression(Expression* expr);

////////////////////////////////////////////////////////////////////////////////
// Evaluation

/**
 * Evaluate an expression given the current assignment in query. The result is
 * written in the result argument. This may need to be cleaned. In case of
 * evaluation error, result may have changed, but nothing needs to be cleaned.
 *
 * @param expr the expression to evaluate
 * @param result value to fill in with the result
 * @return false on evaluation error, true otherwise
 */
bool expression_evaluate(Expression* expr, Value* result);

/**
 * Evaluate an expression given the current assignment in query. Returns the
 * effective boolean value (EBV).
 *
 * @param expr the expression to evaluate
 * @return true if the EBV is true, false if the EBV is false or an error
 *         occured
 */
bool expression_is_true(Expression* expr);

#endif // EXPRESSION_H
