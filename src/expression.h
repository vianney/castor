/* This file is part of Castor
 *
 * Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
 * Copyright (C) 2010-2013, Université catholique de Louvain
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
     * Post this expression as a (reified) constraint.
     *
     * @param sub subtree in which to add the constraint
     * @param b the truth value of the expression
     */
    virtual void post(cp::Subtree& sub, cp::TriStateVar* b);

    /**
     * @return whether this expression can be interpreted as an arithmetic
     *         constraint
     */
    virtual bool isArithmetic() { return false; }

    /**
     * Post this arithmetic expression.
     *
     * @pre isArithmetic()
     * @param sub subtree in which to add the constraints
     * @param n the numeric value of the expression
     * @param b guard (if RDF_ERROR: do not propagate)
     */
    virtual void postArithmetic(cp::Subtree& sub, cp::NumVar* n,
                                cp::TriStateVar* b) {}

    /**
     * Evaluate the expression given the current assignment in query. The result
     * is written in the result argument.
     *
     * @param[out] result value to fill in with the result
     * @return false on evaluation error, true otherwise
     */
    virtual bool evaluate(Value& result) = 0;

    /**
     * Evaluate the expression and compute its effective boolean value (EBV).
     *
     * @param buffer value buffer to be used during evaluation
     * @return the effective boolean value
     */
    TriState evaluateEBV(Value& buffer);

    /**
     * Evaluate the expression and compute its effective boolean value (EBV).
     *
     * @return the effective boolean value
     */
    TriState evaluateEBV() {
        Value buffer;
        return evaluateEBV(buffer);
    }

    /**
     * Evaluate the expression given the current assignment in query.
     * Returns the effective boolean value (EBV).
     *
     * @return true if the EBV is true, false if the EBV is false or an error
     *         occured
     */
    bool isTrue() { return evaluateEBV() == RDF_TRUE; }

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

    Expression* optimize() override {
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

    Expression* optimize() override {
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
    bool evaluate(Value& result) override;
    bool isArithmetic() override { return value_->isNumeric(); }
    void postArithmetic(cp::Subtree &sub, cp::NumVar *n,
                        cp::TriStateVar *b) override;

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
    bool evaluate(Value& result) override;
    bool isArithmetic() override { return true; }
    void postArithmetic(cp::Subtree &sub, cp::NumVar *n,
                        cp::TriStateVar *b) override;

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
    bool evaluate(Value& result) override;

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
    BangExpression(Expression* arg) :
        UnaryExpression(arg) {}
    bool evaluate(Value& result) override;
    void post(cp::Subtree& sub, cp::TriStateVar* b) override;
};

/**
 * + arg
 */
class UPlusExpression : public UnaryExpression {
public:
    UPlusExpression(Expression* arg) : UnaryExpression(arg) {}
    UPlusExpression(UnaryExpression&& o) : UnaryExpression(std::move(o)) {}
    bool evaluate(Value& result) override;
};

/**
 * - arg
 */
class UMinusExpression : public UnaryExpression {
public:
    UMinusExpression(Expression* arg) : UnaryExpression(arg) {}
    UMinusExpression(UnaryExpression&& o) : UnaryExpression(std::move(o)) {}
    bool evaluate(Value& result) override;
};

/**
 * ISIRI(arg)
 */
class IsIriExpression : public UnaryExpression {
public:
    IsIriExpression(Expression* arg) : UnaryExpression(arg) {}
    IsIriExpression(UnaryExpression&& o) : UnaryExpression(std::move(o)) {}
    bool evaluate(Value& result) override;
};

/**
 * ISBLANK(arg)
 */
class IsBlankExpression : public UnaryExpression {
public:
    IsBlankExpression(Expression* arg) : UnaryExpression(arg) {}
    IsBlankExpression(UnaryExpression&& o) : UnaryExpression(std::move(o)) {}
    bool evaluate(Value& result) override;
};

/**
 * ISLITERAL(arg)
 */
class IsLiteralExpression : public UnaryExpression {
public:
    IsLiteralExpression(Expression* arg) : UnaryExpression(arg) {}
    IsLiteralExpression(UnaryExpression&& o) : UnaryExpression(std::move(o)) {}
    bool evaluate(Value& result) override;
};

/**
 * STR(arg)
 */
class StrExpression : public UnaryExpression {
public:
    StrExpression(Expression* arg) : UnaryExpression(arg) {}
    StrExpression(UnaryExpression&& o) : UnaryExpression(std::move(o)) {}
    bool evaluate(Value& result) override;
};

/**
 * LANG(arg)
 */
class LangExpression : public UnaryExpression {
public:
    LangExpression(Expression* arg) : UnaryExpression(arg) {}
    LangExpression(UnaryExpression&& o) : UnaryExpression(std::move(o)) {}
    bool evaluate(Value& result) override;
};

/**
 * DATATYPE(arg)
 */
class DatatypeExpression : public UnaryExpression {
public:
    DatatypeExpression(Expression* arg) : UnaryExpression(arg) {}
    DatatypeExpression(UnaryExpression&& o) : UnaryExpression(std::move(o)) {}
    bool evaluate(Value& result) override;
};

/**
 * arg1 || arg2
 */
class OrExpression : public BinaryExpression {
public:
    OrExpression(Expression* arg1, Expression* arg2) :
        BinaryExpression(arg1, arg2) {}
    bool evaluate(Value& result) override;
    void post(cp::Subtree &sub, cp::TriStateVar *b) override;
};

/**
 * arg1 && arg2
 */
class AndExpression : public BinaryExpression {
public:
    AndExpression(Expression* arg1, Expression* arg2) :
        BinaryExpression(arg1, arg2) {}
    bool evaluate(Value& result) override;
    void post(cp::Subtree& sub, cp::TriStateVar* b) override;
};

/**
 * Base class for a comparison expression
 */
class EqualityExpression : public BinaryExpression {
public:
    EqualityExpression(Expression* arg1, Expression* arg2) :
        BinaryExpression(arg1, arg2) {}
    EqualityExpression(BinaryExpression&& o) : BinaryExpression(std::move(o)) {}
    void post(cp::Subtree& sub, cp::TriStateVar* b) override;

    /**
     * Post the constraint when both arguments are variables.
     */
    virtual void postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2,
                          cp::TriStateVar* b) = 0;
    /**
     * Post the constraint when one argument is a variable and the other is
     * constant.
     */
    virtual void postConst(cp::Subtree& sub, cp::RDFVar* x, Value& v,
                           cp::TriStateVar* b) = 0;
};

