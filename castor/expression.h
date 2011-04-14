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
#ifndef CASTOR_EXPRESSION_H
#define CASTOR_EXPRESSION_H

namespace castor { class Expression; }

#include <vector>
#include "model.h"
#include "query.h"
#include "solver/subtree.h"

namespace castor {

/**
 * Operator enumeration
 */
enum ExprOperator {
    EXPR_OP_VALUE,        //!< literal value
    EXPR_OP_VARIABLE,     //!< variable
    EXPR_OP_BOUND,        //!< BOUND(var)
    EXPR_OP_BANG,         //!< ! arg1
    EXPR_OP_UPLUS,        //!< + arg1
    EXPR_OP_UMINUS,       //!< - arg1
    EXPR_OP_ISIRI,        //!< ISIRI(arg1)
    EXPR_OP_ISBLANK,      //!< ISBLANK(arg1)
    EXPR_OP_ISLITERAL,    //!< ISLITERAL(arg1)
    EXPR_OP_STR,          //!< STR(arg1)
    EXPR_OP_LANG,         //!< LANG(arg1)
    EXPR_OP_DATATYPE,     //!< DATATYPE(arg1)
    EXPR_OP_OR,           //!< arg1 || arg2
    EXPR_OP_AND,          //!< arg1 && arg2
    EXPR_OP_EQ,           //!< arg1 = arg2
    EXPR_OP_NEQ,          //!< arg1 != arg2
    EXPR_OP_LT,           //!< arg1 < arg2
    EXPR_OP_GT,           //!< arg1 > arg2
    EXPR_OP_LE,           //!< arg1 <= arg2
    EXPR_OP_GE,           //!< arg1 >= arg2
    EXPR_OP_STAR,         //!< arg1 * arg2
    EXPR_OP_SLASH,        //!< arg1 / arg2
    EXPR_OP_PLUS,         //!< arg1 + arg2
    EXPR_OP_MINUS,        //!< arg1 - arg2
    EXPR_OP_SAMETERM,     //!< SAMETERM(arg1, arg2)
    EXPR_OP_LANGMATCHES,  //!< LANGMATCHES(arg1, arg2)
    EXPR_OP_REGEX,        //!< REGEX(arg1, arg2, arg3)
    EXPR_OP_CAST          //!< cast
};

/**
 * Base class for a SPARQL expression
 */
class Expression {
public:
    Expression(Query *query, ExprOperator op) :
            query(query), op(op), vars(query) {}
    virtual ~Expression() {}

    /**
     * @return parent query
     */
    Query* getQuery() { return query; }

    /**
     * @return operator
     */
    ExprOperator getOp() { return op; }

    /**
     * @return variables occuring in this expression
     */
    VariableSet& getVars() { return vars; }

    /**
     * Post this expression as a constraint.
     * @param sub subtree in which to add the constraint
     */
    virtual void post(Subtree &sub);

    /**
     * Evaluate the expression given the current assignment in query. The result
     * is written in the result argument.
     *
     * @param[out] result value to fill in with the result
     * @return false on evaluation error, true otherwise
     */
    virtual bool evaluate(Value &result) = 0;

    /**
     * Evaluate the expression given the current assignment in query.
     * Returns the effective boolean value (EBV).
     *
     * @return true if the EBV is true, false if the EBV is false or an error
     *         occured
     */
    bool isTrue() {
        Value buffer;
        return evaluateEBV(buffer) == 1;
    }

    /**
     * Evaluate the expression and compute its effective boolean value (EBV).
     *
     * @param buffer value buffer to be used during evaluation
     * @return 1 if true, 0 if false, -1 if type error
     */
    int evaluateEBV(Value &buffer);

protected:
    /**
     * Parent query.
     */
    Query *query;
    /**
     * Operator.
     */
    ExprOperator op;
    /**
     * Variables occuring in this expression.
     */
    VariableSet vars;
};

/**
 * Base class for unary expressions
 */
class UnaryExpression : public Expression {
protected:
    Expression *arg; //!< argument
public:
    UnaryExpression(ExprOperator op, Expression *arg);
    ~UnaryExpression();

