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
#include "query.h"

#include <cassert>
#include <sstream>

#include "pattern.h"
#include "expression.h"
#include "constraints/distinct.h"
#include "constraints/bnborder.h"

using castor::librdf::Sequence;

namespace castor {

Solution::Solution(Query* query) : query_(query) {
    values_ = new Value::id_t[query->variables().size()];
    for(unsigned i = 0; i < query->variables().size(); i++)
        values_[i] = query->variable(i)->valueId();
}

Solution::~Solution() {
    delete [] values_;
}

void Solution::restore() const {
    for(unsigned i = 0; i < query_->variables().size(); i++)
        query_->variable(i)->valueId(values_[i]);
}

bool Solution::operator<(const Solution& o) const {
    assert(o.query_ == query_);
    const Solution& t = *this;
    for(Order order : query_->orders()) {
        if(VariableExpression* varexpr = dynamic_cast<VariableExpression*>(order.expression())) {
            Variable* x = varexpr->variable();
            if(t[x] != o[x])
                return order.isDescending() ? t[x] > o[x] : t[x] < o[x];
        } else {
            Value v1, v2;
            t.restore();
            if(!order.expression()->evaluate(v1))
                return false;
            o.restore();
            if(!order.expression()->evaluate(v2))
                return false;
            if(v1 != v2)
                return order.isDescending() ? v1 > v2 : v1 < v2;
        }
    }
    return false;
}

Query::Query(Store* store, const char* queryString) : store_(store) {
    rasqal_query* query = rasqal_new_query(librdf::World::instance().rasqal,
                                           "sparql", nullptr);
    pattern_ = nullptr;
    solutions_ = nullptr;
    try {
        if(rasqal_query_prepare(query,
                                reinterpret_cast<const unsigned char*>(queryString),
                                nullptr))
            throw CastorException() << "Unable to parse query";

        // DISTINCT, LIMIT and OFFSET (+ check for unsupported verbs)
        rasqal_query_verb verb = rasqal_query_get_verb(query);
        int rasqalOffset;
        switch(verb) {
        case RASQAL_QUERY_VERB_SELECT:
            distinct_ = rasqal_query_get_distinct(query); // ignores mode
            limit_ = rasqal_query_get_limit(query);
            rasqalOffset = rasqal_query_get_offset(query);
            offset_ = rasqalOffset < 0 ? 0 : rasqalOffset;
            break;
        case RASQAL_QUERY_VERB_ASK:
            distinct_ = false;
            limit_ = 1;
            offset_ = 0;
            break;
        default:
            throw CastorException() << "Unsupported rasqal verb: "
                                    << rasqal_query_get_verb(query);
        }

        // variables
        Sequence<rasqal_variable> seqRequested;
        switch(verb) {
        case RASQAL_QUERY_VERB_SELECT:
            seqRequested = rasqal_query_get_bound_variable_sequence(query);
            requested_ = seqRequested.size();
            break;
        case RASQAL_QUERY_VERB_ASK:
            requested_ = 0;
            break;
        default:
            assert(false); // should be catched above
            throw CastorException() << "BUG: should not happen";
        }
        Sequence<rasqal_variable> seqVars =
                rasqal_query_get_all_variable_sequence(query);
        Sequence<rasqal_variable> seqAnon =
                rasqal_query_get_anonymous_variable_sequence(query);

        vars_.reserve(seqVars.size() + seqAnon.size());
        for(rasqal_variable* var : seqRequested) {
            Variable* x = new Variable(this, vars_.size(),
                                       reinterpret_cast<const char*>(var->name));
            var->user_data = x;
            vars_.push_back(x);
        }
        for(rasqal_variable* var : seqVars) {
            if(var->user_data == nullptr) {
                Variable* x = new Variable(this, vars_.size(),
                                           reinterpret_cast<const char*>(var->name));
                var->user_data = x;
                vars_.push_back(x);
            }
        }
        for(rasqal_variable* var : seqAnon) {
            Variable* x = new Variable(this, vars_.size(), "");
            var->user_data = x;
            vars_.push_back(x);
        }

        // ORDERY BY expressions
        bnbOrderCstr_ = nullptr;
        if(verb == RASQAL_QUERY_VERB_SELECT) {
            Sequence<rasqal_expression> seqOrder =
                    rasqal_query_get_order_conditions_sequence(query);
            orders_.reserve(seqOrder.size());
            for(rasqal_expression* expr : seqOrder) {
                bool descending;
                switch(expr->op) {
                case RASQAL_EXPR_ORDER_COND_ASC:
                    descending = false;
                    expr = expr->arg1;
                    break;
                case RASQAL_EXPR_ORDER_COND_DESC:
                    descending = true;
                    expr = expr->arg1;
                    break;
                default:
                    descending = false;
                }
                orders_.emplace_back(convert(expr)->optimize(), descending);
            }
            if(!orders_.empty()) {
                solutions_ = new SolutionSet;
                if(limit_ >= 0) {
                    bnbOrderCstr_ = new BnBOrderConstraint(this);
                    solver_.add(bnbOrderCstr_);
                }
            }
        }

        // graph pattern
        pattern_ = convert(rasqal_query_get_query_graph_pattern(query));
        pattern_ = pattern_->optimize();
        pattern_->init();

        // DISTINCT constraint
        if(isDistinct()) {
            distinctCstr_ = new DistinctConstraint(this);
            solver_.add(distinctCstr_);
        } else {
            distinctCstr_ = nullptr;
        }

        // cleanup
        rasqal_free_query(query);

        nbSols_ = 0;

    } catch(std::exception) {
        delete pattern_;
        for(Variable* x : vars_)
            delete x;
        rasqal_free_query(query);
        throw;
    }
}

Query::~Query() {
    if(solutions_ != nullptr) {
        for(Solution* sol : *solutions_)
            delete sol;
        delete solutions_;
    }
    for(Order order : orders_)
        delete order.expression();
    delete pattern_;
    for(Variable* x : vars_)
        delete x;
}

Pattern* Query::convert(rasqal_graph_pattern* gp) {
    switch(rasqal_graph_pattern_get_operator(gp)) {
    case RASQAL_GRAPH_PATTERN_OPERATOR_BASIC:
    {
        BasicPattern* pat = new BasicPattern(this);
        try {
            rasqal_triple* triple;
            int i = 0;
            while((triple = rasqal_graph_pattern_get_triple(gp, i++))) {
                TriplePattern tpat(resolve(triple->subject),
                                   resolve(triple->predicate),
                                   resolve(triple->object));
                for(int i = 0; i < tpat.COMPONENTS; i++) {
                    if(tpat[i].isUnknown()) {
                        // We have an unknown value, this BGP will never match
                        delete pat;
                        return new FalsePattern(this);
                    }
                }
                pat->add(tpat);
            }
        } catch(std::exception) {
            delete pat;
            throw;
        }
        return pat;
    }
    case RASQAL_GRAPH_PATTERN_OPERATOR_UNION:
    {
        Pattern* pat = nullptr;
        Sequence<rasqal_graph_pattern> seq =
                rasqal_graph_pattern_get_sub_graph_pattern_sequence(gp);
        try {
            for(rasqal_graph_pattern* subgp : seq) {
                Pattern* subpat = convert(subgp);
                if(dynamic_cast<FalsePattern*>(subpat)) {
                    delete subpat;
                    continue;
                }
                if(!pat)
                    pat = subpat;
                else
                    pat = new UnionPattern(pat, subpat);
            }
        } catch(std::exception) {
            delete pat;
            throw;
        }
        if(!pat)
            pat = new FalsePattern(this);
        return pat;
    }
    case RASQAL_GRAPH_PATTERN_OPERATOR_GROUP:
    {
        Expression* expr = nullptr;
        Pattern* pat = nullptr;
        Sequence<rasqal_graph_pattern> seq =
                rasqal_graph_pattern_get_sub_graph_pattern_sequence(gp);
        try {
            for(rasqal_graph_pattern* subgp : seq) {
                switch(rasqal_graph_pattern_get_operator(subgp)) {
                case RASQAL_GRAPH_PATTERN_OPERATOR_FILTER:
                {
                    Expression* subexpr = convert(
                            rasqal_graph_pattern_get_filter_expression(subgp))
                            ->optimize();
                    if(expr == nullptr)
                        expr = subexpr;
                    else
                        expr = new AndExpression(expr, subexpr);
                    break;
                }
                case RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL:
                {
                    int n = raptor_sequence_size(
                            rasqal_graph_pattern_get_sub_graph_pattern_sequence(subgp));
                    if(n != 1)
                        throw CastorException()
                                << "Unable to handle OPTIONAL pattern with "
                                << n << " subpatterns";
                    Pattern* subpat = convert(
                            rasqal_graph_pattern_get_sub_graph_pattern(subgp, 0));
                    if(dynamic_cast<FalsePattern*>(subpat)) {
                        delete subpat;
                        break;
                    }
                    if(!pat)
                        pat = new BasicPattern(this); // empty pattern
                    pat = new LeftJoinPattern(pat, subpat);
                    break;
                }
                default:
                    Pattern* subpat = convert(subgp);
                    if(dynamic_cast<FalsePattern*>(subpat)) {
                        // one false pattern in a join makes the whole group false
                        delete pat;
                        delete expr;
                        return subpat;
                    }
                    if(!pat)
                        pat = subpat;
                    else
                        pat = new JoinPattern(pat, subpat);
                }
            }
        } catch(std::exception) {
            delete pat;
            delete expr;
            throw;
        }
        if(!pat)
            pat = new BasicPattern(this); // empty pattern
        if(expr)
            pat = new FilterPattern(pat, expr);
        return pat;
    }
    case RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL:
    {
        // lone optional pattern
        int n = raptor_sequence_size(
                rasqal_graph_pattern_get_sub_graph_pattern_sequence(gp));
        if(n != 1)
            throw CastorException()
                    << "Unable to handle OPTIONAL pattern with " << n
                    << " subpatterns";
        Pattern* subpat = convert(
                rasqal_graph_pattern_get_sub_graph_pattern(gp, 0));
        Pattern* pat = new BasicPattern(this); // empty pattern
        if(dynamic_cast<FalsePattern*>(subpat)) {
            delete subpat;
            return pat;
        }
        pat = new LeftJoinPattern(pat, subpat);
        return pat;
    }
    case RASQAL_GRAPH_PATTERN_OPERATOR_FILTER:
    {
        // lone filter pattern
        Expression* expr = convert(
                rasqal_graph_pattern_get_filter_expression(gp))->optimize();
        return new FilterPattern(new BasicPattern(this), expr);
    }
    default:
        throw CastorException() << "Unsupported rasqal graph pattern op: "
                                << rasqal_graph_pattern_get_operator(gp);
    }
}

Expression* Query::convert(rasqal_expression* expr) {
    if(expr == nullptr)
        return nullptr;
    switch(expr->op) {
    case RASQAL_EXPR_LITERAL:
    {
        rasqal_literal* lit = expr->literal;
        if(lit->type == RASQAL_LITERAL_VARIABLE) {
            return new VariableExpression(
                reinterpret_cast<Variable*>(lit->value.variable->user_data));
        } else {
            Value* val = new Value(lit);
            store_->resolve(*val);
            return new ValueExpression(this, val);
        }
    }
    case RASQAL_EXPR_BANG:   return new BangExpression  (convert(expr->arg1));
    case RASQAL_EXPR_UMINUS: return new UMinusExpression(convert(expr->arg1));
    case RASQAL_EXPR_BOUND:
        if(expr->arg1->op != RASQAL_EXPR_LITERAL ||
           expr->arg1->literal->type != RASQAL_LITERAL_VARIABLE)
            throw CastorException() << "BOUND expression expects a variable";
        return new BoundExpression(reinterpret_cast<Variable*>
                                   (expr->arg1->literal->value.variable->user_data));
    case RASQAL_EXPR_ISURI:     return new IsIriExpression    (convert(expr->arg1));
    case RASQAL_EXPR_ISBLANK:   return new IsBlankExpression  (convert(expr->arg1));
    case RASQAL_EXPR_ISLITERAL: return new IsLiteralExpression(convert(expr->arg1));
    case RASQAL_EXPR_STR:       return new StrExpression      (convert(expr->arg1));
    case RASQAL_EXPR_LANG:      return new LangExpression     (convert(expr->arg1));
    case RASQAL_EXPR_DATATYPE:  return new DatatypeExpression (convert(expr->arg1));

    /* The following cases have potential memory leaks when an exception occurs
     * in a recursive call of convertExpression after the first subexpression
     * has already been created. This only happens if rasqal produces an
     * incorrect SPARQL expression, which should not happen.
     */
    case RASQAL_EXPR_OR:
        return new OrExpression(convert(expr->arg1),
                                convert(expr->arg2));
    case RASQAL_EXPR_AND:
        return new AndExpression(convert(expr->arg1),
                                 convert(expr->arg2));
    case RASQAL_EXPR_EQ:
        return new EqExpression(convert(expr->arg1),
                                convert(expr->arg2));
    case RASQAL_EXPR_NEQ:
        return new NEqExpression(convert(expr->arg1),
                                 convert(expr->arg2));
    case RASQAL_EXPR_LT:
        return new LTExpression(convert(expr->arg1),
                                convert(expr->arg2));
    case RASQAL_EXPR_GT:
        return new GTExpression(convert(expr->arg1),
                                convert(expr->arg2));
    case RASQAL_EXPR_LE:
        return new LEExpression(convert(expr->arg1),
                                convert(expr->arg2));
    case RASQAL_EXPR_GE:
        return new GEExpression(convert(expr->arg1),
                                convert(expr->arg2));
    case RASQAL_EXPR_STAR:
        return new StarExpression(convert(expr->arg1),
                                  convert(expr->arg2));
    case RASQAL_EXPR_SLASH:
        return new SlashExpression(convert(expr->arg1),
                                   convert(expr->arg2));
    case RASQAL_EXPR_PLUS:
        return new PlusExpression(convert(expr->arg1),
                                  convert(expr->arg2));
    case RASQAL_EXPR_MINUS:
        return new MinusExpression(convert(expr->arg1),
                                   convert(expr->arg2));
    case RASQAL_EXPR_SAMETERM:
        return new SameTermExpression(convert(expr->arg1),
                                      convert(expr->arg2));
    case RASQAL_EXPR_LANGMATCHES:
        return new LangMatchesExpression(convert(expr->arg1),
                                         convert(expr->arg2));
    case RASQAL_EXPR_REGEX:
        return new RegExExpression(convert(expr->arg1),
                                   convert(expr->arg2),
                                   convert(expr->arg3));
    default:
        throw CastorException() << "Unsupported rasqal expression op: "
                                << expr->op;
    }
}

VarVal Query::resolve(rasqal_literal* literal) {
    if(literal->type == RASQAL_LITERAL_VARIABLE) {
        return VarVal((Variable*) literal->value.variable->user_data);
    } else {
        Value val(literal);
        store_->resolve(val);
        return VarVal(val);
    }
}

std::ostream& operator<<(std::ostream& out, const Query& q) {
    out << *q.pattern();
    return out;
}

////////////////////////////////////////////////////////////////////////////////
// Search

bool Query::next() {
    if(solutions_ == nullptr) {
        if(limit_ >= 0 && nbSols_ >= static_cast<unsigned>(limit_))
            return false;
        if(nbSols_ == 0) {
            for(unsigned i = 0; i < offset_; i++) {
                if(!nextPatternSolution())
                    return false;
            }
        }
        if(!nextPatternSolution())
            return false;
        nbSols_++;
        return true;
    } else {
        if(nbSols_ == 0) {
            while(nextPatternSolution()) {
                solutions_->insert(new Solution(this));
                if(limit_ >= 0) {
                    unsigned n = solutions_->size();
                    if(n > static_cast<unsigned>(limit_) + offset_) {
                        // only keep the first (offset + limit) solutions
                        SolutionSet::iterator it = solutions_->end();
                        --it;
                        delete *it;
                        solutions_->erase(it);
                        n--;
                    }
                    if(n == static_cast<unsigned>(limit_) + offset_)
                        bnbOrderCstr_->updateBound(*solutions_->rbegin());
                }
            }
            it_ = solutions_->begin();
            for(unsigned i = 0; i < offset_ && it_ != solutions_->end(); i++)
                ++it_;
        }
        if(it_ == solutions_->end())
            return false;
        Solution* sol = *it_;
        ++it_;
        nbSols_++;
        sol->restore();
        return true;
    }
}

bool Query::nextPatternSolution() {
    if(!pattern_->next())
        return false;
    for(Variable* x : vars_)
        x->setFromCP();
    if(distinctCstr_ != nullptr)
        distinctCstr_->addSolution();
    return true;
}

void Query::reset() {
    pattern_->discard();
    nbSols_ = 0;
    if(distinctCstr_ != nullptr)
        distinctCstr_->reset();
    if(solutions_ != nullptr) {
        for(Solution* sol : *solutions_)
            delete sol;
        solutions_->clear();
    }
}

}
