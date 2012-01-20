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
 * Statement pattern.
 */
struct StatementPattern {
    VarVal subject;
    VarVal predicate;
    VarVal object;

    StatementPattern(VarVal subject, VarVal predicate, VarVal object) :
            subject(subject), predicate(predicate), object(object) {}
};

/**
 * Base class for a SPARQL graph pattern
 */
class Pattern {
public:
    Pattern(Query *query) : query(query), vars(query), cvars(query) {}
    virtual ~Pattern() {}

    /**
     * @return parent query
     */
    Query* getQuery() { return query; }

    /**
     * @return variables occuring in this pattern
     */
    VariableSet& getVars() { return vars; }

    /**
     * @return certain variables
     */
    VariableSet& getCVars() { return cvars; }

    /**
     * Same effect as "delete this", but do not delete subpatterns.
     * This is useful if the subpatterns were reused for building another
     * pattern.
     */
    virtual void deleteThisOnly() { delete this; }

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
    void print(std::ostream &out) const { print(out, 0); }

    /**
     * For debugging purpose
     * @param indent indent level to add in front of the string representation
     */
    virtual void print(std::ostream &out, int indent) const = 0;

protected:
    /**
     * Parent query
     */
    Query *query;
    /**
     * Variables occuring in this pattern.
     */
    VariableSet vars;
    /**
     * Certain variables: variables we are sure will be assigned in this
     * pattern. Always a subset of vars.
     */
    VariableSet cvars;

    /**
     * For debugging purpose, return a string of whitespace corresponding to the
     * indent.
     * @param indent indent level
     */
    std::string ws(int indent) const { return std::string(2*indent, ' '); }
};

std::ostream& operator<<(std::ostream &out, const Pattern &p);

/**
 * Dummy pattern always resulting in an empty set of solutions, i.e., it
 * cannot be matched
 */
class FalsePattern : public Pattern {
public:
    FalsePattern(Query *query) : Pattern(query) {}
    void init() {}
    bool next() { return false; }
    void discard() {}
    void print(std::ostream &out, int indent) const {
        out << ws(indent) << "FalsePattern";
    }
};

/**
 * Basic graph pattern (set of statement patterns)
 */
class BasicPattern : public Pattern {
    std::vector<StatementPattern> triples;
    cp::Subtree sub;
public:
    BasicPattern(Query *query);

    /**
     * Add a triple pattern
     * @param[in] triple the triple pattern
     */
    void add(const StatementPattern &triple);

    /**
     * @return triple patterns
     */
    std::vector<StatementPattern>& getTriples() { return triples; }

    void init();
    bool next();
    void discard();

    void print(std::ostream &out, int indent) const {
        out << ws(indent) << "BasicPattern(" << triples.size() << " triples)";
    }

    friend class FilterPattern;
};

/**
 * Filter pattern
 */
class FilterPattern : public Pattern {
    Pattern *subpattern;
    Expression *condition;
public:
    FilterPattern(Pattern *subpattern, Expression *condition);
    ~FilterPattern();

    /**
     * @return the subpattern
     */
    Pattern* getSubPattern() { return subpattern; }

    /**
     * @return the filter condition
     */
    Expression* getCondition() { return condition; }

    void deleteThisOnly() { subpattern = NULL; condition = NULL; delete this; }

    Pattern* optimize();
    void init();
    bool next();
    void discard();

    void print(std::ostream &out, int indent) const {
        out << ws(indent) << "FilterPattern("
            << vars.getSize() << " variables)" << std::endl;
        subpattern->print(out, indent+1);
    }
};

/**
 * Base class for a compound pattern
 */
class CompoundPattern : public Pattern {
protected:
    Pattern *left, *right;
public:
    CompoundPattern(Pattern *left, Pattern *right)
        : Pattern(left->getQuery()), left(left), right(right) {}
    ~CompoundPattern();

    /**
     * @return the left subpattern
     */
    Pattern* getLeft() { return left; }

    /**
     * @return the right subpattern
     */
    Pattern* getRight() { return right; }

    void deleteThisOnly() { left = NULL; right = NULL; delete this; }

    Pattern* optimize();
    void init();

    void print(std::ostream &out, int indent) const {
        out << ws(indent) << typeid(*this).name() << std::endl;
        left->print(out, indent+1); out << std::endl;
        right->print(out, indent+1);
    }
};

/**
 * Concatenation pattern
 */
class JoinPattern : public CompoundPattern {
public:
    JoinPattern(Pattern *left, Pattern *right);
    bool next();
    void discard();
};

/**
 * Optional pattern
 */
class LeftJoinPattern : public CompoundPattern {
    bool consistent; //!< is the right branch consistent?
public:
    LeftJoinPattern(Pattern *left, Pattern *right);
    bool next();
    void discard();
};

/**
 * OPTIONAL { ... } FILTER(!bound(...))
 */
class DiffPattern : public CompoundPattern {
public:
    DiffPattern(Pattern *left, Pattern *right);
    bool next();
    void discard();
};

/**
 * Union pattern
 */
class UnionPattern : public CompoundPattern {
    bool onRightBranch; //!< exploring the left (false) or right (true) branch
public:
    UnionPattern(Pattern *left, Pattern *right);
    bool next();
    void discard();
};

}

#endif // CASTOR_PATTERN_H
