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

UnaryExpression::UnaryExpression(Expression *arg) :
        Expression(arg->getQuery()), arg(arg) {
    vars = arg->getVars();
}
UnaryExpression::~UnaryExpression() {
    if(arg != NULL)
        delete arg;
}
void UnaryExpression::deleteThisOnly() {
    arg = NULL;
    delete this;
}

BinaryExpression::BinaryExpression(Expression *arg1, Expression *arg2) :
        Expression(arg1->getQuery()), arg1(arg1), arg2(arg2) {
    vars = arg1->getVars();
    vars += arg2->getVars();
}
BinaryExpression::~BinaryExpression() {
    if(arg1 != NULL)
        delete arg1;
    if(arg2 != NULL)
        delete arg2;
}
void BinaryExpression::deleteThisOnly() {
    arg1 = NULL;
    arg2 = NULL;
    delete this;
}

ValueExpression::ValueExpression(Query *query, Value *value) :
        Expression(query), value(value) {
    value->ensureInterpreted();
}
ValueExpression::~ValueExpression() {
    delete value;
}

VariableExpression::VariableExpression(Variable *variable) :
        Expression(variable->getQuery()), variable(variable) {
    vars += variable;
}
BoundExpression::BoundExpression(Variable *variable) :
        Expression(variable->getQuery()), variable(variable) {
    vars += variable;
}

RegExExpression::RegExExpression(Expression *arg1, Expression *arg2,
                                 Expression *arg3) :
        Expression(arg1->getQuery()), arg1(arg1), arg2(arg2), arg3(arg3) {
    vars = arg1->getVars();
    vars += arg2->getVars();
    vars += arg3->getVars();
}
RegExExpression::~RegExExpression() {
    if(arg1 != NULL)
        delete arg1;
    if(arg2 != NULL)
        delete arg2;
    if(arg3 != NULL)
        delete arg3;
}
void RegExExpression::deleteThisOnly() {
    arg1 = NULL;
    arg2 = NULL;
    arg3 = NULL;
    delete this;
}

CastExpression::CastExpression(Value::Type destination, Expression *arg) :
        Expression(arg->getQuery()), destination(destination), arg(arg) {
    vars = arg->getVars();
}
CastExpression::~CastExpression() {
    if(arg != NULL)
        delete arg;
}
void CastExpression::deleteThisOnly() {
    arg = NULL;
    delete this;
}

////////////////////////////////////////////////////////////////////////////////
// Optimzing expressions

Expression* BangExpression::optimize() {
    UnaryExpression::optimize();
    if(SameTermExpression *e = dynamic_cast<SameTermExpression*>(arg)) {
        Expression *result = new DiffTermExpression(e->getLeft(), e->getRight());
        arg->deleteThisOnly();
        arg = NULL;
        delete this;
        return result;
    }
    return this;
}


////////////////////////////////////////////////////////////////////////////////
// Evaluation functions

int Expression::evaluateEBV(Value &buffer) {
    if(!evaluate(buffer))
        return -1;
    buffer.ensureInterpreted();
    if(buffer.isBoolean())
        return buffer.boolean ? 1 : 0;
    else if(buffer.isInteger())
        return buffer.integer ? 1 : 0;
    else if(buffer.isFloating())
        return isnan(buffer.floating) || buffer.floating == .0 ? 0 : 1;
    else if(buffer.isDecimal())
        return buffer.decimal->isZero() ? 0 : 1;
    else if(buffer.isPlain() || buffer.isXSDString())
        return buffer.lexicalLen == 0 ? 0 : 1;
    else
        return -1;
}

bool ValueExpression::evaluate(Value &result) {
    result.fillCopy(*value);
    return true;
}

bool VariableExpression::evaluate(Value &result) {
    Value::id_t valid = variable->getValueId();
    if(valid == 0)
        return false;
    query->getStore()->fetchValue(valid, result);
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
    result.ensureInterpreted();
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
    bool freeLex = result.hasCleanFlag(Value::CLEAN_LEXICAL);
    result.removeCleanFlag(Value::CLEAN_LEXICAL);
    result.fillSimpleLiteral(result.lexical, result.lexicalLen, freeLex);
    return true;
}

bool LangExpression::evaluate(Value &result) {
    if(!arg->evaluate(result) || !result.isPlain())
        return false;
    char *lang = result.language;
    if(lang == NULL)
        lang = const_cast<char*>("");
    bool freeLex = result.hasCleanFlag(Value::CLEAN_DATA);
    result.removeCleanFlag(Value::CLEAN_DATA);
    result.fillSimpleLiteral(lang, result.languageLen, freeLex);
    return true;
}

