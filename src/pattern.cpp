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
#include "pattern.h"

#include "config.h"
#include "query.h"
#include "constraints/unary.h"
#include "constraints/triple.h"

namespace castor {

std::ostream& operator<<(std::ostream& out, const Pattern& p) {
    p.print(out);
    return out;
}

BasicPattern::BasicPattern(Query* query) :
    Pattern(query), sub_(query->solver()) {}

BasicPattern::~BasicPattern() {
    for(unsigned i = 0; i < triples_.size(); i++) {
        for(int j = 0; j < TriplePattern::COMPONENTS; j++) {
            if(!triples_[i][j].isVariable())
                delete cptriples_[i][j];
        }
    }
}

void BasicPattern::add(const TriplePattern& triple) {
    triples_.push_back(triple);
    RDFVarTriple cptriple;
    for(int i = 0; i < triple.COMPONENTS; i++) {
        VarVal v = triple[i];
        if(v.isVariable()) {
            Variable* x = query_->variable(v);
            vars_ += x;
            cvars_ += x;
            cptriple[i] = x->cp();
        } else {
            cptriple[i] = new cp::RDFVar(query_->solver(),
                                         v.valueId(), v.valueId());
        }
    }
    cptriples_.push_back(cptriple);
}

void BasicPattern::init() {
    for(Variable* x : vars_) {
        sub_.add(x->cp());
        sub_.add(new BoundConstraint(query_, x->cp()));
    }
    for(RDFVarTriple& t : cptriples_)
        sub_.add(new TripleConstraint(query_, t));
}

bool BasicPattern::next() {
    if(!sub_.isActive())
        sub_.activate();
    else if(!sub_.isCurrent())
        return true; // another BGP is posted further down
    return sub_.search();
}

void BasicPattern::discard() {
    if(sub_.isActive())
        sub_.discard();
}

FilterPattern::FilterPattern(Pattern* subpattern, Expression* condition) :
        Pattern(subpattern->query()),
        subpattern_(subpattern), condition_(condition) {
    vars_ = subpattern->variables();
    cvars_ = subpattern->certainVars();
}

FilterPattern::~FilterPattern() {
    delete subpattern_;
    delete condition_;
}

/**
 * @param expr a SPARQL expression
 * @param pat the LeftJoin subpattern
 * @return true if expr is of the form !BOUND(?x) where ?x in right and
 *         not in left
 */
bool isNotBound(Expression* expr, LeftJoinPattern* pat) {
    // TODO: recursive check + handle other parts of the condition
    if(BangExpression* bang = dynamic_cast<BangExpression*>(expr)) {
        if(BoundExpression* bound =
                    dynamic_cast<BoundExpression*>(bang->argument())) {
            return pat->right()->certainVars().contains(bound->variable()) &&
                  !pat->left() ->variables()  .contains(bound->variable());
        }
    }
    return false;
}

Pattern* FilterPattern::optimize() {
    subpattern_ = subpattern_->optimize();
#ifndef CASTOR_NOFILTERS
    LeftJoinPattern* subpat = dynamic_cast<LeftJoinPattern*>(subpattern_);
    if(subpat && isNotBound(condition_, subpat)) {
        DiffPattern* pat = new DiffPattern(std::move(*subpat));
        delete this;
        return pat;
    }
#endif
    return this;
}

void FilterPattern::init() {
    subpattern_->init();
#ifndef CASTOR_NOFILTERS
    if(BasicPattern* subpat = dynamic_cast<BasicPattern*>(subpattern_))
        condition_->post(subpat->sub_);
#endif
}

bool FilterPattern::next() {
#ifndef CASTOR_NOFILTERS
    if(dynamic_cast<BasicPattern*>(subpattern_)) {
        return subpattern_->next();
    } else {
#endif
        while(subpattern_->next()) {
            for(Variable* x : condition_->variables())
                x->setFromCP();
            if(condition_->isTrue())
                return true;
        }
        return false;
#ifndef CASTOR_NOFILTERS
    }
#endif
}

void FilterPattern::discard() {
    subpattern_->discard();
}

CompoundPattern::CompoundPattern(Pattern* left, Pattern* right) :
        Pattern(left->query()), left_(left), right_(right) {}

CompoundPattern::CompoundPattern(CompoundPattern&& o) : Pattern(o.query()) {
    left_    = o.left_;
    right_   = o.right_;
    o.left_  = nullptr;
    o.right_ = nullptr;
}

CompoundPattern::~CompoundPattern() {
    delete left_;
    delete right_;
}

Pattern* CompoundPattern::optimize() {
    left_ = left_->optimize();
    right_ = right_->optimize();
    return this;
}

void CompoundPattern::init() {
    left_->init();
    right_->init();
}

void JoinPattern::initialize() {
    vars_ = left_->variables();
    vars_ += right_->variables();
    cvars_ = left_->certainVars();
    cvars_ += right_->certainVars();
}

bool JoinPattern::next() {
    while(left_->next())
        if(right_->next())
            return true;
    return false;
}

void JoinPattern::discard() {
    right_->discard();
    left_->discard();
}

void LeftJoinPattern::initialize() {
    vars_ = left_->variables();
    vars_ += right_->variables();
    cvars_ = left_->certainVars();
    consistent_ = false;
}

bool LeftJoinPattern::next() {
    while(left_->next()) {
        if(right_->next()) {
            consistent_ = true;
            return true;
        } else if(!consistent_) {
            return true;
        } else {
            consistent_ = false;
        }
    }
    return false;
}

void LeftJoinPattern::discard() {
    right_->discard();
    left_->discard();
    consistent_ = false;
}

void DiffPattern::initialize() {
    vars_ = left_->variables();
    cvars_ = left_->certainVars();
}

bool DiffPattern::next() {
    while(left_->next()) {
        if(right_->next())
            right_->discard();
        else
            return true;
    }
    return false;
}

void DiffPattern::discard() {
    right_->discard();
    left_->discard();
}

void UnionPattern::initialize() {
    vars_ = left_->variables();
    vars_ += right_->variables();
    cvars_ = left_->certainVars() * right_->certainVars();
    onRightBranch_ = false;
}

bool UnionPattern::next() {
    if(!onRightBranch_ && left_->next())
        return true;
    onRightBranch_ = true;
    if(right_->next())
        return true;
    onRightBranch_ = false;
    return false;
}

void UnionPattern::discard() {
    if(onRightBranch_)
        right_->discard();
    else
        left_->discard();
    onRightBranch_ = false;
}

}