    /**
     * @return argument
     */
    Expression* getArgument() { return arg; }
};

/**
 * Base class for binary expressions
 */
class BinaryExpression : public Expression {
protected:
    Expression *arg1; //!< first argument
    Expression *arg2; //!< second argument
public:
    BinaryExpression(ExprOperator op, Expression *arg1, Expression *arg2);
    ~BinaryExpression();

    /**
     * @return left argument
     */
    Expression* getLeft() { return arg1; }

    /**
     * @return left argument
     */
    Expression* getRight() { return arg2; }
};

/**
 * Literal value
 */
class ValueExpression : public Expression {
    Value *value; //!< the value
    bool ownership; //!< whether to take over ownership of the value
public:
    ValueExpression(Query *query, Value *value, bool ownership);
    ~ValueExpression();
    bool evaluate(Value &result);

    /**
     * @return the value
     */
    Value* getValue() { return value; }
};

/**
 * Variable
 */
class VariableExpression : public Expression {
    Variable *variable; //!< the variable
public:
    VariableExpression(Variable *variable);
    bool evaluate(Value &result);

    /**
     * @return the variable
     */
    Variable* getVariable() { return variable; }
};

/**
 * BOUND(arg)
 */
class BoundExpression : public Expression {
    Variable *variable; //!< the variable
public:
    BoundExpression(Variable *variable);
    bool evaluate(Value &result);

