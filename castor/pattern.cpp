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

std::ostream& operator<<(std::ostream &out, const Pattern &p) {
    p.print(out);
    return out;
}

BasicPattern::~BasicPattern() {
    if(sub)
        delete sub;
}

void BasicPattern::add(const StatementPattern &triple) {
    triples.push_back(triple);
    if(triple.subject.isVariable()) {
        vars += query->getVariable(triple.subject.getVariableId());
        cvars += query->getVariable(triple.subject.getVariableId());
    }
    if(triple.predicate.isVariable()) {
        vars += query->getVariable(triple.predicate.getVariableId());
        cvars += query->getVariable(triple.predicate.getVariableId());
    }
    if(triple.object.isVariable()) {
        vars += query->getVariable(triple.object.getVariableId());
        cvars += query->getVariable(triple.object.getVariableId());
    }
}

void BasicPattern::init() {
    sub = new Subtree(query->getSolver(), vars.getCPVars(), vars.getSize());
    for(unsigned i = 0; i < vars.getSize(); i++)
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
        Pattern(subpattern->getQuery()),
        subpattern(subpattern), condition(condition) {
    vars = subpattern->getVars();
    cvars = subpattern->getCVars();
}

FilterPattern::~FilterPattern() {
    if(subpattern)
        delete subpattern;
    if(condition)
        delete condition;
}

/**
 * @param expr a SPARQL expression
 * @param pat the LeftJoin subpattern
 * @return true if expr is of the form !BOUND(?x) where ?x in right and
 *         not in left
 */
bool isNotBound(Expression *expr, LeftJoinPattern *pat) {
// TODO recursive check + handle other parts of the condition
    if(BangExpression *bang = dynamic_cast<BangExpression*>(expr)) {
        if(BoundExpression *bound =
                dynamic_cast<BoundExpression*>(bang->getArgument())) {
            return pat->getRight()->getCVars().contains(bound->getVariable()) &&
                    !pat->getLeft()->getVars().contains(bound->getVariable());
        }
    }
    return false;
}

Pattern* FilterPattern::optimize() {
    subpattern = subpattern->optimize();
    LeftJoinPattern *subpat = dynamic_cast<LeftJoinPattern*>(subpattern);
    if(subpat && isNotBound(condition, subpat)) {
        DiffPattern *pat = new DiffPattern(subpat->getLeft(),
                                           subpat->getRight());
        subpat->deleteThisOnly();
        subpattern = NULL;
        delete this;
        return pat;
    }
    return this;
}

void FilterPattern::init() {
    subpattern->init();
    if(BasicPattern *subpat = dynamic_cast<BasicPattern*>(subpattern))
        condition->post(*subpat->sub);
}

bool FilterPattern::next() {
    if(dynamic_cast<BasicPattern*>(subpattern)) {
        return subpattern->next();
    } else {
        while(subpattern->next()) {
            for(unsigned i = 0; i < condition->getVars().getSize(); i++)
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

CompoundPattern::~CompoundPattern() {
    if(left)
        delete left;
    if(right)
        delete right;
}

Pattern* CompoundPattern::optimize() {
    left = left->optimize();
    right = right->optimize();
    return this;
}

void CompoundPattern::init() {
    left->init();
    right->init();
}

JoinPattern::JoinPattern(Pattern *left, Pattern *right)
        : CompoundPattern(left, right) {
    vars = left->getVars();
    vars += right->getVars();
    cvars = left->getCVars();
    cvars += right->getCVars();
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

LeftJoinPattern::LeftJoinPattern(Pattern *left, Pattern *right)
        : CompoundPattern(left, right), consistent(false) {
    vars = left->getVars();
    vars += right->getVars();
    cvars = left->getCVars();
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

DiffPattern::DiffPattern(Pattern *left, Pattern *right)
        : CompoundPattern(left, right) {
    vars = left->getVars();
    cvars = left->getCVars();
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

UnionPattern::UnionPattern(Pattern *left, Pattern *right)
        : CompoundPattern(left, right), onRightBranch(false) {
    vars = left->getVars();
    vars += right->getVars();
    cvars = left->getCVars() * right->getCVars();
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
