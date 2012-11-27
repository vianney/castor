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
#ifndef CASTOR_PATTERN_H
#define CASTOR_PATTERN_H

#include <string>
#include <iostream>
#include <typeinfo>

#include "solver/subtree.h"
#include "variable.h"
#include "expression.h"

namespace castor {

class Query;

/**
 * Base class for a SPARQL graph pattern
 */
class Pattern {
public:
    Pattern(Query* query) : query_(query), vars_(query), cvars_(query) {}
    virtual ~Pattern() {}

    //! Non-copyable
    Pattern(const Pattern&) = delete;
    Pattern& operator=(const Pattern&) = delete;

    /**
     * @return parent query
     */
    Query* query() { return query_; }

    /**
     * @return variables occuring in this pattern
     */
    const VariableSet& variables() { return vars_; }

    /**
     * @return certain variables
     */
    const VariableSet& certainVars() { return cvars_; }

    /**
     * Optimize this pattern and its subpatterns. Beware: the patterns may
     * change. You shouldn't use this object anymore, but instead use the
     * returned pointer (which may or may not be the same as this).
     *
     * @return the optimized pattern
     */
    virtual Pattern* optimize() { return this; }

    /**
     * Initialize subtree recursively.
     */
    virtual void init() = 0;

    /**
     * Find the next solution of the subtree
     * @return false if there are no more solution, true otherwise
     */
    virtual bool next() = 0;

    /**
     * Discard the rest of the subtree's solutions.
     */
    virtual void discard() = 0;

    /**
     * For debugging purpose
     */
    void print(std::ostream& out) const { print(out, 0); }

    /**
     * For debugging purpose
     * @param out output stream
     * @param indent indent level to add in front of the string representation
     */
    virtual void print(std::ostream& out, int indent) const = 0;

protected:
    /**
     * Parent query
     */
    Query* query_;
    /**
     * Variables occuring in this pattern.
     */
    VariableSet vars_;
    /**
     * Certain variables: variables we are sure will be assigned in this
     * pattern. Always a subset of vars.
     */
    VariableSet cvars_;

    /**
     * For debugging purpose, return a string of whitespace corresponding to the
     * indent.
     * @param indent indent level
     */
    std::string ws(int indent) const { return std::string(2*indent, ' '); }
};

std::ostream& operator<<(std::ostream& out, const Pattern& p);

/**
 * Dummy pattern always resulting in an empty set of solutions, i.e., it
 * cannot be matched
 */
class FalsePattern : public Pattern {
public:
    FalsePattern(Query* query) : Pattern(query) {}
    void init() {}
    bool next() { return false; }
    void discard() {}
    void print(std::ostream& out, int indent) const {
        out << ws(indent) << "FalsePattern";
    }
};

typedef BasicTriple<VarVal> TriplePattern;

/**
 * Basic graph pattern (set of triple patterns)
 */
class BasicPattern : public Pattern {
public:
    BasicPattern(Query* query);

    /**
     * Add a triple pattern
     * @param[in] triple the triple pattern
     */
    void add(const TriplePattern& triple);

    /**
     * @return triple patterns
     */
    std::vector<TriplePattern>& triples() { return triples_; }

    void init();
    bool next();
    void discard();

    void print(std::ostream& out, int indent) const {
        out << ws(indent) << "BasicPattern(" << triples_.size() << " triples)";
    }

private:
    std::vector<TriplePattern> triples_;
    cp::Subtree                sub_;

    friend class FilterPattern;
};

/**
 * Filter pattern
 */
class FilterPattern : public Pattern {
public:
    FilterPattern(Pattern* subpattern, Expression* condition);
    ~FilterPattern();

    /**
     * @return the subpattern
     */
    Pattern* subPattern() { return subpattern_; }

    /**
     * @return the filter condition
     */
    Expression* condition() { return condition_; }

    Pattern* optimize();
    void init();
    bool next();
    void discard();

    void print(std::ostream& out, int indent) const {
        out << ws(indent) << "FilterPattern("
            << vars_.size() << " variables)" << std::endl;
        subpattern_->print(out, indent+1);
    }

private:
    Pattern*    subpattern_;
    Expression* condition_;
};

/**
 * Base class for a compound pattern
 */
class CompoundPattern : public Pattern {
public:
    CompoundPattern(Pattern* left, Pattern* right);
    //! Move constructor
    CompoundPattern(CompoundPattern&& o);
    ~CompoundPattern();

    /**
     * @return the left subpattern
     */
    Pattern* left() { return left_; }

    /**
     * @return the right subpattern
     */
    Pattern* right() { return right_; }

    Pattern* optimize();
    void init();

    void print(std::ostream& out, int indent) const {
        out << ws(indent) << typeid(*this).name() << std::endl;
        left_->print(out, indent+1); out << std::endl;
        right_->print(out, indent+1);
    }

protected:
    Pattern* left_;
    Pattern* right_;
};

/**
 * Concatenation pattern
 */
class JoinPattern : public CompoundPattern {
public:
    JoinPattern(Pattern* left, Pattern* right) : CompoundPattern(left, right) {
        initialize();
    }
    JoinPattern(CompoundPattern&& o) : CompoundPattern(std::move(o)) {
        initialize();
    }
    bool next();
    void discard();

protected:
    void initialize();
};

/**
 * Optional pattern
 */
class LeftJoinPattern : public CompoundPattern {
public:
    LeftJoinPattern(Pattern* left, Pattern* right) : CompoundPattern(left, right) {
        initialize();
    }
    LeftJoinPattern(CompoundPattern&& o) : CompoundPattern(std::move(o)) {
        initialize();
    }
    bool next();
    void discard();

protected:
    void initialize();

private:
    bool consistent_; //!< is the right branch consistent?
};

/**
 * OPTIONAL { ... } FILTER(!bound(...))
 */
class DiffPattern : public CompoundPattern {
public:
    DiffPattern(Pattern* left, Pattern* right) : CompoundPattern(left, right) {
        initialize();
    }
    DiffPattern(CompoundPattern&& o) : CompoundPattern(std::move(o)) {
        initialize();
    }
    bool next();
    void discard();

protected:
    void initialize();
};

/**
 * Union pattern
 */
class UnionPattern : public CompoundPattern {
public:
    UnionPattern(Pattern* left, Pattern* right) : CompoundPattern(left, right) {
        initialize();
    }
    UnionPattern(CompoundPattern&& o) : CompoundPattern(std::move(o)) {
        initialize();
    }
    bool next();
    void discard();

protected:
    void initialize();

private:
    bool onRightBranch_; //!< exploring the left (false) or right (true) branch
};

}

#endif // CASTOR_PATTERN_H
