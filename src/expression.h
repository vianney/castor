/* This file is part of Castor
 *
 * Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
 * Copyright (C) 2010-2012, Université catholique de Louvain
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef CASTOR_EXPRESSION_H
#define CASTOR_EXPRESSION_H

#include <vector>

#include "model.h"
#include "variable.h"
#include "solver/subtree.h"

namespace castor {

class Query;

/**
 * Base class for a SPARQL expression
 */
class Expression {
public:
    Expression(Query* query) : query_(query), vars_(query) {}
    virtual ~Expression() {}

    //! Non-copyable
    Expression(const Expression&) = delete;
    Expression& operator=(const Expression&) = delete;

    /**
     * @return parent query
     */
    Query* query() { return query_; }

    /**
     * @return variables occuring in this expression
     */
    const VariableSet& variables() { return vars_; }

    /**
     * @return true if this is a constant expression
     */
    bool isConstant() { return vars_.size() == 0; }

    /**
     * Optimize this expression and its subexpressions. Beware: the expressions
     * may change. You shouldn't use this object anymore, but instead use the
     * returned pointer (which may or may not be the same as this).
     *
     * @return the optimized expression
     */
    virtual Expression* optimize() { return this; }

    /**
     * Post this expression as a constraint.
     * @param sub subtree in which to add the constraint
     */
    virtual void post(cp::Subtree& sub);

    /**
     * Evaluate the expression given the current assignment in query. The result
     * is written in the result argument.
     *
     * @param[out] result value to fill in with the result
     * @return false on evaluation error, true otherwise
     */
    virtual bool evaluate(Value& result) = 0;

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
    int evaluateEBV(Value& buffer);

protected:
    /**
     * Parent query.
     */
    Query* query_;
    /**
     * Variables occuring in this expression.
     */
    VariableSet vars_;
};

/**
 * Base class for unary expressions
 */
class UnaryExpression : public Expression {
public:
    UnaryExpression(Expression* arg);
    //! Move constructor
    UnaryExpression(UnaryExpression&& o);
    ~UnaryExpression();

    Expression* optimize() {
        arg_ = arg_->optimize();
        return this;
    }

    /**
     * @return argument
     */
    Expression* argument() { return arg_; }

protected:
    Expression* arg_; //!< argument
};

/**
 * Base class for binary expressions
 */
class BinaryExpression : public Expression {
public:
    BinaryExpression(Expression* arg1, Expression* arg2);
    BinaryExpression(BinaryExpression&& o);
    ~BinaryExpression();

    Expression* optimize() {
        arg1_ = arg1_->optimize();
        arg2_ = arg2_->optimize();
        return this;
    }

    /**
     * @return left argument
     */
    Expression* left() { return arg1_; }

    /**
     * @return left argument
     */
    Expression* right() { return arg2_; }

protected:
    Expression* arg1_; //!< first argument
    Expression* arg2_; //!< second argument
};

/**
 * Literal value. Takes ownership of the value.
 */
class ValueExpression : public Expression {
public:
    ValueExpression(Query* query, Value* value);
    ~ValueExpression();
    bool evaluate(Value& result);

    /**
     * @return the value
     */
    Value* value() { return value_; }

private:
    Value* value_; //!< the value
};

/**
 * Variable
 */
class VariableExpression : public Expression {
public:
    VariableExpression(Variable* variable);
    bool evaluate(Value& result);

    /**
     * @return the variable
     */
    Variable* variable() { return variable_; }

private:
    Variable* variable_; //!< the variable
};

/**
 * BOUND(arg)
 */
class BoundExpression : public Expression {
public:
    BoundExpression(Variable* variable);
    bool evaluate(Value& result);

    /**
     * @return the variable
     */
    Variable* variable() { return variable_; }

private:
    Variable* variable_; //!< the variable
};

/**
 * ! arg
 */
class BangExpression : public UnaryExpression {
public:
    BangExpression(Expression* arg) : UnaryExpression(arg) {}
    BangExpression(UnaryExpression&& o) : UnaryExpression(std::move(o)) {}
    bool evaluate(Value& result);
    Expression* optimize();
};

