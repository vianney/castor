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
#include "expression.h"

#include <cmath>

#include <pcrecpp.h>

#include "util.h"
#include "query.h"
#include "constraints/fallback.h"
#include "constraints/unary.h"
#include "constraints/bool.h"
#include "constraints/compare.h"

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
    value->ensureInterpreted(*query->store());
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

RegExExpression::RegExExpression(Expression* text, Expression* pattern,
                                 Expression* flags) :
        Expression(text->query()), text_(text), pattern_(pattern), flags_(flags) {
    vars_ = text->variables();
    vars_ += pattern->variables();
    if(flags_ != nullptr)
        vars_ += flags->variables();
}
RegExExpression::~RegExExpression() {
    delete text_;
    delete pattern_;
    delete flags_;
}

//CastExpression::CastExpression(Value::Type target, Expression* arg) :
//        Expression(arg->query()), target_(target), arg_(arg) {
//    vars_ = arg->variables();
//}
//CastExpression::~CastExpression() {
//    delete arg_;
//}

////////////////////////////////////////////////////////////////////////////////
// Optimzing expressions


////////////////////////////////////////////////////////////////////////////////
// Evaluation functions

TriState Expression::evaluateEBV(Value& buffer) {
    if(!evaluate(buffer))
        return RDF_ERROR;
    buffer.ensureInterpreted(*query_->store());
    if(buffer.isBoolean()) {
        return buffer.boolean() ? RDF_TRUE : RDF_FALSE;
    } else if(buffer.isInteger()) {
        return buffer.integer() ? RDF_TRUE : RDF_FALSE;
    } else if(buffer.isFloating()) {
        return std::isnan(buffer.floating()) || buffer.floating() == .0 ?
                    RDF_FALSE : RDF_TRUE;
    } else if(buffer.isDecimal()) {
        return buffer.decimal().isZero() ? RDF_FALSE : RDF_TRUE;
    } else if(buffer.isPlain() || buffer.isXSDString()) {
        // TODO: we only need lexical to be direct
        buffer.ensureDirectStrings(*query_->store());
        return buffer.lexical().length() == 0 ? RDF_FALSE : RDF_TRUE;
    } else {
        return RDF_ERROR;
    }
}

bool ValueExpression::evaluate(Value& result) {
    result.fillCopy(*value_, false); // shallow copy
    return true;
}

bool VariableExpression::evaluate(Value& result) {
    Value::id_t id = variable_->valueId();
    if(id == 0)
        return false;
    result = query_->store()->lookupValue(id);
    return true;
}

bool BangExpression::evaluate(Value& result) {
    TriState ebv = arg_->evaluateEBV(result);
    if(ebv == RDF_ERROR)
        return false;
    result.fillBoolean(ebv == RDF_TRUE ? false : true);
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
    result.ensureInterpreted(*query_->store());
    if(result.isInteger())
        result.fillInteger(-result.integer());
    else if(result.isDecimal())
        result.fillDecimal(result.decimal().negate());
    else if(result.isFloating())
        result.fillFloating(-result.floating());
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
    result.fillBoolean(result.isURI());
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
    result.fillSimpleLiteral(std::move(result.lexical()));
    return true;
}

bool LangExpression::evaluate(Value& result) {
    if(!arg_->evaluate(result) || !result.isPlain())
        return false;
    if(result.isSimple())
        result.fillSimpleLiteral(String(""));
    else
        result.fillSimpleLiteral(std::move(result.language()));
    return true;
}

bool DatatypeExpression::evaluate(Value& result) {
    if(!arg_->evaluate(result) || !result.isLiteral() || result.isPlainWithLang())
        return false;
    if(result.isSimple()) {
        result.fillURI(String(       "http://www.w3.org/2001/XMLSchema#string",
                              sizeof("http://www.w3.org/2001/XMLSchema#string") - 1));
    } else {
        // FIXME: ensure there is a datatype lexical
        result.fillURI(std::move(result.datatypeLex()));
    }
    return true;
}

