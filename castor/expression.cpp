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

#include <cmath>
#include "expression.h"
#include "constraints.h"

namespace castor {

VarVal Expression::getVarVal() {
    Value val;
    if(evaluate(val))
        val.fillId(query->getStore());
    else
        val.id = 0;
    return val;
}

UnaryExpression::UnaryExpression(ExprOperator op, Expression *arg) :
        Expression(arg->getQuery(), op), arg(arg) {
    vars = arg->getVars();
}

UnaryExpression::~UnaryExpression() {
    delete arg;
}

BinaryExpression::BinaryExpression(ExprOperator op,
                                   Expression *arg1, Expression *arg2) :
        Expression(arg1->getQuery(), op), arg1(arg1), arg2(arg2) {
    vars = arg1->getVars();
    vars += arg2->getVars();
}

BinaryExpression::~BinaryExpression() {
    delete arg1;
    delete arg2;
}

ValueExpression::ValueExpression(Query *query, Value *value, bool ownership) :
        Expression(query, EXPR_OP_VALUE), value(value), ownership(ownership) {
}

ValueExpression::~ValueExpression() {
    if(ownership)
        delete value;
}

VariableExpression::VariableExpression(Variable *variable) :
        Expression(variable->getQuery(), EXPR_OP_VARIABLE), variable(variable) {
    vars += variable;
}

BoundExpression::BoundExpression(Variable *variable) :
        Expression(variable->getQuery(), EXPR_OP_BOUND), variable(variable) {
    vars += variable;
}

RegExExpression::RegExExpression(Expression *arg1, Expression *arg2,
                                 Expression *arg3) :
        Expression(arg1->getQuery(), EXPR_OP_REGEX),
        arg1(arg1), arg2(arg2), arg3(arg3) {
    vars = arg1->getVars();
    vars += arg2->getVars();
    vars += arg3->getVars();
}

RegExExpression::~RegExExpression() {
    delete arg1;
    delete arg2;
    delete arg3;
}

CastExpression::CastExpression(ValueType destination, Expression *arg) :
        Expression(arg->getQuery(), EXPR_OP_CAST),
        destination(destination), arg(arg) {
    vars = arg->getVars();
}

CastExpression::~CastExpression() {
    delete arg;
}

////////////////////////////////////////////////////////////////////////////////
// Evaluation functions

int Expression::evaluateEBV(Value &buffer) {
    if(!evaluate(buffer))
        return -1;
    if(buffer.isBoolean())
        return buffer.boolean ? 1 : 0;
    else if(buffer.isInteger())
        return buffer.integer ? 1 : 0;
    else if(buffer.isFloating())
        return isnan(buffer.floating) || buffer.floating == .0 ? 0 : 1;
    else if(buffer.isDecimal())
        return buffer.decimal->isZero() ? 0 : 1;
    else if(buffer.isPlain() || buffer.isXSDString())
        return buffer.lexical[0] == '\0' ? 0 : 1;
    else
        return -1;
}

bool ValueExpression::evaluate(Value &result) {
    result.fillCopy(value);
    return true;
}

bool VariableExpression::evaluate(Value &result) {
    Value *val = variable->getValue();
    if(!val)
        return false;
    result.fillCopy(val);
    return true;
}

bool BangExpression::evaluate(Value &result) {
    int ebv = arg->evaluateEBV(result);
    if(ebv == -1)
        return false;
    result.fillBoolean(!ebv);
    return true;
}

bool UPlusExpression::evaluate(Value &result) {
    if(!arg->evaluate(result) || !result.isNumeric())
        return false;
    return true;
}

bool UMinusExpression::evaluate(Value &result) {
    if(!arg->evaluate(result))
        return false;
    if(result.isInteger())
        result.fillInteger(-result.integer);
    else if(result.isDecimal())
        result.fillDecimal(result.decimal->negate());
    else if(result.isFloating())
        result.fillFloating(-result.floating);
    else
        return false;
    return true;
}

bool BoundExpression::evaluate(Value &result) {
    result.fillBoolean(variable->isBound());
    return true;
}

bool IsIRIExpression::evaluate(Value &result) {
    if(!arg->evaluate(result))
        return false;
    result.fillBoolean(result.isIRI());
    return true;
}

bool IsBlankExpression::evaluate(Value &result) {
    if(!arg->evaluate(result))
        return false;
    result.fillBoolean(result.isBlank());
    return true;
}

bool IsLiteralExpression::evaluate(Value &result) {
    if(!arg->evaluate(result))
        return false;
    result.fillBoolean(result.isLiteral());
    return true;
}

bool StrExpression::evaluate(Value &result) {
    if(!arg->evaluate(result) || result.isBlank())
        return false;
    result.ensureLexical();
    bool freeLex = result.hasCleanFlag(VALUE_CLEAN_LEXICAL);
    result.removeCleanFlag(VALUE_CLEAN_LEXICAL);
    result.fillSimpleLiteral(result.lexical, freeLex);
    return true;
}

bool LangExpression::evaluate(Value &result) {
    if(!arg->evaluate(result) || !result.isPlain())
        return false;
    char *lang = result.languageTag;
    if(lang == NULL)
        lang = (char*) "";
    bool freeLex = result.hasCleanFlag(VALUE_CLEAN_DATA);
    result.removeCleanFlag(VALUE_CLEAN_DATA);
    result.fillSimpleLiteral(lang, freeLex);
    return true;
}

bool DatatypeExpression::evaluate(Value &result) {
    if(!arg->evaluate(result) || !result.isLiteral())
        return false;
    if(result.isPlain()) {
        if(result.language != 0)
            return false;
        result.fillSimpleLiteral(VALUETYPE_URIS[VALUE_TYPE_PLAIN_STRING],
                                 false);
        return true;
    } else {
        bool freeLex = result.hasCleanFlag(VALUE_CLEAN_TYPE_URI);
        result.removeCleanFlag(VALUE_CLEAN_TYPE_URI);
        result.fillSimpleLiteral(result.typeUri, freeLex);
        return true;
    }
}

bool OrExpression::evaluate(Value &result) {
    int left = arg1->evaluateEBV(result);
    int right = arg2->evaluateEBV(result);
    if(left == 1 || right == 1)
        result.fillBoolean(true);
    else if(left == 0 && right == 0)
        result.fillBoolean(false);
    else
        return false;
    return true;
}

bool AndExpression::evaluate(Value &result) {
    int left = arg1->evaluateEBV(result);
    int right = arg2->evaluateEBV(result);
    if(left == 0 || right == 0)
        result.fillBoolean(false);
    else if(left == 1 && right == 1)
        result.fillBoolean(true);
    else
        return false;
    return true;
}

bool EqExpression::evaluate(Value &result) {
    Value right;
    if(!arg1->evaluate(result) || !arg2->evaluate(right))
        return false;
    int cmp = result.compare(right);
    if(cmp == -2) {
        cmp = result.rdfequals(right);
        if(cmp == -1)
            return false;
    }
    if(cmp == 0)
        result.fillBoolean(true);
    else
        result.fillBoolean(false);
    return true;
}

bool NEqExpression::evaluate(Value &result) {
    Value right;
    if(!arg1->evaluate(result) || !arg2->evaluate(right))
        return false;
    int cmp = result.compare(right);
    if(cmp == -2) {
        cmp = result.rdfequals(right);
        if(cmp == -1)
            return false;
    }
    if(cmp != 0)
        result.fillBoolean(true);
    else
        result.fillBoolean(false);
    return true;
}

bool LTExpression::evaluate(Value &result) {
    Value right;
    if(!arg1->evaluate(result) || !arg2->evaluate(right))
        return false;
    int cmp = result.compare(right);
    if(cmp == -2)
        return false;
    else if(cmp < 0)
        result.fillBoolean(true);
    else
        result.fillBoolean(false);
    return true;
}

bool GTExpression::evaluate(Value &result) {
    Value right;
    if(!arg1->evaluate(result) || !arg2->evaluate(right))
        return false;
    int cmp = result.compare(right);
    if(cmp == -2)
        return false;
    else if(cmp > 0)
        result.fillBoolean(true);
    else
        result.fillBoolean(false);
    return true;
}

bool LEExpression::evaluate(Value &result) {
    Value right;
    if(!arg1->evaluate(result) || !arg2->evaluate(right))
        return false;
    int cmp = result.compare(right);
    if(cmp == -2)
        return false;
    else if(cmp <= 0)
        result.fillBoolean(true);
    else
        result.fillBoolean(false);
    return true;
}

bool GEExpression::evaluate(Value &result) {
    Value right;
    if(!arg1->evaluate(result) || !arg2->evaluate(right))
        return false;
    int cmp = result.compare(right);
    if(cmp == -2)
        return false;
    else if(cmp >= 0)
        result.fillBoolean(true);
    else
        result.fillBoolean(false);
    return true;
}

bool StarExpression::evaluate(Value &result) {
    Value right;
    if(!arg1->evaluate(result) || !result.isNumeric() ||
       !arg2->evaluate(right) || !right.isNumeric())
        return false;
    promoteNumericType(result, right);
    if(right.isInteger())
        result.fillInteger(result.integer * right.integer);
    else if(right.isDecimal())
        result.fillDecimal(result.decimal->multiply(*right.decimal));
    else
        result.fillFloating(result.floating * right.floating);
    return true;
}

bool SlashExpression::evaluate(Value &result) {
    Value right;
    if(!arg1->evaluate(result) || !result.isNumeric() ||
       !arg2->evaluate(right) || !right.isNumeric())
        return false;
    promoteNumericType(result, right);
    if(right.isInteger()) {
        XSDDecimal d1(result.integer), d2(result.integer);
        result.fillDecimal(d1.divide(d2));
    } else if(right.isDecimal()) {
        result.fillDecimal(result.decimal->divide(*right.decimal));
    } else {
        result.fillFloating(result.floating / right.floating);
    }
    return true;
}

bool PlusExpression::evaluate(Value &result) {
    Value right;
    if(!arg1->evaluate(result) || !result.isNumeric() ||
       !arg2->evaluate(right) || !right.isNumeric())
        return false;
    promoteNumericType(result, right);
    if(right.isInteger())
        result.fillInteger(result.integer + right.integer);
    else if(right.isDecimal())
        result.fillDecimal(result.decimal->add(*right.decimal));
    else
        result.fillFloating(result.floating + right.floating);
    return true;
}

bool MinusExpression::evaluate(Value &result) {
    Value right;
    if(!arg1->evaluate(result) || !result.isNumeric() ||
       !arg2->evaluate(right) || !right.isNumeric())
        return false;
    promoteNumericType(result, right);
    if(right.isInteger())
        result.fillInteger(result.integer - right.integer);
    else if(right.isDecimal())
        result.fillDecimal(result.decimal->substract(*right.decimal));
    else
        result.fillFloating(result.floating - right.floating);
    return true;
}

bool SameTermExpression::evaluate(Value &result) {
    Value right;
    if(!arg1->evaluate(result) || !arg2->evaluate(right))
        return false;
    result.fillBoolean(result.rdfequals(right) == 0);
    return true;
}

bool LangMatchesExpression::evaluate(Value &result) {
    result.clean();
    throw "unsupported";
}

bool RegExExpression::evaluate(Value &result) {
    result.clean();
    throw "unsupported";
}

bool CastExpression::evaluate(Value &result) {
    result.clean();
    throw "unsupported";
}

////////////////////////////////////////////////////////////////////////////////
// Posting constraints

void Expression::post(Subtree &sub) {
    sub.add(new FilterConstraint(query->getStore(), this));
}

void AndExpression::post(Subtree &sub) {
    arg1->post(sub);
    arg2->post(sub);
}

void EqExpression::post(Subtree &sub) {
    if(arg1->isVarVal() && arg2->isVarVal())
        sub.add(new EqConstraint(query, arg1->getVarVal(), arg2->getVarVal()));
    else
        Expression::post(sub);
}

void NEqExpression::post(Subtree &sub) {
    if(arg1->isVarVal() && arg2->isVarVal())
        sub.add(new DiffConstraint(query,
                                   arg1->getVarVal(), arg2->getVarVal()));
    else
        Expression::post(sub);
}

}
