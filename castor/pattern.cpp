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
#include "pattern.h"
#include "constraints.h"

namespace castor {

BasicPattern::~BasicPattern() {
    if(sub)
        delete sub;
}

void BasicPattern::add(const StatementPattern &triple) {
    triples.push_back(triple);
    if(triple.subject.isVariable())
        vars += query->getVariable(triple.subject.getVariableId());
    if(triple.predicate.isVariable())
        vars += query->getVariable(triple.predicate.getVariableId());
    if(triple.object.isVariable())
        vars += query->getVariable(triple.object.getVariableId());
}

void BasicPattern::init() {
    sub = new Subtree(query->getSolver(), vars.getCPVars(), vars.getSize());
    for(int i = 0; i < vars.getSize(); i++)
        sub->add(new BoundConstraint(vars[i]->getCPVariable()));
    for(std::vector<StatementPattern>::iterator it = triples.begin(),
        end = triples.end(); it != end; ++it)
        sub->add(new StatementConstraint(query, *it));
}

bool BasicPattern::next() {
    if(!sub->isActive())
        sub->activate();
    else if(!sub->isCurrent())
        return true; // another BGP is posted further down
    return sub->search();
}

void BasicPattern::discard() {
    if(sub->isActive())
        sub->discard();
}

FilterPattern::FilterPattern(Pattern *subpattern, Expression *condition) :
        Pattern(subpattern->getQuery(), PATTERN_TYPE_FILTER),
        subpattern(subpattern), condition(condition) {
    vars = subpattern->getVars();
}

FilterPattern::~FilterPattern() {
    delete subpattern;
    delete condition;
}

void FilterPattern::init() {
    subpattern->init();
    if(subpattern->getType() == PATTERN_TYPE_BASIC)
        condition->post(*((BasicPattern*) subpattern)->sub);
}

bool FilterPattern::next() {
    if(subpattern->getType() == PATTERN_TYPE_BASIC) {
        return subpattern->next();
    } else {
        while(subpattern->next()) {
            for(int i = 0; i < condition->getVars().getSize(); i++)
                condition->getVars()[i]->setValueFromCP();
            if(condition->isTrue())
                return true;
        }
        return false;
    }
}

void FilterPattern::discard() {
    subpattern->discard();
}

CompoundPattern::CompoundPattern(PatternType type, Pattern *left, Pattern *right) :
        Pattern(left->getQuery(), type), left(left), right(right) {
    vars = left->getVars();
    vars += right->getVars();
}

CompoundPattern::~CompoundPattern() {
    delete left;
    delete right;
}

void CompoundPattern::init() {
    left->init();
    right->init();
}

bool JoinPattern::next() {
    while(left->next())
        if(right->next())
            return true;
    return false;
}

void JoinPattern::discard() {
    right->discard();
    left->discard();
}

bool LeftJoinPattern::next() {
    while(left->next()) {
        if(right->next()) {
            consistent = true;
            return true;
        } else if(!consistent) {
            return true;
        } else {
            consistent = false;
        }
    }
    return false;
}

void LeftJoinPattern::discard() {
    right->discard();
    left->discard();
    consistent = false;
}

bool DiffPattern::next() {
    while(left->next()) {
        if(right->next())
            right->discard();
        else
            return true;
    }
    return false;
}

void DiffPattern::discard() {
    right->discard();
    left->discard();
}

bool UnionPattern::next() {
    if(!onRightBranch && left->next())
        return true;
    onRightBranch = true;
    if(right->next())
        return true;
    onRightBranch = false;
    return false;
}

void UnionPattern::discard() {
    if(onRightBranch)
        right->discard();
    else
        left->discard();
    onRightBranch = false;
}

}