bool OrExpression::evaluate(Value& result) {
    TriState left = arg1_->evaluateEBV(result);
    TriState right = arg2_->evaluateEBV(result);
    if(left == RDF_TRUE || right == RDF_TRUE)
        result.fillBoolean(true);
    else if(left == RDF_FALSE && right == RDF_FALSE)
        result.fillBoolean(false);
    else
        return false;
    return true;
}

bool AndExpression::evaluate(Value& result) {
    TriState left = arg1_->evaluateEBV(result);
    TriState right = arg2_->evaluateEBV(result);
    if(left == RDF_FALSE || right == RDF_FALSE)
        result.fillBoolean(false);
    else if(left == RDF_TRUE && right == RDF_TRUE)
        result.fillBoolean(true);
    else
        return false;
    return true;
}

bool EqExpression::evaluate(Value& result) {
    Value right;
    if(!arg1_->evaluate(result) || !arg2_->evaluate(right))
        return false;
    result.ensureInterpreted(*query_->store());
    right.ensureInterpreted(*query_->store());
    switch(result.equals(right)) {
    case -1:
        return false;
    case 0:
        result.fillBoolean(true);
        return true;
    case 1:
        result.fillBoolean(false);
        return true;
    default:
        assert(false); // should not happen
        return false;
    }
}

bool NEqExpression::evaluate(Value& result) {
    Value right;
    if(!arg1_->evaluate(result) || !arg2_->evaluate(right))
        return false;
    result.ensureInterpreted(*query_->store());
    right.ensureInterpreted(*query_->store());
    switch(result.equals(right)) {
    case -1:
        return false;
    case 0:
        result.fillBoolean(false);
        return true;
    case 1:
        result.fillBoolean(true);
        return true;
    default:
        assert(false); // should not happen
        return false;
    }
}

bool LTExpression::evaluate(Value& result) {
    Value right;
    if(!arg1_->evaluate(result) || !arg2_->evaluate(right))
        return false;
    result.ensureInterpreted(*query_->store());
    right.ensureInterpreted(*query_->store());
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
    result.ensureInterpreted(*query_->store());
    right.ensureInterpreted(*query_->store());
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
    result.ensureInterpreted(*query_->store());
    right.ensureInterpreted(*query_->store());
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
    result.ensureInterpreted(*query_->store());
    right.ensureInterpreted(*query_->store());
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
    result.ensureInterpreted(*query_->store());
    right.ensureInterpreted(*query_->store());
    Value::promoteNumericType(result, right);
    if(right.isInteger())
        result.fillInteger(result.integer() * right.integer());
    else if(right.isDecimal())
        result.fillDecimal(result.decimal().multiply(right.decimal()));
    else
        result.fillFloating(result.floating() * right.floating());
    return true;
}

bool SlashExpression::evaluate(Value& result) {
    Value right;
    if(!arg1_->evaluate(result) || !result.isNumeric() ||
       !arg2_->evaluate(right) || !right.isNumeric())
        return false;
    result.ensureInterpreted(*query_->store());
    right.ensureInterpreted(*query_->store());
    Value::promoteNumericType(result, right);
    if(right.isInteger()) {
        XSDDecimal d1(result.integer()), d2(result.integer());
        result.fillDecimal(d1.divide(d2));
    } else if(right.isDecimal()) {
        result.fillDecimal(result.decimal().divide(right.decimal()));
    } else {
        result.fillFloating(result.floating() / right.floating());
    }
    return true;
}

bool PlusExpression::evaluate(Value& result) {
    Value right;
    if(!arg1_->evaluate(result) || !result.isNumeric() ||
       !arg2_->evaluate(right) || !right.isNumeric())
        return false;
    result.ensureInterpreted(*query_->store());
    right.ensureInterpreted(*query_->store());
    Value::promoteNumericType(result, right);
    if(right.isInteger())
        result.fillInteger(result.integer() + right.integer());
    else if(right.isDecimal())
        result.fillDecimal(result.decimal().add(right.decimal()));
    else
        result.fillFloating(result.floating() + right.floating());
    return true;
}