/**
 * arg1 = arg2
 */
class EqExpression : public EqualityExpression {
public:
    EqExpression(Expression* arg1, Expression* arg2) :
        EqualityExpression(arg1, arg2) {}
    EqExpression(BinaryExpression&& o) : EqualityExpression(std::move(o)) {}
    bool evaluate(Value& result) override;
    void postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2,
                  cp::TriStateVar* b) override;
    void postConst(cp::Subtree& sub, cp::RDFVar* x, Value& v,
                   cp::TriStateVar* b) override;
};

/**
 * arg1 != arg2
 */
class NEqExpression : public EqExpression {
public:
    NEqExpression(Expression* arg1, Expression* arg2) :
            EqExpression(arg1, arg2) {}
    NEqExpression(BinaryExpression&& o) : EqExpression(std::move(o)) {}
    bool evaluate(Value& result) override;
    void post(cp::Subtree &sub, cp::TriStateVar *b) override;
};

/**
 * Base class for an inequality expression
 */
class InequalityExpression : public BinaryExpression {
public:
    InequalityExpression(Expression* arg1, Expression* arg2) :
        BinaryExpression(arg1, arg2) {}
    InequalityExpression(BinaryExpression&& o) :
        BinaryExpression(std::move(o)) {}
    void post(cp::Subtree& sub, cp::TriStateVar* b) override;