/**
 * + arg
 */
class UPlusExpression : public UnaryExpression {
public:
    UPlusExpression(Expression* arg) : UnaryExpression(arg) {}
    UPlusExpression(UnaryExpression&& o) : UnaryExpression(std::move(o)) {}
    bool evaluate(Value& result);
};

/**
 * - arg
 */
class UMinusExpression : public UnaryExpression {
public:
    UMinusExpression(Expression* arg) : UnaryExpression(arg) {}
    UMinusExpression(UnaryExpression&& o) : UnaryExpression(std::move(o)) {}
    bool evaluate(Value& result);
};

/**
 * ISIRI(arg)
 */
class IsIriExpression : public UnaryExpression {
public:
    IsIriExpression(Expression* arg) : UnaryExpression(arg) {}
    IsIriExpression(UnaryExpression&& o) : UnaryExpression(std::move(o)) {}
    bool evaluate(Value& result);
};

/**
 * ISBLANK(arg)
 */
class IsBlankExpression : public UnaryExpression {
public:
    IsBlankExpression(Expression* arg) : UnaryExpression(arg) {}
    IsBlankExpression(UnaryExpression&& o) : UnaryExpression(std::move(o)) {}
    bool evaluate(Value& result);
};

/**
 * ISLITERAL(arg)
 */
class IsLiteralExpression : public UnaryExpression {
public:
    IsLiteralExpression(Expression* arg) : UnaryExpression(arg) {}
    IsLiteralExpression(UnaryExpression&& o) : UnaryExpression(std::move(o)) {}
    bool evaluate(Value& result);
};

/**
 * STR(arg)
 */
class StrExpression : public UnaryExpression {
public:
    StrExpression(Expression* arg) : UnaryExpression(arg) {}
    StrExpression(UnaryExpression&& o) : UnaryExpression(std::move(o)) {}
    bool evaluate(Value& result);
};

/**
 * LANG(arg)
 */
class LangExpression : public UnaryExpression {
public:
    LangExpression(Expression* arg) : UnaryExpression(arg) {}
    LangExpression(UnaryExpression&& o) : UnaryExpression(std::move(o)) {}
    bool evaluate(Value& result);
};

/**
 * DATATYPE(arg)
 */
class DatatypeExpression : public UnaryExpression {
public:
    DatatypeExpression(Expression* arg) : UnaryExpression(arg) {}
    DatatypeExpression(UnaryExpression&& o) : UnaryExpression(std::move(o)) {}
    bool evaluate(Value& result);
};

/**
 * arg1 || arg2
 */
class OrExpression : public BinaryExpression {
public:
    OrExpression(Expression* arg1, Expression* arg2) :
            BinaryExpression(arg1, arg2) {}
    OrExpression(BinaryExpression&& o) : BinaryExpression(std::move(o)) {}
    bool evaluate(Value& result);
};

/**
 * arg1 && arg2
 */
class AndExpression : public BinaryExpression {
public:
    AndExpression(Expression* arg1, Expression* arg2) :
            BinaryExpression(arg1, arg2) {}
    AndExpression(BinaryExpression&& o) : BinaryExpression(std::move(o)) {}
    void post(cp::Subtree& sub);
    bool evaluate(Value& result);
};

/**
 * Base class for a equality expression
 */
class EqualityExpression : public BinaryExpression {
public:
    EqualityExpression(Expression* arg1, Expression* arg2) :
            BinaryExpression(arg1, arg2) {}
    EqualityExpression(BinaryExpression&& o) : BinaryExpression(std::move(o)) {}
    void post(cp::Subtree& sub);

    /**
     * Post the constraint when both arguments are variables.
     */
    virtual void postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2) = 0;
    /**
     * Post the constraint when one argument is a variable and the other is
     * constant.
     */
    virtual void postConst(cp::Subtree& sub, cp::RDFVar* x1, Value& v2) = 0;
};

/**
 * arg1 = arg2
 */