bool MinusExpression::evaluate(Value& result) {
    Value right;
    if(!arg1_->evaluate(result) || !result.isNumeric() ||
       !arg2_->evaluate(right) || !right.isNumeric())
        return false;
    result.ensureInterpreted(*query_->store());
    right.ensureInterpreted(*query_->store());
    Value::promoteNumericType(result, right);
    if(right.isInteger())
        result.fillInteger(result.integer() - right.integer());
    else if(right.isDecimal())
        result.fillDecimal(result.decimal().substract(right.decimal()));
    else
        result.fillFloating(result.floating() - right.floating());
    return true;
}

bool SameTermExpression::evaluate(Value& result) {
    Value right;
    if(!arg1_->evaluate(result) || !arg2_->evaluate(right))
        return false;
    result.ensureLexical();
    right.ensureLexical();
    result.fillBoolean(result == right);
    return true;
}

bool LangMatchesExpression::evaluate(Value& result) {
    Value right;
    if(!arg1_->evaluate(result) || !result.isSimple() ||
       !arg2_->evaluate(right)  || !right.isSimple())
        return false;
    result.ensureDirectStrings(*query_->store());
    right.ensureDirectStrings(*query_->store());
    const char* tag = result.lexical().str();
    unsigned taglen = result.lexical().length();
    const char* range = right.lexical().str();
    unsigned rangelen = right.lexical().length();
    if(strcmp(range, "*") == 0) {
        result.fillBoolean(taglen > 0);
    } else if(taglen < rangelen) {
        result.fillBoolean(false);
    } else {
        result.fillBoolean(strncasecmp(tag, range, rangelen) == 0 &&
                           (tag[rangelen] == '\0' || tag[rangelen] == '-'));
    }
    return true;
}

bool RegExExpression::evaluate(Value& result) {
    Value pattern, flags;
    pcrecpp::RE_Options opts;
    opts.set_utf8(true);
    opts.set_no_auto_capture(true);
    if(flags_ != nullptr) {
        if(!flags_->evaluate(flags) || !flags.isSimple())
            return false;
        flags.ensureDirectStrings(*query_->store());
        const char* f = flags.lexical().str();
        while(*f != '\0') {
            switch(*f) {
            case 'i': opts.set_caseless (true); break;
            case 's': opts.set_dotall   (true); break;
            case 'm': opts.set_multiline(true); break;
            case 'x': opts.set_extended (true); break;
            default: return false;
            }
            ++f;
        }
    }
    if(!text_->evaluate(result)     || !result.isSimple() ||
       !pattern_->evaluate(pattern) || !pattern.isSimple())
        return false;
    result.ensureDirectStrings(*query_->store());
    pattern.ensureDirectStrings(*query_->store());
    result.fillBoolean(pcrecpp::RE(pattern.lexical().str(), opts)
                       .PartialMatch(result.lexical().str()));
    return true;
}

//bool CastExpression::evaluate(Value& result) {
//    throw CastorException() << "Unsupported operator: casting";
//}

////////////////////////////////////////////////////////////////////////////////
// Posting constraints

void Expression::post(cp::Subtree& sub, cp::TriStateVar* b) {
    sub.add(new FilterConstraint(query_, this, b));
}

void BangExpression::post(cp::Subtree& sub, cp::TriStateVar* b) {
    cp::TriStateVar* x = new cp::TriStateVar(query_->solver());
    query_->solver()->collect(x);
    arg_->post(sub, x);
    sub.add(new NotConstraint(query_, x, b));
}

void OrExpression::post(cp::Subtree& sub, cp::TriStateVar* b) {
    cp::TriStateVar* b1 = new cp::TriStateVar(query_->solver());
    cp::TriStateVar* b2 = new cp::TriStateVar(query_->solver());
    query_->solver()->collect(b1);
    query_->solver()->collect(b2);
    arg1_->post(sub, b1);
    arg2_->post(sub, b2);
    sub.add(new OrConstraint(query_, b1, b2, b));
}