    /**
     * Post the constraint when both arguments are variables.
     */
    virtual void postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2,
                          cp::TriStateVar* b) = 0;
    /**
     * Post the constraint when arg1 is a variable and arg2 is constant.
     */
    virtual void postConst(cp::Subtree& sub, cp::RDFVar* x1, Value& v2,
                           cp::TriStateVar* b) = 0;
    /**
     * Post the constraint when arg1 is constant and arg2 is a variable.
     */
    virtual void postConst(cp::Subtree& sub, Value& v1, cp::RDFVar* x2,
                           cp::TriStateVar* b) = 0;
    /**
     * Post the redundant arithmetic constraint.
     */
    virtual void postArithmetic(cp::Subtree& sub, cp::NumVar* x, cp::NumVar* y,
                                cp::TriStateVar *b) = 0;
};

/**
 * arg1 < arg2
 */
class LTExpression : public InequalityExpression {
public:
    LTExpression(Expression* arg1, Expression* arg2) :
            InequalityExpression(arg1, arg2) {}
    LTExpression(BinaryExpression&& o) : InequalityExpression(std::move(o)) {}
    bool evaluate(Value& result) override;
    void postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2,
                  cp::TriStateVar* b) override;
    void postConst(cp::Subtree& sub, cp::RDFVar* x1, Value& v2,
                   cp::TriStateVar* b) override;
    void postConst(cp::Subtree& sub, Value& v1, cp::RDFVar* x2,
                   cp::TriStateVar* b) override;
    void postArithmetic(cp::Subtree &sub, cp::NumVar *x, cp::NumVar *y,
                        cp::TriStateVar *b) override;
};

/**
 * arg1 > arg2
 */
class GTExpression : public InequalityExpression {
public:
    GTExpression(Expression* arg1, Expression* arg2) :
            InequalityExpression(arg1, arg2) {}
    GTExpression(BinaryExpression&& o) : InequalityExpression(std::move(o)) {}
    bool evaluate(Value& result) override;
    void postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2,
                  cp::TriStateVar* b) override;
    void postConst(cp::Subtree& sub, cp::RDFVar* x1, Value& v2,
                   cp::TriStateVar* b) override;
    void postConst(cp::Subtree& sub, Value& v1, cp::RDFVar* x2,
                   cp::TriStateVar* b) override;
    void postArithmetic(cp::Subtree &sub, cp::NumVar *x, cp::NumVar *y,
                        cp::TriStateVar *b) override;
};

/**
 * arg1 <= arg2
 */
class LEExpression : public InequalityExpression {
public:
    LEExpression(Expression* arg1, Expression* arg2) :
            InequalityExpression(arg1, arg2) {}
    LEExpression(BinaryExpression&& o) : InequalityExpression(std::move(o)) {}
    bool evaluate(Value& result) override;
    void postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2,
                  cp::TriStateVar* b) override;
    void postConst(cp::Subtree& sub, cp::RDFVar* x1, Value& v2,
                   cp::TriStateVar* b) override;
    void postConst(cp::Subtree& sub, Value& v1, cp::RDFVar* x2,
                   cp::TriStateVar* b) override;
    void postArithmetic(cp::Subtree &sub, cp::NumVar *x, cp::NumVar *y,
                        cp::TriStateVar *b) override;
};

/**
 * arg1 >= arg2
 */
class GEExpression : public InequalityExpression {
public:
    GEExpression(Expression* arg1, Expression* arg2) :
            InequalityExpression(arg1, arg2) {}
    GEExpression(BinaryExpression&& o) : InequalityExpression(std::move(o)) {}
    bool evaluate(Value& result) override;
    void postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2,
                  cp::TriStateVar* b) override;
    void postConst(cp::Subtree& sub, cp::RDFVar* x1, Value& v2,
                   cp::TriStateVar* b) override;
    void postConst(cp::Subtree& sub, Value& v1, cp::RDFVar* x2,
                   cp::TriStateVar* b) override;
    void postArithmetic(cp::Subtree &sub, cp::NumVar *x, cp::NumVar *y,
                        cp::TriStateVar *b) override;
};

