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
#include "expression.h"

#include <cmath>

#include "util.h"
#include "query.h"
#include "constraints.h"

namespace castor {

UnaryExpression::UnaryExpression(Expression* arg) :
        Expression(arg->query()), arg_(arg) {
    vars_ = arg->variables();
}
UnaryExpression::UnaryExpression(UnaryExpression&& o) :
        Expression(o.query()) {
    arg_   = o.arg_;
    o.arg_ = nullptr;
    vars_  = o.vars_;
}
UnaryExpression::~UnaryExpression() {
    delete arg_;
}

BinaryExpression::BinaryExpression(Expression* arg1, Expression* arg2) :
        Expression(arg1->query()), arg1_(arg1), arg2_(arg2) {
    vars_ = arg1->variables();
    vars_ += arg2->variables();
}
BinaryExpression::BinaryExpression(BinaryExpression&& o) :
        Expression(o.query()) {
    arg1_   = o.arg1_;
    arg2_   = o.arg2_;
    o.arg1_ = nullptr;
    o.arg2_ = nullptr;
    vars_   = o.vars_;
}
BinaryExpression::~BinaryExpression() {
    delete arg1_;
    delete arg2_;
}

ValueExpression::ValueExpression(Query* query, Value* value) :
        Expression(query), value_(value) {
    value->ensureInterpreted();
}
ValueExpression::~ValueExpression() {
    delete value_;
}

VariableExpression::VariableExpression(Variable* variable) :
        Expression(variable->query()), variable_(variable) {
    vars_ += variable;
}
BoundExpression::BoundExpression(Variable* variable) :
        Expression(variable->query()), variable_(variable) {
    vars_ += variable;
}

RegExExpression::RegExExpression(Expression* arg1, Expression* arg2,
                                 Expression* arg3) :
        Expression(arg1->query()), arg1_(arg1), arg2_(arg2), arg3_(arg3) {
    vars_ = arg1->variables();
    vars_ += arg2->variables();
    vars_ += arg3->variables();
}
RegExExpression::~RegExExpression() {
    delete arg1_;
    delete arg2_;
    delete arg3_;
}

CastExpression::CastExpression(Value::Type target, Expression* arg) :
        Expression(arg->query()), target_(target), arg_(arg) {
    vars_ = arg->variables();
}
CastExpression::~CastExpression() {
    delete arg_;
}

////////////////////////////////////////////////////////////////////////////////
// Optimzing expressions

Expression* BangExpression::optimize() {
    UnaryExpression::optimize();
    if(SameTermExpression* e = dynamic_cast<SameTermExpression*>(arg_)) {
        Expression* result = new DiffTermExpression(std::move(*e));
        delete this;
        return result;
    }
    if(DiffTermExpression* e = dynamic_cast<DiffTermExpression*>(arg_)) {
        Expression* result = new SameTermExpression(std::move(*e));
        delete this;
        return result;
    }
    return this;
}


////////////////////////////////////////////////////////////////////////////////
// Evaluation functions

int Expression::evaluateEBV(Value& buffer) {
    if(!evaluate(buffer))
        return -1;
    buffer.ensureInterpreted();
    if(buffer.isBoolean())
        return buffer.boolean ? 1 : 0;
    else if(buffer.isInteger())
        return buffer.integer ? 1 : 0;
    else if(buffer.isFloating())
        return std::isnan(buffer.floating) || buffer.floating == .0 ? 0 : 1;
    else if(buffer.isDecimal())
        return buffer.decimal->isZero() ? 0 : 1;
    else if(buffer.isPlain() || buffer.isXSDString())
        return buffer.lexicalLen == 0 ? 0 : 1;
    else
        return -1;
}

bool ValueExpression::evaluate(Value& result) {
    result.fillCopy(*value_, false); // shallow copy
    return true;
}

bool VariableExpression::evaluate(Value& result) {
    Value::id_t valid = variable_->valueId();
    if(valid == 0)
        return false;
    query_->store()->fetch(valid, result);
    return true;
}

bool BangExpression::evaluate(Value& result) {
    int ebv = arg_->evaluateEBV(result);
    if(ebv == -1)
        return false;
    result.fillBoolean(!ebv);
    return true;
}

bool UPlusExpression::evaluate(Value& result) {
    if(!arg_->evaluate(result) || !result.isNumeric())
        return false;
    return true;
}

bool UMinusExpression::evaluate(Value& result) {
    if(!arg_->evaluate(result))
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

bool BoundExpression::evaluate(Value& result) {
    result.fillBoolean(variable_->isBound());
    return true;
}

bool IsIriExpression::evaluate(Value& result) {
    if(!arg_->evaluate(result))
        return false;
    result.fillBoolean(result.isIRI());
    return true;
}

bool IsBlankExpression::evaluate(Value& result) {
    if(!arg_->evaluate(result))
        return false;
    result.fillBoolean(result.isBlank());
    return true;
}

bool IsLiteralExpression::evaluate(Value& result) {
    if(!arg_->evaluate(result))
        return false;
    result.fillBoolean(result.isLiteral());
    return true;
}

bool StrExpression::evaluate(Value& result) {
    if(!arg_->evaluate(result) || result.isBlank())
        return false;
    result.ensureLexical();
    bool freeLex = result.hasCleanFlag(Value::CLEAN_LEXICAL);
    result.removeCleanFlag(Value::CLEAN_LEXICAL);
    result.fillSimpleLiteral(result.lexical, result.lexicalLen, freeLex);
    return true;
}

bool LangExpression::evaluate(Value& result) {
    if(!arg_->evaluate(result) || !result.isPlain())
        return false;
    const char* lang = result.language;
    if(lang == nullptr)
        lang = "";
    bool freeLex = result.hasCleanFlag(Value::CLEAN_DATA);
    result.removeCleanFlag(Value::CLEAN_DATA);
    result.fillSimpleLiteral(lang, result.languageLen, freeLex);
    return true;
}

bool DatatypeExpression::evaluate(Value& result) {
    if(!arg_->evaluate(result) || !result.isLiteral())
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

bool OrExpression::evaluate(Value& result) {
    int left = arg1_->evaluateEBV(result);
    int right = arg2_->evaluateEBV(result);
    if(left == 1 || right == 1)
        result.fillBoolean(true);
    else if(left == 0 && right == 0)
        result.fillBoolean(false);
    else
        return false;
    return true;
}

bool AndExpression::evaluate(Value& result) {
    int left = arg1_->evaluateEBV(result);
    int right = arg2_->evaluateEBV(result);
    if(left == 0 || right == 0)
        result.fillBoolean(false);
    else if(left == 1 && right == 1)
        result.fillBoolean(true);
    else
        return false;
    return true;
}

bool EqExpression::evaluate(Value& result) {
    Value right;
    if(!arg1_->evaluate(result) || !arg2_->evaluate(right))
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

bool NEqExpression::evaluate(Value& result) {
    Value right;
    if(!arg1_->evaluate(result) || !arg2_->evaluate(right))
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

bool LTExpression::evaluate(Value& result) {
    Value right;
    if(!arg1_->evaluate(result) || !arg2_->evaluate(right))
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

bool GTExpression::evaluate(Value& result) {
    Value right;
    if(!arg1_->evaluate(result) || !arg2_->evaluate(right))
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

bool LEExpression::evaluate(Value& result) {
    Value right;
    if(!arg1_->evaluate(result) || !arg2_->evaluate(right))
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

bool GEExpression::evaluate(Value& result) {
    Value right;
    if(!arg1_->evaluate(result) || !arg2_->evaluate(right))
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

bool StarExpression::evaluate(Value& result) {
    Value right;
    if(!arg1_->evaluate(result) || !result.isNumeric() ||
       !arg2_->evaluate(right) || !right.isNumeric())
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

bool SlashExpression::evaluate(Value& result) {
    Value right;
    if(!arg1_->evaluate(result) || !result.isNumeric() ||
       !arg2_->evaluate(right) || !right.isNumeric())
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

bool PlusExpression::evaluate(Value& result) {
    Value right;
    if(!arg1_->evaluate(result) || !result.isNumeric() ||
       !arg2_->evaluate(right) || !right.isNumeric())
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

bool MinusExpression::evaluate(Value& result) {
    Value right;
    if(!arg1_->evaluate(result) || !result.isNumeric() ||
       !arg2_->evaluate(right) || !right.isNumeric())
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

bool SameTermExpression::evaluate(Value& result) {
    Value right;
    if(!arg1_->evaluate(result) || !arg2_->evaluate(right))
        return false;
    result.ensureLexical();
    right.ensureLexical();
    result.fillBoolean(result.rdfequals(right) == 0);
    return true;
}

bool DiffTermExpression::evaluate(Value& result) {
    Value right;
    if(!arg1_->evaluate(result) || !arg2_->evaluate(right))
        return false;
    result.ensureLexical();
    right.ensureLexical();
    result.fillBoolean(result.rdfequals(right) != 0);
    return true;
}

bool LangMatchesExpression::evaluate(Value& result) {
    result.clean();
    throw CastorException() << "Unsupported operator: LANGMATCHES";
}

bool RegExExpression::evaluate(Value& result) {
    result.clean();
    throw CastorException() << "Unsupported operator: REGEX";
}

bool CastExpression::evaluate(Value& result) {
    result.clean();
    throw CastorException() << "Unsupported operator: casting";
}

////////////////////////////////////////////////////////////////////////////////
// Posting constraints

void Expression::post(cp::Subtree& sub) {
    sub.add(new FilterConstraint(query_->store(), this));
}

void AndExpression::post(cp::Subtree& sub) {
    arg1_->post(sub);
    arg2_->post(sub);
}

void EqualityExpression::post(cp::Subtree& sub) {
    VariableExpression* var1 = dynamic_cast<VariableExpression*>(arg1_);
    VariableExpression* var2 = dynamic_cast<VariableExpression*>(arg2_);
    if(var1 && var2) {
        cp::RDFVar* x1 = var1->variable()->cp();
        cp::RDFVar* x2 = var2->variable()->cp();
        postVars(sub, x1, x2);
    } else if(var1 && arg2_->isConstant()) {
        Value val;
        if(arg2_->evaluate(val)) {
            cp::RDFVar* x = var1->variable()->cp();
            query_->store()->lookup(val);
            postConst(sub, x, val);
        } else {
            sub.add(new FalseConstraint());
        }
    } else if(var2 && arg1_->isConstant()) {
        Value val;
        if(arg1_->evaluate(val)) {
            cp::RDFVar* x = var2->variable()->cp();
            query_->store()->lookup(val);
            postConst(sub, x, val);
        } else {
            sub.add(new FalseConstraint());
        }
    } else {
        Expression::post(sub);
    }
}
void EqExpression::postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2) {
    sub.add(new VarEqConstraint(query_->store(), x1, x2));
}
void EqExpression::postConst(cp::Subtree& sub, cp::RDFVar* x, Value& v) {
    sub.add(new InRangeConstraint(x, query_->store()->eqClass(v)));
}
void NEqExpression::postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2) {
    /* In class CUSTOM, either two values are equal (and thus return false) or
     * the comparison produces a type error (making the constraint false).
     */
    sub.add(new NotInRangeConstraint(x1,
                    query_->store()->range(Value::CAT_OTHER)));
    sub.add(new NotInRangeConstraint(x2,
                    query_->store()->range(Value::CAT_OTHER)));
    sub.add(new VarDiffConstraint(query_->store(), x1, x2));
}
void NEqExpression::postConst(cp::Subtree& sub, cp::RDFVar* x, Value& v) {
    if(v.isLiteral() && v.type != Value::TYPE_CUSTOM) {
        sub.add(new InRangesConstraint(x,
            { query_->store()->range(Value::CAT_BLANK, Value::CAT_IRI),
              query_->store()->range(v.category()) }));
    } else {
        sub.add(new InRangeConstraint(x,
                        query_->store()->range(Value::CAT_BLANK,
                                                          Value::CAT_IRI)));
    }
    sub.add(new NotInRangeConstraint(x, query_->store()->eqClass(v)));
}
void SameTermExpression::postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2) {
    sub.add(new VarSameTermConstraint(x1, x2));
}
void SameTermExpression::postConst(cp::Subtree& sub, cp::RDFVar* x, Value& v) {
    if(v.id == 0) {
        sub.add(new FalseConstraint());
    } else {
        ValueRange rng = {v.id, v.id};
        sub.add(new InRangeConstraint(x, rng));
    }
}
void DiffTermExpression::postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2) {
    sub.add(new VarDiffTermConstraint(x1, x2));
}
void DiffTermExpression::postConst(cp::Subtree& sub, cp::RDFVar* x, Value& v) {
    if(v.id != 0) {
        ValueRange rng = {v.id, v.id};
        sub.add(new NotInRangeConstraint(x, rng));
    }
}

void InequalityExpression::post(cp::Subtree& sub) {
    VariableExpression* var1 = dynamic_cast<VariableExpression*>(arg1_);
    VariableExpression* var2 = dynamic_cast<VariableExpression*>(arg2_);
    if(var1 && var2) {
        cp::RDFVar* x1 = var1->variable()->cp();
        cp::RDFVar* x2 = var2->variable()->cp();
        sub.add(new ComparableConstraint(query_->store(), x1));
        sub.add(new ComparableConstraint(query_->store(), x2));
        sub.add(new SameClassConstraint(query_->store(), x1, x2));
        postVars(sub, x1, x2);
    } else if(var1 && arg2_->isConstant()) {
        Value val;
        if(arg2_->evaluate(val) && val.isComparable()) {
            cp::RDFVar* x = var1->variable()->cp();
            query_->store()->lookup(val);
            sub.add(new InRangeConstraint(x,
                        query_->store()->range(val.category())));
            postConst(sub, x, val);
        } else {
            sub.add(new FalseConstraint());
        }
    } else if(var2 && arg1_->isConstant()) {
        Value val;
        if(arg1_->evaluate(val) && val.isComparable()) {
            cp::RDFVar* x = var2->variable()->cp();
            query_->store()->lookup(val);
            sub.add(new InRangeConstraint(x,
                        query_->store()->range(val.category())));
            postConst(sub, val, x);
        } else {
            sub.add(new FalseConstraint());
        }
    } else {
        Expression::post(sub);
    }
}
void LTExpression::postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2) {
    sub.add(new VarLessConstraint(query_->store(), x1, x2, false));
}
void LTExpression::postConst(cp::Subtree& sub, cp::RDFVar* x1, Value& v2) {
    sub.add(new ConstLEConstraint(x1,
                        query_->store()->eqClass(v2).from - 1));
}
void LTExpression::postConst(cp::Subtree& sub, Value& v1, cp::RDFVar* x2) {
    sub.add(new ConstGEConstraint(x2,
                        query_->store()->eqClass(v1).to + 1));
}


void GTExpression::postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2) {
    sub.add(new VarLessConstraint(query_->store(), x2, x1, false));
}
void GTExpression::postConst(cp::Subtree& sub, cp::RDFVar* x1, Value& v2) {
    sub.add(new ConstGEConstraint(x1,
                        query_->store()->eqClass(v2).to + 1));
}
void GTExpression::postConst(cp::Subtree& sub, Value& v1, cp::RDFVar* x2) {
    sub.add(new ConstLEConstraint(x2,
                        query_->store()->eqClass(v1).from - 1));
}


void LEExpression::postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2) {
    sub.add(new VarLessConstraint(query_->store(), x1, x2, true));
}
void LEExpression::postConst(cp::Subtree& sub, cp::RDFVar* x1, Value& v2) {
    sub.add(new ConstLEConstraint(x1,
                        query_->store()->eqClass(v2).to));
}
void LEExpression::postConst(cp::Subtree& sub, Value& v1, cp::RDFVar* x2) {
    sub.add(new ConstGEConstraint(x2,
                        query_->store()->eqClass(v1).from));
}


void GEExpression::postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2) {
    sub.add(new VarLessConstraint(query_->store(), x2, x1, true));
}
void GEExpression::postConst(cp::Subtree& sub, cp::RDFVar* x1, Value& v2) {
    sub.add(new ConstGEConstraint(x1,
                        query_->store()->eqClass(v2).from));
}
void GEExpression::postConst(cp::Subtree& sub, Value& v1, cp::RDFVar* x2) {
    sub.add(new ConstLEConstraint(x2,
                        query_->store()->eqClass(v1).to));
}

}
