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
 * Base class for a SPARQL expression
 */
class Expression {
public:
    Expression(Query *query) : query(query), vars(query) {}
    virtual ~Expression() {}

    /**
     * @return parent query
     */
    Query* getQuery() { return query; }

    /**
     * @return variables occuring in this expression
     */
    VariableSet& getVars() { return vars; }

    /**
     * @return true if this is a constant expression
     */
    bool isConstant() { return vars.getSize() == 0; }

    /**
     * @return true if this expression is a single variable or a value from
     *         the store
     */
    virtual bool isVarVal() { return false; }

    /**
     * @pre isVarVal() == true
     * @return the VarVal corresponding to this variable or constant expression
     */
    virtual VarVal getVarVal() { return VarVal(static_cast<Value::id_t>(0)); }

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
    UnaryExpression(Expression *arg);
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
    BinaryExpression(Expression *arg1, Expression *arg2);
    ~BinaryExpression();
    void post(Subtree &sub);

    /**
     * @return left argument
     */
    Expression* getLeft() { return arg1; }

    /**
     * @return left argument
     */
    Expression* getRight() { return arg2; }

    // Handlers for default post implementation
    /**
     * Post the constraint when both arguments are variables.
     */
    virtual void postVars(Subtree &sub, VarInt *x1, VarInt *x2) { postOther(sub); }
    /**
     * Post the constraint when arg1 is a variable and arg2 is constant.
     */
    virtual void postConst(Subtree &sub, VarInt *x1, Value &v2) { postOther(sub); }
    /**
     * Post the constraint when arg1 is constant and arg2 is a variable.
     * The default implementation assumes the operator is reflexive.
     */
    virtual void postConst(Subtree &sub, Value &v1, VarInt *x2) { postConst(sub, x2, v1); }
    /**
     * Post the constraint for the other cases.
     */
    virtual void postOther(Subtree &sub) { Expression::post(sub); }
};

/**
 * Literal value. Takes ownership of the value.
 */
class ValueExpression : public Expression {
    Value *value; //!< the value
public:
    ValueExpression(Query *query, Value *value);
    ~ValueExpression();
    bool evaluate(Value &result);

    bool isVarVal() { return value->id != 0; }
    VarVal getVarVal() { return value; }

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

    bool isVarVal() { return true; }
    VarVal getVarVal() { return variable; }

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
    BangExpression(Expression *arg) : UnaryExpression(arg) {}
    bool evaluate(Value &result);
};

/**
 * + arg
 */
class UPlusExpression : public UnaryExpression {
public:
    UPlusExpression(Expression *arg) : UnaryExpression(arg) {}
    bool evaluate(Value &result);
};

/**
 * - arg
 */
class UMinusExpression : public UnaryExpression {
public:
    UMinusExpression(Expression *arg) : UnaryExpression(arg) {}
    bool evaluate(Value &result);
};

/**
 * ISIRI(arg)
 */
class IsIRIExpression : public UnaryExpression {
public:
    IsIRIExpression(Expression *arg) : UnaryExpression(arg) {}
    bool evaluate(Value &result);
};

/**
 * ISBLANK(arg)
 */
class IsBlankExpression : public UnaryExpression {
public:
    IsBlankExpression(Expression *arg) : UnaryExpression(arg) {}
    bool evaluate(Value &result);
};

/**
 * ISLITERAL(arg)
 */
class IsLiteralExpression : public UnaryExpression {
public:
    IsLiteralExpression(Expression *arg) : UnaryExpression(arg) {}
    bool evaluate(Value &result);
};

/**
 * STR(arg)
 */
class StrExpression : public UnaryExpression {
public:
    StrExpression(Expression *arg) : UnaryExpression(arg) {}
    bool evaluate(Value &result);
};

/**
 * LANG(arg)
 */
class LangExpression : public UnaryExpression {
public:
    LangExpression(Expression *arg) : UnaryExpression(arg) {}
    bool evaluate(Value &result);
};

/**
 * DATATYPE(arg)
 */
class DatatypeExpression : public UnaryExpression {
public:
    DatatypeExpression(Expression *arg) : UnaryExpression(arg) {}
    bool evaluate(Value &result);
};

/**
 * arg1 || arg2
 */