/**
 * arg1 * arg2
 */
class StarExpression : public BinaryExpression {
public:
    StarExpression(Expression* arg1, Expression* arg2) :
            BinaryExpression(arg1, arg2) {}
    StarExpression(BinaryExpression&& o) : BinaryExpression(std::move(o)) {}
    bool evaluate(Value& result) override;
};

/**
 * arg1 / arg2
 */
class SlashExpression : public BinaryExpression {
public:
    SlashExpression(Expression* arg1, Expression* arg2) :
            BinaryExpression(arg1, arg2) {}
    SlashExpression(BinaryExpression&& o) : BinaryExpression(std::move(o)) {}
    bool evaluate(Value& result) override;
};

/**
 * arg1 + arg2
 */
class PlusExpression : public BinaryExpression {
public:
    PlusExpression(Expression* arg1, Expression* arg2) :
            BinaryExpression(arg1, arg2) {}
    PlusExpression(BinaryExpression&& o) : BinaryExpression(std::move(o)) {}
    bool evaluate(Value& result) override;
    bool isArithmetic() override {
        return arg1_->isArithmetic() && arg2_->isArithmetic();
    }
    void postArithmetic(cp::Subtree &sub, cp::NumVar *n,
                        cp::TriStateVar *b) override;
};

/**
 * arg1 - arg2
 */
class MinusExpression : public BinaryExpression {
public:
    MinusExpression(Expression* arg1, Expression* arg2) :
            BinaryExpression(arg1, arg2) {}
    MinusExpression(BinaryExpression&& o) : BinaryExpression(std::move(o)) {}
    bool evaluate(Value& result) override;
    bool isArithmetic() override {
        return arg1_->isArithmetic() && arg2_->isArithmetic();
    }
    void postArithmetic(cp::Subtree &sub, cp::NumVar *n,
                        cp::TriStateVar *b) override;
};

/**
 * SAMETERM(arg1, arg2)
 */
class SameTermExpression : public EqualityExpression {
public:
    SameTermExpression(Expression* arg1, Expression* arg2) :
        EqualityExpression(arg1, arg2) {}
    bool evaluate(Value& result) override;
    void postVars(cp::Subtree &sub, cp::RDFVar *x1, cp::RDFVar *x2,
                  cp::TriStateVar *b) override;
    void postConst(cp::Subtree &sub, cp::RDFVar *x1, Value &v2,
                   cp::TriStateVar *b) override;
};

/**
 * LANGMATCHES(arg1, arg2)
 */
class LangMatchesExpression : public BinaryExpression {
public:
    LangMatchesExpression(Expression* arg1, Expression* arg2) :
            BinaryExpression(arg1, arg2) {}
    LangMatchesExpression(BinaryExpression&& o) : BinaryExpression(std::move(o)) {}
    bool evaluate(Value& result) override;
};

/**
 * REGEX(arg1, arg2, arg3)
 */
class RegExExpression : public Expression {
public:
    RegExExpression(Expression* text, Expression* pattern, Expression* flags);
    ~RegExExpression();
    bool evaluate(Value& result) override;

    Expression* optimize() override {
        text_ = text_->optimize();
        pattern_ = pattern_->optimize();
        if(flags_ != nullptr)
            flags_ = flags_->optimize();
        return this;
    }

    /**
     * @return the text (first argument)
     */
    Expression* text() { return text_; }

    /**
     * @return the pattern (second argument)
     */
    Expression* pattern() { return pattern_; }

    /**
     * @return the flags (third argument), nullptr if unset
     */
    Expression* flags() { return flags_; }

private:
    Expression* text_; //!< first argument
    Expression* pattern_; //!< second argument
    Expression* flags_; //!< third argument
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