    /**
     * @return the variable
     */
    Variable* getVariable() { return variable; }
};

/**
 * ! arg
 */
class BangExpression : public UnaryExpression {
public:
    BangExpression(Expression *arg) : UnaryExpression(EXPR_OP_BANG, arg) {}
    bool evaluate(Value &result);
};

/**
 * + arg
 */
class UPlusExpression : public UnaryExpression {
public:
    UPlusExpression(Expression *arg) : UnaryExpression(EXPR_OP_UPLUS, arg) {}
    bool evaluate(Value &result);
};

/**
 * - arg
 */
class UMinusExpression : public UnaryExpression {
public:
    UMinusExpression(Expression *arg) : UnaryExpression(EXPR_OP_UMINUS, arg) {}
    bool evaluate(Value &result);
};

/**
 * ISIRI(arg)
 */
class IsIRIExpression : public UnaryExpression {
public:
    IsIRIExpression(Expression *arg) : UnaryExpression(EXPR_OP_ISIRI, arg) {}
    bool evaluate(Value &result);
};

/**
 * ISBLANK(arg)
 */
class IsBlankExpression : public UnaryExpression {
public:
    IsBlankExpression(Expression *arg) : UnaryExpression(EXPR_OP_ISBLANK, arg) {}
    bool evaluate(Value &result);
};

/**
 * ISLITERAL(arg)
 */
class IsLiteralExpression : public UnaryExpression {
public:
    IsLiteralExpression(Expression *arg) : UnaryExpression(EXPR_OP_ISLITERAL, arg) {}
    bool evaluate(Value &result);
};

/**
 * STR(arg)
 */
class StrExpression : public UnaryExpression {
public:
    StrExpression(Expression *arg) : UnaryExpression(EXPR_OP_STR, arg) {}
    bool evaluate(Value &result);
};

/**
 * LANG(arg)
 */
class LangExpression : public UnaryExpression {
public:
    LangExpression(Expression *arg) : UnaryExpression(EXPR_OP_LANG, arg) {}
    bool evaluate(Value &result);
};

/**
 * DATATYPE(arg)
 */
class DatatypeExpression : public UnaryExpression {
public:
    DatatypeExpression(Expression *arg) : UnaryExpression(EXPR_OP_DATATYPE, arg) {}
    bool evaluate(Value &result);
};

/**
 * arg1 || arg2
 */
class OrExpression : public BinaryExpression {
public:
    OrExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(EXPR_OP_OR, arg1, arg2) {}
    bool evaluate(Value &result);
};

/**
 * arg1 && arg2
 */
class AndExpression : public BinaryExpression {
public:
    AndExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(EXPR_OP_AND, arg1, arg2) {}
    bool evaluate(Value &result);
};

/**
 * arg1 = arg2
 */
class EqExpression : public BinaryExpression {
public:
    EqExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(EXPR_OP_EQ, arg1, arg2) {}
    bool evaluate(Value &result);
};

/**
 * arg1 != arg2
 */
class NEqExpression : public BinaryExpression {
public:
    NEqExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(EXPR_OP_NEQ, arg1, arg2) {}
    bool evaluate(Value &result);
};

/**
 * arg1 < arg2
 */
class LTExpression : public BinaryExpression {
public:
    LTExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(EXPR_OP_LT, arg1, arg2) {}
    bool evaluate(Value &result);
};

/**
 * arg1 > arg2
 */
class GTExpression : public BinaryExpression {
public:
    GTExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(EXPR_OP_GT, arg1, arg2) {}
    bool evaluate(Value &result);
};

/**
 * arg1 <= arg2
 */
class LEExpression : public BinaryExpression {
public:
    LEExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(EXPR_OP_LE, arg1, arg2) {}
    bool evaluate(Value &result);
};

/**
 * arg1 >= arg2
 */
class GEExpression : public BinaryExpression {
public:
    GEExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(EXPR_OP_GE, arg1, arg2) {}
    bool evaluate(Value &result);
};

/**
 * arg1 * arg2
 */
class StarExpression : public BinaryExpression {
public:
    StarExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(EXPR_OP_STAR, arg1, arg2) {}
    bool evaluate(Value &result);
};

/**
 * arg1 / arg2
 */
class SlashExpression : public BinaryExpression {
public:
    SlashExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(EXPR_OP_SLASH, arg1, arg2) {}
    bool evaluate(Value &result);
};

/**
 * arg1 + arg2
 */
class PlusExpression : public BinaryExpression {
public:
    PlusExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(EXPR_OP_PLUS, arg1, arg2) {}
    bool evaluate(Value &result);
};

/**
 * arg1 - arg2
 */
class MinusExpression : public BinaryExpression {
public:
    MinusExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(EXPR_OP_MINUS, arg1, arg2) {}
    bool evaluate(Value &result);
};

/**
 * SAMETERM(arg1, arg2)
 */
class SameTermExpression : public BinaryExpression {
public:
    SameTermExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(EXPR_OP_SAMETERM, arg1, arg2) {}
    bool evaluate(Value &result);
};

/**
 * LANGMATCHES(arg1, arg2)
 */
class LangMatchesExpression : public BinaryExpression {
public:
    LangMatchesExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(EXPR_OP_LANGMATCHES, arg1, arg2) {}
    bool evaluate(Value &result);
};

/**
 * REGEX(arg1, arg2, arg3)
 */
class RegExExpression : public Expression {
    Expression *arg1; //!< first argument
    Expression *arg2; //!< second argument
    Expression *arg3; //!< third argument
public:
    RegExExpression(Expression *arg1, Expression *arg2, Expression* arg3);
    ~RegExExpression();
    bool evaluate(Value &result);

    /**
     * @return the first argument
     */
    Expression* getArg1() { return arg1; }

    /**
     * @return the first argument
     */
    Expression* getArg2() { return arg2; }

    /**
     * @return the first argument
     */
    Expression* getArg3() { return arg3; }
};

/**
 * Cast expression
 */
class CastExpression : public Expression {
    ValueType destination; //!< destination type
    Expression *arg; //!< argument to cast
public:
    CastExpression(ValueType destination, Expression *arg);
    ~CastExpression();
    bool evaluate(Value &result);

    /**
     * @return destination type
     */
    ValueType getDestination() { return destination; }

    /**
     * @return argument
     */
    Expression* getArgument() { return arg; }
};

}

#endif // CASTOR_EXPRESSION_H