class EqExpression : public EqualityExpression {
public:
    EqExpression(Expression* arg1, Expression* arg2) :
            EqualityExpression(arg1, arg2) {}
    EqExpression(BinaryExpression&& o) : EqualityExpression(std::move(o)) {}
    bool evaluate(Value& result);
    void postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2);
    void postConst(cp::Subtree& sub, cp::RDFVar* x, Value& v);
};

/**
 * arg1 != arg2
 */
class NEqExpression : public EqualityExpression {
public:
    NEqExpression(Expression* arg1, Expression* arg2) :
            EqualityExpression(arg1, arg2) {}
    NEqExpression(BinaryExpression&& o) : EqualityExpression(std::move(o)) {}
    bool evaluate(Value& result);
    void postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2);
    void postConst(cp::Subtree& sub, cp::RDFVar* x, Value& v);
};

/**
 * Base class for a inequality expression
 */
class InequalityExpression : public BinaryExpression {
public:
    InequalityExpression(Expression* arg1, Expression* arg2)
        : BinaryExpression(arg1, arg2) {}
    InequalityExpression(BinaryExpression&& o) : BinaryExpression(std::move(o)) {}
    void post(cp::Subtree& sub);

    // Handlers for default post implementation
    /**
     * Post the constraint when both arguments are variables.
     */
    virtual void postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2) = 0;
    /**
     * Post the constraint when arg1 is a variable and arg2 is constant.
     */
    virtual void postConst(cp::Subtree& sub, cp::RDFVar* x1, Value& v2) = 0;
    /**
     * Post the constraint when arg1 is constant and arg2 is a variable.
     */
    virtual void postConst(cp::Subtree& sub, Value& v1, cp::RDFVar* x2) = 0;
};

/**
 * arg1 < arg2
 */
class LTExpression : public InequalityExpression {
public:
    LTExpression(Expression* arg1, Expression* arg2) :
            InequalityExpression(arg1, arg2) {}
    LTExpression(BinaryExpression&& o) : InequalityExpression(std::move(o)) {}
    bool evaluate(Value& result);
    void postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2);
    void postConst(cp::Subtree& sub, cp::RDFVar* x1, Value& v2);
    void postConst(cp::Subtree& sub, Value& v1, cp::RDFVar* x2);
};

/**
 * arg1 > arg2
 */
class GTExpression : public InequalityExpression {
public:
    GTExpression(Expression* arg1, Expression* arg2) :
            InequalityExpression(arg1, arg2) {}
    GTExpression(BinaryExpression&& o) : InequalityExpression(std::move(o)) {}
    bool evaluate(Value& result);
    void postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2);
    void postConst(cp::Subtree& sub, cp::RDFVar* x1, Value& v2);
    void postConst(cp::Subtree& sub, Value& v1, cp::RDFVar* x2);
};

/**
 * arg1 <= arg2
 */
class LEExpression : public InequalityExpression {
public:
    LEExpression(Expression* arg1, Expression* arg2) :
            InequalityExpression(arg1, arg2) {}
    LEExpression(BinaryExpression&& o) : InequalityExpression(std::move(o)) {}
    bool evaluate(Value& result);
    void postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2);
    void postConst(cp::Subtree& sub, cp::RDFVar* x1, Value& v2);
    void postConst(cp::Subtree& sub, Value& v1, cp::RDFVar* x2);
};

/**
 * arg1 >= arg2
 */
class GEExpression : public InequalityExpression {
public:
    GEExpression(Expression* arg1, Expression* arg2) :
            InequalityExpression(arg1, arg2) {}
    GEExpression(BinaryExpression&& o) : InequalityExpression(std::move(o)) {}
    bool evaluate(Value& result);
    void postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2);
    void postConst(cp::Subtree& sub, cp::RDFVar* x1, Value& v2);
    void postConst(cp::Subtree& sub, Value& v1, cp::RDFVar* x2);
};

/**
 * arg1 * arg2
 */
