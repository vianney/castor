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

#include <cassert>
#include <sstream>
#include "query.h"
#include "pattern.h"
#include "expression.h"
#include "distinct.h"
#include "bnborder.h"

namespace castor {

Solution::Solution(Query *query) : query(query) {
    values = new Value::id_t[query->getVariablesCount()];
    for(unsigned i = 0; i < query->getVariablesCount(); i++)
        values[i] = query->getVariable(i)->getValueId();
}

Solution::~Solution() {
    delete [] values;
}

void Solution::restore() const {
    for(unsigned i = 0; i < query->getVariablesCount(); i++)
        query->getVariable(i)->setValueId(values[i]);
}

bool Solution::operator <(const Solution &o) const {
    if(o.query != query)
        return false;
    for(unsigned i = 0; i < query->getOrderCount(); i++) {
        Expression *expr = query->getOrder(i);
        if(VariableExpression *varexpr = dynamic_cast<VariableExpression*>(expr)) {
            unsigned varid = varexpr->getVariable()->getId();
            if(getValueId(varid) != o.getValueId(varid))
                return query->isOrderDescending(i) ?
                       getValueId(varid) > o.getValueId(varid) :
                       getValueId(varid) < o.getValueId(varid);
        } else {
            Value v1, v2;
            restore();
            if(!expr->evaluate(v1))
                return false;
            o.restore();
            if(!expr->evaluate(v2))
                return false;
            if(v1 != v2)
                return query->isOrderDescending(i) ? v1 > v2 : v1 < v2;
        }
    }
    return false;
}


using librdf::Sequence;

Query::Query(Store *store, const char *queryString) throw(QueryParseException)
        : store(store) {
    rasqal_query *query = rasqal_new_query(librdf::World::instance().rasqal,
                                           "sparql", NULL);
    vars = NULL;
    pattern = NULL;
    solutions = NULL;
    try {
        if(rasqal_query_prepare(query,
                                reinterpret_cast<const unsigned char*>(queryString),
                                NULL))
            throw QueryParseException("unable to parse query");

        // DISTINCT, LIMIT and OFFSET (+ check for unsupported verbs)
        rasqal_query_verb verb = rasqal_query_get_verb(query);
        int rasqalOffset;
        switch(verb) {
        case RASQAL_QUERY_VERB_SELECT:
            distinct = rasqal_query_get_distinct(query); // ignores mode
            limit = rasqal_query_get_limit(query);
            rasqalOffset = rasqal_query_get_offset(query);
            offset = rasqalOffset < 0 ? 0 : rasqalOffset;
            break;
        case RASQAL_QUERY_VERB_ASK:
            distinct = false;
            limit = 1;
            offset = 0;
            break;
        default:
            std::ostringstream msg;
            msg << "unsupported rasqal verb " << rasqal_query_get_verb(query);
            throw QueryParseException(msg.str());
        }

        // variables
        Sequence<rasqal_variable> seq;
        switch(verb) {
        case RASQAL_QUERY_VERB_SELECT:
            seq = rasqal_query_get_bound_variable_sequence(query);
            nbRequestedVars = seq.size();
            break;
        case RASQAL_QUERY_VERB_ASK:
            nbRequestedVars = 0;
            break;
        default:
            assert(false); // should not happen
        }
        Sequence<rasqal_variable> seqVars =
                rasqal_query_get_all_variable_sequence(query);
        int nbReal = seqVars.size();
        Sequence<rasqal_variable> seqAnon =
                rasqal_query_get_anonymous_variable_sequence(query);
        int nbAnon = seqAnon.size();

        nbVars = nbReal + nbAnon;
        vars = new Variable[nbVars];
        unsigned i;
        for(i = 0; i < nbRequestedVars; i++) {
            rasqal_variable *var = seq[i];
            var->user_data = &vars[i];
            vars[i].query = this;
            vars[i].id = i;
            vars[i].name = (const char*) var->name;
            vars[i].var = new cp::RDFVar(&solver, 0, store->getValueCount());
        }
        for(int j = 0; j < nbReal; j++) {
            rasqal_variable *var = seqVars[j];
            if(var->user_data == NULL) {
                var->user_data = &vars[i];
                vars[i].query = this;
                vars[i].id = i;
                vars[i].name = (const char*) var->name;
                vars[i].var = new cp::RDFVar(&solver, 0, store->getValueCount());
                i++;
            }
        }
        for(int j = 0; j < nbAnon; j++) {
            rasqal_variable *var = seqAnon[j];
            var->user_data = &vars[i];
            vars[i].query = this;
            vars[i].id = i;
            vars[i].var = new cp::RDFVar(&solver, 0, store->getValueCount());
            i++;
        }

        // ORDERY BY expressions
        bnbOrderCstr = NULL;
        if(verb == RASQAL_QUERY_VERB_SELECT) {
            Sequence<rasqal_expression> seqOrder =
                    rasqal_query_get_order_conditions_sequence(query);
            nbOrder = seqOrder.size();
            if(nbOrder > 0) {
                order = new Expression*[nbOrder];
                orderDescending = new bool[nbOrder];
                for(unsigned i = 0; i < nbOrder; i++) {
                    rasqal_expression *expr = seqOrder[i];
                    switch(expr->op) {
                    case RASQAL_EXPR_ORDER_COND_ASC:
                        orderDescending[i] = false;
                        expr = expr->arg1;
                        break;
                    case RASQAL_EXPR_ORDER_COND_DESC:
                        orderDescending[i] = true;
                        expr = expr->arg1;
                        break;
                    default:
                        orderDescending[i] = false;
                    }
                    order[i] = convertExpression(expr)->optimize();
                }
                solutions = new SolutionSet;
                if(limit >= 0) {
                    bnbOrderCstr = new BnBOrderConstraint(this);
                    solver.add(bnbOrderCstr);
                }
            } else {
                order = NULL;
                orderDescending = NULL;
            }
        } else {
            nbOrder = 0;
            order = NULL;
            orderDescending = NULL;
        }

        // graph pattern
        pattern = convertPattern(rasqal_query_get_query_graph_pattern(query));
        pattern = pattern->optimize();
        pattern->init();

        // DISTINCT constraint
        if(isDistinct()) {
            distinctCstr = new DistinctConstraint(this);
            solver.add(distinctCstr);
        } else {
            distinctCstr = NULL;
        }

        // cleanup
        rasqal_free_query(query);

        nbSols = 0;

    } catch(QueryParseException) {
        if(pattern)
            delete pattern;
        if(vars)
            delete [] vars;
        rasqal_free_query(query);
        throw;
    }
}

Query::~Query() {
    if(solutions != NULL) {
        for(Solution* sol : *solutions)
            delete sol;
        delete solutions;
    }
    if(nbOrder > 0) {
        for(unsigned i = 0; i < nbOrder; i++)
            delete order[i];
        delete [] order;
        delete [] orderDescending;
    }
    delete [] vars;
    delete pattern;
}

Pattern* Query::convertPattern(rasqal_graph_pattern *gp) throw(QueryParseException) {
    switch(rasqal_graph_pattern_get_operator(gp)) {
    case RASQAL_GRAPH_PATTERN_OPERATOR_BASIC:
    {
        BasicPattern *pat = new BasicPattern(this);
        try {
            rasqal_triple *triple;
            int i = 0;
            while((triple = rasqal_graph_pattern_get_triple(gp, i++))) {
                StatementPattern stmt(getVarVal(triple->subject),
                                      getVarVal(triple->predicate),
                                      getVarVal(triple->object));
                if(stmt.subject.isUnknown() ||
                   stmt.predicate.isUnknown() ||
                   stmt.object.isUnknown()) {
                    // We have an unknown value, this BGP will never match
                    delete pat;
                    return new FalsePattern(this);
                }
                pat->add(stmt);
            }
        } catch(QueryParseException) {
            delete pat;
            throw;
        }
        return pat;
    }
    case RASQAL_GRAPH_PATTERN_OPERATOR_UNION:
    {
        Pattern *pat = NULL;
        Sequence<rasqal_graph_pattern> seq =
                rasqal_graph_pattern_get_sub_graph_pattern_sequence(gp);
        int n = seq.size();
        try {
            for(int i = 0; i < n; i++) {
                rasqal_graph_pattern *subgp = seq[i];
                Pattern *subpat = convertPattern(subgp);
                if(dynamic_cast<FalsePattern*>(subpat)) {
                    delete subpat;
                    continue;
                }
                if(!pat)
                    pat = subpat;
                else
                    pat = new UnionPattern(pat, subpat);
            }
        } catch(QueryParseException) {
            if(pat != NULL)
                delete pat;
            throw;
        }
        if(!pat)
            pat = new FalsePattern(this);
        return pat;
    }
    case RASQAL_GRAPH_PATTERN_OPERATOR_GROUP:
    {
        Expression *expr = NULL;
        Pattern *pat = NULL;
        Sequence<rasqal_graph_pattern> seq =
                rasqal_graph_pattern_get_sub_graph_pattern_sequence(gp);
        int n = seq.size();
        try {
            for(int i = 0; i < n; i++) {
                rasqal_graph_pattern *subgp = seq[i];
                switch(rasqal_graph_pattern_get_operator(subgp)) {
                case RASQAL_GRAPH_PATTERN_OPERATOR_FILTER:
                {
                    Expression *subexpr = convertExpression(
                            rasqal_graph_pattern_get_filter_expression(subgp))
                            ->optimize();
                    if(expr == NULL)
                        expr = subexpr;
                    else
                        expr = new AndExpression(expr, subexpr);
                    break;
                }
                case RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL:
                {
                    int m = raptor_sequence_size(
                            rasqal_graph_pattern_get_sub_graph_pattern_sequence(subgp));
                    if(m != 1) {
                        std::ostringstream msg;
                        msg << "unable to handle OPTIONAL pattern with " << m
                                << " subpatterns";
                        throw QueryParseException(msg.str());
                    }
                    Pattern *subpat = convertPattern(
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
                    Pattern *subpat = convertPattern(subgp);
                    if(dynamic_cast<FalsePattern*>(subpat)) {
                        // one false pattern in a join makes the whole group false
                        if(pat)
                            delete pat;
                        if(expr)
                            delete expr;
                        return subpat;
                    }
                    if(!pat)
                        pat = subpat;
                    else
                        pat = new JoinPattern(pat, subpat);
                }
            }
        } catch(QueryParseException) {
            if(pat)
                delete pat;
            if(expr)
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
        if(n != 1) {
            std::ostringstream msg;
            msg << "unable to handle OPTIONAL pattern with " << n
                    << " subpatterns";
            throw QueryParseException(msg.str());
        }
        Pattern *subpat = convertPattern(
                rasqal_graph_pattern_get_sub_graph_pattern(gp, 0));
        Pattern *pat = new BasicPattern(this); // empty pattern
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
        Expression *expr = convertExpression(
                rasqal_graph_pattern_get_filter_expression(gp))->optimize();
        return new FilterPattern(new BasicPattern(this), expr);
    }
    default:
        std::ostringstream msg;
        msg << "unsupported rasqal graph pattern op " <<
                rasqal_graph_pattern_get_operator(gp);
        throw QueryParseException(msg.str());
    }
}

Expression* Query::convertExpression(rasqal_expression *expr)
            throw(QueryParseException) {
    switch(expr->op) {
    case RASQAL_EXPR_LITERAL:
    {
        rasqal_literal *lit = expr->literal;
        if(lit->type == RASQAL_LITERAL_VARIABLE) {
            return new VariableExpression(
                reinterpret_cast<Variable*>(lit->value.variable->user_data));
        } else {
            Value *val = new Value(lit);
            store->lookupId(*val);
            return new ValueExpression(this, val);
        }
    }
    case RASQAL_EXPR_BANG:
        return new BangExpression(convertExpression(expr->arg1));
    case RASQAL_EXPR_UMINUS:
        return new UMinusExpression(convertExpression(expr->arg1));
    case RASQAL_EXPR_BOUND:
        if(expr->arg1->op != RASQAL_EXPR_LITERAL ||
           expr->arg1->literal->type != RASQAL_LITERAL_VARIABLE)
            throw QueryParseException("BOUND expression expects a variable");
        return new BoundExpression((Variable*)
                        expr->arg1->literal->value.variable->user_data);
    case RASQAL_EXPR_ISURI:
        return new IsIRIExpression(convertExpression(expr->arg1));
    case RASQAL_EXPR_ISBLANK:
        return new IsBlankExpression(convertExpression(expr->arg1));
    case RASQAL_EXPR_ISLITERAL:
        return new IsLiteralExpression(convertExpression(expr->arg1));
    case RASQAL_EXPR_STR:
        return new StrExpression(convertExpression(expr->arg1));
    case RASQAL_EXPR_LANG:
        return new LangExpression(convertExpression(expr->arg1));
    case RASQAL_EXPR_DATATYPE:
        return new DatatypeExpression(convertExpression(expr->arg1));

    /* The following cases have potential memory leaks when an exception occurs
     * in a recursive call of convertExpression after the first subexpression
     * has already been created. This only happens if rasqal produces an
     * incorrect SPARQL expression, which should not happen.
     */
    case RASQAL_EXPR_OR:
        return new OrExpression(convertExpression(expr->arg1),
                                convertExpression(expr->arg2));
    case RASQAL_EXPR_AND:
        return new AndExpression(convertExpression(expr->arg1),
                                 convertExpression(expr->arg2));
    case RASQAL_EXPR_EQ:
        return new EqExpression(convertExpression(expr->arg1),
                                convertExpression(expr->arg2));
    case RASQAL_EXPR_NEQ:
        return new NEqExpression(convertExpression(expr->arg1),
                                 convertExpression(expr->arg2));
    case RASQAL_EXPR_LT:
        return new LTExpression(convertExpression(expr->arg1),
                                convertExpression(expr->arg2));
    case RASQAL_EXPR_GT:
        return new GTExpression(convertExpression(expr->arg1),
                                convertExpression(expr->arg2));
    case RASQAL_EXPR_LE:
        return new LEExpression(convertExpression(expr->arg1),
                                convertExpression(expr->arg2));
    case RASQAL_EXPR_GE:
        return new GEExpression(convertExpression(expr->arg1),
                                convertExpression(expr->arg2));
    case RASQAL_EXPR_STAR:
        return new StarExpression(convertExpression(expr->arg1),
                                  convertExpression(expr->arg2));
    case RASQAL_EXPR_SLASH:
        return new SlashExpression(convertExpression(expr->arg1),
                                   convertExpression(expr->arg2));
    case RASQAL_EXPR_PLUS:
        return new PlusExpression(convertExpression(expr->arg1),
                                  convertExpression(expr->arg2));
    case RASQAL_EXPR_MINUS:
        return new MinusExpression(convertExpression(expr->arg1),
                                   convertExpression(expr->arg2));
    case RASQAL_EXPR_SAMETERM:
        return new SameTermExpression(convertExpression(expr->arg1),
                                      convertExpression(expr->arg2));
    case RASQAL_EXPR_LANGMATCHES:
        return new LangMatchesExpression(convertExpression(expr->arg1),
                                         convertExpression(expr->arg2));
    case RASQAL_EXPR_REGEX:
        return new RegExExpression(convertExpression(expr->arg1),
                                   convertExpression(expr->arg2),
                                   convertExpression(expr->arg3));
    default:
        std::ostringstream msg;
        msg << "unsupported rasqal expression op " << expr->op;
        throw QueryParseException(msg.str());
    }
}

char* Query::convertURI(raptor_uri *uri) {
    char *str = reinterpret_cast<char*>(raptor_uri_as_string(uri));
    char *result = new char[strlen(str) + 1];
    strcpy(result, str);
    return result;
}

VarVal Query::getVarVal(rasqal_literal* literal) throw(QueryParseException) {
    if(literal->type == RASQAL_LITERAL_VARIABLE) {
        return VarVal((Variable*) literal->value.variable->user_data);
    } else {
        Value val(literal);
        store->lookupId(val);
        return VarVal(val);
    }
}

std::ostream& operator<<(std::ostream &out, const Query &q) {
    out << *q.getPattern();
    return out;
}

////////////////////////////////////////////////////////////////////////////////
// Search

bool Query::next() {
    if(solutions == NULL) {
        if(limit >= 0 && nbSols >= static_cast<unsigned>(limit))
            return false;
        if(nbSols == 0) {
            for(unsigned i = 0; i < offset; i++) {
                if(!nextPatternSolution())
                    return false;
            }
        }
        if(!nextPatternSolution())
            return false;
        nbSols++;
        return true;
    } else {
        if(nbSols == 0) {
            while(nextPatternSolution()) {
                solutions->insert(new Solution(this));
                if(limit >= 0) {
                    unsigned n = solutions->size();
                    if(n > static_cast<unsigned>(limit) + offset) {
                        // only keep the first (offset + limit) solutions
                        SolutionSet::iterator it = solutions->end();
                        --it;
                        delete *it;
                        solutions->erase(it);
                        n--;
                    }
                    if(n == static_cast<unsigned>(limit) + offset)
                        bnbOrderCstr->updateBound(*solutions->rbegin());
                }
            }
            it = solutions->begin();
            for(unsigned i = 0; i < offset && it != solutions->end(); i++)
                ++it;
        }
        if(it == solutions->end())
            return false;
        Solution *sol = *it;
        ++it;
        nbSols++;
        sol->restore();
        return true;
    }
}

bool Query::nextPatternSolution() {
    if(!pattern->next())
        return false;
    for(unsigned i = 0; i < nbVars; i++)
        vars[i].setValueFromCP();
    if(distinctCstr != NULL)
        distinctCstr->addSolution();
    return true;
}

void Query::reset() {
    pattern->discard();
    nbSols = 0;
    if(distinctCstr != NULL)
        distinctCstr->reset();
    if(solutions != NULL) {
        for(Solution* sol : *solutions)
            delete sol;
        solutions->clear();
    }
}

}