class OrExpression : public BinaryExpression {
public:
    OrExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(arg1, arg2) {}
    bool evaluate(Value &result);
};

/**
 * arg1 && arg2
 */
class AndExpression : public BinaryExpression {
public:
    AndExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(arg1, arg2) {}
    void post(Subtree &sub);
    bool evaluate(Value &result);
};

/**
 * arg1 = arg2
 */
class EqExpression : public BinaryExpression {
public:
    EqExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(arg1, arg2) {}
    bool evaluate(Value &result);
    void postVars(Subtree &sub, VarInt *x1, VarInt *x2);
    void postConst(Subtree &sub, VarInt *x, Value &v);
};

/**
 * arg1 != arg2
 */
class NEqExpression : public BinaryExpression {
public:
    NEqExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(arg1, arg2) {}
    bool evaluate(Value &result);
    void postVars(Subtree &sub, VarInt *x1, VarInt *x2);
    void postConst(Subtree &sub, VarInt *x, Value &v);
};

/**
 * arg1 < arg2
 */
class LTExpression : public BinaryExpression {
public:
    LTExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(arg1, arg2) {}
    bool evaluate(Value &result);
    void postVars(Subtree &sub, VarInt *x1, VarInt *x2);
    void postConst(Subtree &sub, VarInt *x1, Value &v2);
    void postConst(Subtree &sub, Value &v1, VarInt *x2);
};

/**
 * arg1 > arg2
 */
class GTExpression : public BinaryExpression {
public:
    GTExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(arg1, arg2) {}
    bool evaluate(Value &result);
    void postVars(Subtree &sub, VarInt *x1, VarInt *x2);
    void postConst(Subtree &sub, VarInt *x1, Value &v2);
    void postConst(Subtree &sub, Value &v1, VarInt *x2);
};

/**
 * arg1 <= arg2
 */
class LEExpression : public BinaryExpression {
public:
    LEExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(arg1, arg2) {}
    bool evaluate(Value &result);
    void postVars(Subtree &sub, VarInt *x1, VarInt *x2);
    void postConst(Subtree &sub, VarInt *x1, Value &v2);
    void postConst(Subtree &sub, Value &v1, VarInt *x2);
};

/**
 * arg1 >= arg2
 */
class GEExpression : public BinaryExpression {
public:
    GEExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(arg1, arg2) {}
    bool evaluate(Value &result);
    void postVars(Subtree &sub, VarInt *x1, VarInt *x2);
    void postConst(Subtree &sub, VarInt *x1, Value &v2);
    void postConst(Subtree &sub, Value &v1, VarInt *x2);
};

/**
 * arg1 * arg2
 */
class StarExpression : public BinaryExpression {
public:
    StarExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(arg1, arg2) {}
    bool evaluate(Value &result);
};

/**
 * arg1 / arg2
 */
class SlashExpression : public BinaryExpression {
public:
    SlashExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(arg1, arg2) {}
    bool evaluate(Value &result);
};

/**
 * arg1 + arg2
 */
class PlusExpression : public BinaryExpression {
public:
    PlusExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(arg1, arg2) {}
    bool evaluate(Value &result);
};

/**
 * arg1 - arg2
 */
class MinusExpression : public BinaryExpression {
public:
    MinusExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(arg1, arg2) {}
    bool evaluate(Value &result);
};

/**
 * SAMETERM(arg1, arg2)
 */
class SameTermExpression : public BinaryExpression {
public:
    SameTermExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(arg1, arg2) {}
    bool evaluate(Value &result);
};

/**
 * LANGMATCHES(arg1, arg2)
 */
class LangMatchesExpression : public BinaryExpression {
public:
    LangMatchesExpression(Expression *arg1, Expression *arg2) :
            BinaryExpression(arg1, arg2) {}
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
    Value::Type destination; //!< destination type
    Expression *arg; //!< argument to cast
public:
    CastExpression(Value::Type destination, Expression *arg);
    ~CastExpression();
    bool evaluate(Value &result);

    /**
     * @return destination type
     */
    Value::Type getDestination() { return destination; }

    /**
     * @return argument
     */
    Expression* getArgument() { return arg; }
};

}

#endif // CASTOR_EXPRESSION_H