bool DatatypeExpression::evaluate(Value &result) {
    if(!arg->evaluate(result) || !result.isLiteral())
        return false;
    if(result.isPlain()) {
        if(result.language != 0)
            return false;
        result.fillSimpleLiteral(Value::TYPE_URIS[Value::TYPE_PLAIN_STRING],
                                 sizeof(Value::TYPE_URIS[Value::TYPE_PLAIN_STRING]) - 1,
                                 false);
        return true;
    } else {
        bool freeLex = result.hasCleanFlag(Value::CLEAN_TYPE_URI);
        result.removeCleanFlag(Value::CLEAN_TYPE_URI);
        result.fillSimpleLiteral(result.typeUri, result.typeUriLen, freeLex);
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
    result.ensureInterpreted();
    right.ensureInterpreted();
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
    result.ensureInterpreted();
    right.ensureInterpreted();
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
    result.ensureInterpreted();
    right.ensureInterpreted();
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
    result.ensureInterpreted();
    right.ensureInterpreted();
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
    result.ensureInterpreted();
    right.ensureInterpreted();
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
    result.ensureInterpreted();
    right.ensureInterpreted();
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
    Value::promoteNumericType(result, right);
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
    Value::promoteNumericType(result, right);
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
    Value::promoteNumericType(result, right);
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
    Value::promoteNumericType(result, right);
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
    result.ensureLexical();
    right.ensureLexical();
    result.fillBoolean(result.rdfequals(right) == 0);
    return true;
}

bool DiffTermExpression::evaluate(Value &result) {
    Value right;
    if(!arg1->evaluate(result) || !arg2->evaluate(right))
        return false;
    result.ensureLexical();
    right.ensureLexical();
    result.fillBoolean(result.rdfequals(right) != 0);
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

void EqualityExpression::post(Subtree &sub) {
    VariableExpression *var1 = dynamic_cast<VariableExpression*>(arg1);
    VariableExpression *var2 = dynamic_cast<VariableExpression*>(arg2);
    if(var1 && var2) {
        VarInt *x1 = var1->getVariable()->getCPVariable();
        VarInt *x2 = var2->getVariable()->getCPVariable();
        postVars(sub, x1, x2);
    } else if(var1 && arg2->isConstant()) {
        Value val;
        if(arg2->evaluate(val)) {
            VarInt *x = var1->getVariable()->getCPVariable();
            query->getStore()->lookupId(val);
            postConst(sub, x, val);
        } else {
            sub.add(new FalseConstraint());
        }
    } else if(var2 && arg1->isConstant()) {
        Value val;
        if(arg1->evaluate(val)) {
            VarInt *x = var2->getVariable()->getCPVariable();
            query->getStore()->lookupId(val);
            postConst(sub, x, val);
        } else {
            sub.add(new FalseConstraint());
        }
    } else {
        Expression::post(sub);
    }
}
void EqExpression::postVars(Subtree &sub, VarInt *x1, VarInt *x2) {
    sub.add(new VarEqConstraint(query->getStore(), x1, x2));
}
void EqExpression::postConst(Subtree &sub, VarInt *x, Value &v) {
    sub.add(new InRangeConstraint(x, query->getStore()->getValueEqClass(v)));
}
void NEqExpression::postVars(Subtree &sub, VarInt *x1, VarInt *x2) {
    /* In class CUSTOM, either two values are equal (and thus return false) or
     * the comparison produces a type error (making the constraint false).
     */
    sub.add(new NotInRangeConstraint(x1,
                    query->getStore()->getClassValues(Value::CLASS_OTHER)));
    sub.add(new NotInRangeConstraint(x2,
                    query->getStore()->getClassValues(Value::CLASS_OTHER)));
    sub.add(new VarDiffConstraint(query->getStore(), x1, x2));
}
void NEqExpression::postConst(Subtree &sub, VarInt *x, Value &v) {
    if(v.isLiteral() && v.type != Value::TYPE_CUSTOM) {
        ValueRange ranges[2] =
            { query->getStore()->getClassValues(Value::CLASS_BLANK,
                                                Value::CLASS_IRI),
              query->getStore()->getClassValues(v.getClass()) };
        sub.add(new InRangesConstraint(x, ranges, 2));
    } else {
        sub.add(new InRangeConstraint(x,
                        query->getStore()->getClassValues(Value::CLASS_BLANK,
                                                          Value::CLASS_IRI)));
    }
    sub.add(new NotInRangeConstraint(x, query->getStore()->getValueEqClass(v)));
}
void SameTermExpression::postVars(Subtree &sub, VarInt *x1, VarInt *x2) {
    sub.add(new VarSameTermConstraint(x1, x2));
}
void SameTermExpression::postConst(Subtree &sub, VarInt *x, Value &v) {
    if(v.id == 0) {
        sub.add(new FalseConstraint());
    } else {
        ValueRange rng = {v.id, v.id};
        sub.add(new InRangeConstraint(x, rng));
    }
}
void DiffTermExpression::postVars(Subtree &sub, VarInt *x1, VarInt *x2) {
    sub.add(new VarDiffTermConstraint(x1, x2));
}
void DiffTermExpression::postConst(Subtree &sub, VarInt *x, Value &v) {
    if(v.id != 0) {
        ValueRange rng = {v.id, v.id};
        sub.add(new NotInRangeConstraint(x, rng));
    }
}

void InequalityExpression::post(Subtree &sub) {
    VariableExpression *var1 = dynamic_cast<VariableExpression*>(arg1);
    VariableExpression *var2 = dynamic_cast<VariableExpression*>(arg2);
    if(var1 && var2) {
        VarInt *x1 = var1->getVariable()->getCPVariable();
        VarInt *x2 = var2->getVariable()->getCPVariable();
        sub.add(new ComparableConstraint(query->getStore(), x1));
        sub.add(new ComparableConstraint(query->getStore(), x2));
        sub.add(new SameClassConstraint(query->getStore(), x1, x2));
        postVars(sub, x1, x2);
    } else if(var1 && arg2->isConstant()) {
        Value val;
        if(arg2->evaluate(val) && val.isComparable()) {
            VarInt *x = var1->getVariable()->getCPVariable();
            query->getStore()->lookupId(val);
            sub.add(new InRangeConstraint(x,
                        query->getStore()->getClassValues(val.getClass())));
            postConst(sub, x, val);
        } else {
            sub.add(new FalseConstraint());
        }
    } else if(var2 && arg1->isConstant()) {
        Value val;
        if(arg1->evaluate(val) && val.isComparable()) {
            VarInt *x = var2->getVariable()->getCPVariable();
            query->getStore()->lookupId(val);
            sub.add(new InRangeConstraint(x,
                        query->getStore()->getClassValues(val.getClass())));
            postConst(sub, val, x);
        } else {
            sub.add(new FalseConstraint());
        }
    } else {
        Expression::post(sub);
    }
}
void LTExpression::postVars(Subtree &sub, VarInt *x1, VarInt *x2) {
    sub.add(new VarLessConstraint(query->getStore(), x1, x2, false));
}
void LTExpression::postConst(Subtree &sub, VarInt *x1, Value &v2) {
    sub.add(new ConstLEConstraint(x1,
                        query->getStore()->getValueEqClass(v2).from - 1));
}
void LTExpression::postConst(Subtree &sub, Value &v1, VarInt *x2) {
    sub.add(new ConstGEConstraint(x2,
                        query->getStore()->getValueEqClass(v1).to + 1));
}


void GTExpression::postVars(Subtree &sub, VarInt *x1, VarInt *x2) {
    sub.add(new VarLessConstraint(query->getStore(), x2, x1, false));
}
void GTExpression::postConst(Subtree &sub, VarInt *x1, Value &v2) {
    sub.add(new ConstGEConstraint(x1,
                        query->getStore()->getValueEqClass(v2).to + 1));
}
void GTExpression::postConst(Subtree &sub, Value &v1, VarInt *x2) {
    sub.add(new ConstLEConstraint(x2,
                        query->getStore()->getValueEqClass(v1).from - 1));
}


void LEExpression::postVars(Subtree &sub, VarInt *x1, VarInt *x2) {
    sub.add(new VarLessConstraint(query->getStore(), x1, x2, true));
}
void LEExpression::postConst(Subtree &sub, VarInt *x1, Value &v2) {
    sub.add(new ConstLEConstraint(x1,
                        query->getStore()->getValueEqClass(v2).to));
}
void LEExpression::postConst(Subtree &sub, Value &v1, VarInt *x2) {
    sub.add(new ConstGEConstraint(x2,
                        query->getStore()->getValueEqClass(v1).from));
}


void GEExpression::postVars(Subtree &sub, VarInt *x1, VarInt *x2) {
    sub.add(new VarLessConstraint(query->getStore(), x2, x1, true));
}
void GEExpression::postConst(Subtree &sub, VarInt *x1, Value &v2) {
    sub.add(new ConstGEConstraint(x1,
                        query->getStore()->getValueEqClass(v2).from));
}
void GEExpression::postConst(Subtree &sub, Value &v1, VarInt *x2) {
    sub.add(new ConstLEConstraint(x2,
                        query->getStore()->getValueEqClass(v1).to));
}

}