void AndExpression::post(cp::Subtree& sub, cp::TriStateVar* b) {
    cp::TriStateVar* b1 = new cp::TriStateVar(query_->solver());
    cp::TriStateVar* b2 = new cp::TriStateVar(query_->solver());
    query_->solver()->collect(b1);
    query_->solver()->collect(b2);
    arg1_->post(sub, b1);
    arg2_->post(sub, b2);
    sub.add(new AndConstraint(query_, b1, b2, b));
}

void EqualityExpression::post(cp::Subtree& sub, cp::TriStateVar* b) {
    VariableExpression* var1 = dynamic_cast<VariableExpression*>(arg1_);
    VariableExpression* var2 = dynamic_cast<VariableExpression*>(arg2_);
    if(var1 && var2) {
        cp::RDFVar* x1 = var1->variable()->cp();
        cp::RDFVar* x2 = var2->variable()->cp();
        postVars(sub, x1, x2, b);
    } else if(var1 && arg2_->isConstant()) {
        Value val;
        if(arg2_->evaluate(val)) {
            cp::RDFVar* x = var1->variable()->cp();
            query_->store()->resolve(val);
            val.ensureInterpreted(*query_->store());
            postConst(sub, x, val, b);
        } else {
            sub.add(new ErrorConstraint(query_, b));
        }
    } else if(var2 && arg1_->isConstant()) {
        Value val;
        if(arg1_->evaluate(val)) {
            cp::RDFVar* x = var2->variable()->cp();
            query_->store()->resolve(val);
            val.ensureInterpreted(*query_->store());
            postConst(sub, x, val, b);
        } else {
            sub.add(new ErrorConstraint(query_, b));
        }
    } else {
        Expression::post(sub, b);
    }
}

void EqExpression::postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2,
                            cp::TriStateVar* b) {
    sub.add(new VarEqConstraint(query_, x1, x2, b));
}
void EqExpression::postConst(cp::Subtree& sub, cp::RDFVar* x, Value& v,
                             cp::TriStateVar* b) {
    ValueRange rng = query_->store()->eqClass(v);
    if(rng.empty()) {
        // No value equivalent to v2 exists in the store
        Value::Category cat = v.category();
        if(cat <= Value::CAT_URI) {
            sub.add(new FalseConstraint(query_, b)); // Always comparable
        } else if(cat > Value::CAT_DATETIME) {
            sub.add(new ErrorConstraint(query_, b)); // Always type error
        } else {
            sub.add(new NotTrueConstraint(query_, b));
            sub.add(new InRangeConstraint(query_, x,
                                          query_->store()->range(cat), b));
        }
    } else {
        cp::RDFVar* x2 = new cp::RDFVar(query_->solver(), rng.from, rng.from);
        query_->solver()->collect(x2);
        sub.add(new VarEqConstraint(query_, x, x2, b));
    }
}

void NEqExpression::post(cp::Subtree &sub, cp::TriStateVar *b) {
    cp::TriStateVar* b2 = new cp::TriStateVar(query_->solver());
    query_->solver()->collect(b2);
    EqExpression::post(sub, b2);
    sub.add(new NotConstraint(query_, b2, b));
}

void SameTermExpression::postVars(cp::Subtree &sub, cp::RDFVar *x1,
                                  cp::RDFVar *x2, cp::TriStateVar *b) {
    sub.add(new VarSameTermConstraint(query_, x1, x2, b));
}
void SameTermExpression::postConst(cp::Subtree &sub, cp::RDFVar *x, Value &v,
                                   cp::TriStateVar *b) {
    if(v.validId()) {
        cp::RDFVar* x2 = new cp::RDFVar(query_->solver(), v.id(), v.id());
        query_->solver()->collect(x2);
        sub.add(new VarSameTermConstraint(query_, x, x2, b));
    } else {
        sub.add(new FalseConstraint(query_, b));
    }
}