class StarExpression : public BinaryExpression {
public:
    StarExpression(Expression* arg1, Expression* arg2) :
            BinaryExpression(arg1, arg2) {}
    StarExpression(BinaryExpression&& o) : BinaryExpression(std::move(o)) {}
    bool evaluate(Value& result);
};

/**
 * arg1 / arg2
 */
class SlashExpression : public BinaryExpression {
public:
    SlashExpression(Expression* arg1, Expression* arg2) :
            BinaryExpression(arg1, arg2) {}
    SlashExpression(BinaryExpression&& o) : BinaryExpression(std::move(o)) {}
    bool evaluate(Value& result);
};

/**
 * arg1 + arg2
 */
class PlusExpression : public BinaryExpression {
public:
    PlusExpression(Expression* arg1, Expression* arg2) :
            BinaryExpression(arg1, arg2) {}
    PlusExpression(BinaryExpression&& o) : BinaryExpression(std::move(o)) {}
    bool evaluate(Value& result);
};

/**
 * arg1 - arg2
 */
class MinusExpression : public BinaryExpression {
public:
    MinusExpression(Expression* arg1, Expression* arg2) :
            BinaryExpression(arg1, arg2) {}
    MinusExpression(BinaryExpression&& o) : BinaryExpression(std::move(o)) {}
    bool evaluate(Value& result);
};

/**
 * SAMETERM(arg1, arg2)
 */
class SameTermExpression : public EqualityExpression {
public:
    SameTermExpression(Expression* arg1, Expression* arg2) :
            EqualityExpression(arg1, arg2) {}
    SameTermExpression(BinaryExpression&& o) : EqualityExpression(std::move(o)) {}
    bool evaluate(Value& result);
    void postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2);
    void postConst(cp::Subtree& sub, cp::RDFVar* x, Value& v);
};

/**
 * !(SAMETERM(arg1, arg2))
 */
class DiffTermExpression : public EqualityExpression {
public:
    DiffTermExpression(Expression* arg1, Expression* arg2) :
            EqualityExpression(arg1, arg2) {}
    DiffTermExpression(BinaryExpression&& o) : EqualityExpression(std::move(o)) {}
    bool evaluate(Value& result);
    void postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2);
    void postConst(cp::Subtree& sub, cp::RDFVar* x, Value& v);
};

/**
 * LANGMATCHES(arg1, arg2)
 */
class LangMatchesExpression : public BinaryExpression {
public:
    LangMatchesExpression(Expression* arg1, Expression* arg2) :
            BinaryExpression(arg1, arg2) {}
    LangMatchesExpression(BinaryExpression&& o) : BinaryExpression(std::move(o)) {}
    bool evaluate(Value& result);
};

/**
 * REGEX(arg1, arg2, arg3)
 */
class RegExExpression : public Expression {
public:
    RegExExpression(Expression* arg1, Expression* arg2, Expression* arg3);
    ~RegExExpression();
    bool evaluate(Value& result);

    Expression* optimize() {
        arg1_ = arg1_->optimize();
        arg2_ = arg2_->optimize();
        arg3_ = arg3_->optimize();
        return this;
    }

    /**
     * @return the first argument
     */
    Expression* arg1() { return arg1_; }

    /**
     * @return the first argument
     */
    Expression* arg2() { return arg2_; }

    /**
     * @return the first argument
     */
    Expression* arg3() { return arg3_; }

private:
    Expression* arg1_; //!< first argument
    Expression* arg2_; //!< second argument
    Expression* arg3_; //!< third argument
};

///**
// * Cast expression
// */
//class CastExpression : public Expression {
//public:
//    CastExpression(Value::Type target, Expression* arg);
//    ~CastExpression();
//    bool evaluate(Value& result);

//    Expression* optimize() {
//        arg_ = arg_->optimize();
//        return this;
//    }

//    /**
//     * @return destination type
//     */
//    Value::Type target() { return target_; }

//    /**
//     * @return argument
//     */
//    Expression* argument() { return arg_; }

//private:
//    Value::Type target_; //!< target type
//    Expression* arg_; //!< argument to cast
//};

}

#endif // CASTOR_EXPRESSION_H