void InequalityExpression::post(cp::Subtree& sub, cp::TriStateVar* b) {
    VariableExpression* var1 = dynamic_cast<VariableExpression*>(arg1_);
    VariableExpression* var2 = dynamic_cast<VariableExpression*>(arg2_);
    if(var1 && var2) {
        cp::RDFVar* x1 = var1->variable()->cp();
        cp::RDFVar* x2 = var2->variable()->cp();
        postVars(sub, x1, x2, b);
    } else if(var1 && arg2_->isConstant()) {
        Value val;
        if(arg2_->evaluate(val)) {
            cp::RDFVar* x = var1->variable()->cp();
            query_->store()->resolve(val);
            val.ensureInterpreted(*query_->store());
            sub.add(new InRangeConstraint(query_, x,
                        query_->store()->range(val.category()), b));
            postConst(sub, x, val, b);
        } else {
            sub.add(new ErrorConstraint(query_, b));
        }
    } else if(var2 && arg1_->isConstant()) {
        Value val;
        if(arg1_->evaluate(val)) {
            cp::RDFVar* x = var2->variable()->cp();
            query_->store()->resolve(val);
            val.ensureInterpreted(*query_->store());
            sub.add(new InRangeConstraint(query_, x,
                                          query_->store()->range(val.category()),
                                          b));
            postConst(sub, val, x, b);
        } else {
            sub.add(new ErrorConstraint(query_, b));
        }
    } else {
        Expression::post(sub, b);
    }
}

void LTExpression::postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2,
                            cp::TriStateVar* b) {
    sub.add(new VarLessConstraint(query_, x1, x2, b, false));
}
void LTExpression::postConst(cp::Subtree& sub, cp::RDFVar* x1, Value& v2,
                             cp::TriStateVar* b) {
    sub.add(new ConstLEConstraint(query_, x1,
                        query_->store()->eqClass(v2).from - 1, b));
}
void LTExpression::postConst(cp::Subtree& sub, Value& v1, cp::RDFVar* x2,
                             cp::TriStateVar* b) {
    sub.add(new ConstGEConstraint(query_, x2,
                        query_->store()->eqClass(v1).to + 1, b));
}


void GTExpression::postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2,
                            cp::TriStateVar* b) {
    sub.add(new VarLessConstraint(query_, x2, x1, b, false));
}
void GTExpression::postConst(cp::Subtree& sub, cp::RDFVar* x1, Value& v2,
                             cp::TriStateVar* b) {
    sub.add(new ConstGEConstraint(query_, x1,
                        query_->store()->eqClass(v2).to + 1, b));
}
void GTExpression::postConst(cp::Subtree& sub, Value& v1, cp::RDFVar* x2,
                             cp::TriStateVar* b) {
    sub.add(new ConstLEConstraint(query_, x2,
                        query_->store()->eqClass(v1).from - 1, b));
}


void LEExpression::postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2,
                            cp::TriStateVar* b) {
    sub.add(new VarLessConstraint(query_, x1, x2, b, true));
}
void LEExpression::postConst(cp::Subtree& sub, cp::RDFVar* x1, Value& v2,
                             cp::TriStateVar* b) {
    sub.add(new ConstLEConstraint(query_, x1,
                        query_->store()->eqClass(v2).to, b));
}
void LEExpression::postConst(cp::Subtree& sub, Value& v1, cp::RDFVar* x2,
                             cp::TriStateVar* b) {
    sub.add(new ConstGEConstraint(query_, x2,
                        query_->store()->eqClass(v1).from, b));
}


void GEExpression::postVars(cp::Subtree& sub, cp::RDFVar* x1, cp::RDFVar* x2,
                            cp::TriStateVar* b) {
    sub.add(new VarLessConstraint(query_, x2, x1, b, true));
}
void GEExpression::postConst(cp::Subtree& sub, cp::RDFVar* x1, Value& v2,
                             cp::TriStateVar* b) {
    sub.add(new ConstGEConstraint(query_, x1,
                        query_->store()->eqClass(v2).from, b));
}
void GEExpression::postConst(cp::Subtree& sub, Value& v1, cp::RDFVar* x2,
                             cp::TriStateVar* b) {
    sub.add(new ConstLEConstraint(query_, x2,
                        query_->store()->eqClass(v1).to, b));
}

}
