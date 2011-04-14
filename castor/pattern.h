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

namespace castor { class Pattern; }

#include "query.h"
#include "expression.h"
#include "solver/solver.h"

namespace castor {

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
 * Pattern type enumeration
 */
enum PatternType {
    /**
     * Dummy pattern always resulting in an empty set of solutions, i.e., it
     * cannot be matched
     */
    PATTERN_TYPE_FALSE,
    /**
     * Basic graph pattern (set of statement patterns)
     */
    PATTERN_TYPE_BASIC,
    /**
     * Filter expression
     */
    PATTERN_TYPE_FILTER,
    /**
     * Concatenation
     */
    PATTERN_TYPE_JOIN,
    /**
     * OPTIONAL
     */
    PATTERN_TYPE_LEFTJOIN,
    /**
     * OPTIONAL { ... } FILTER(!bound(...))
     */
    PATTERN_TYPE_DIFF,
    /**
     * UNION
     */
    PATTERN_TYPE_UNION
};

/**
 * Base class for a SPARQL graph pattern
 */
class Pattern {
public:
    Pattern(Query *query, PatternType type) :
            query(query), type(type), vars(query) {}
    virtual ~Pattern() {}

    /**
     * @return parent query
     */
    Query* getQuery() { return query; }

    /**
     * @return operator
     */
    PatternType getType() { return type; }

    /**
     * @return variables occuring in this pattern
     */
    VariableSet& getVars() { return vars; }

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

protected:
    /**
     * Parent query
     */
    Query *query;
    /**
     * Type
     */
    PatternType type;
    /**
     * Variables occuring in this pattern.
     */
    VariableSet vars;
};

/**
 * Dummy pattern always resulting in an empty set of solutions, i.e., it
 * cannot be matched
 */
class FalsePattern : public Pattern {
public:
    FalsePattern(Query *query) : Pattern(query, PATTERN_TYPE_FALSE) {}
    void init() {}
    bool next() { return false; }
    void discard() {}
};

/**
 * Basic graph pattern (set of statement patterns)
 */
class BasicPattern : public Pattern {
    std::vector<StatementPattern> triples;
    Subtree *sub;
public:
    BasicPattern(Query *query) : Pattern(query, PATTERN_TYPE_BASIC) {}
    ~BasicPattern();

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

    void init();
    bool next();
    void discard();
};

/**
 * Base class for a compound pattern
 */
class CompoundPattern : public Pattern {
protected:
    Pattern *left, *right;
public:
    CompoundPattern(PatternType type, Pattern *left, Pattern *right);
    ~CompoundPattern();

    /**
     * @return the left subpattern
     */
    Pattern* getLeft() { return left; }

    /**
     * @return the right subpattern
     */
    Pattern* getRight() { return right; }

    void init();
};

/**
 * Concatenation pattern
 */
class JoinPattern : public CompoundPattern {
public:
    JoinPattern(Pattern *left, Pattern *right) :
            CompoundPattern(PATTERN_TYPE_JOIN, left, right) {}
    bool next();
    void discard();
};

/**
 * Optional pattern
 */
class LeftJoinPattern : public CompoundPattern {
    bool consistent; //!< is the right branch consistent?
public:
    LeftJoinPattern(Pattern *left, Pattern *right) :
            CompoundPattern(PATTERN_TYPE_LEFTJOIN, left, right),
            consistent(false) {}
    bool next();
    void discard();
};

/**
 * OPTIONAL { ... } FILTER(!bound(...))
 */
class DiffPattern : public CompoundPattern {
public:
    DiffPattern(Pattern *left, Pattern *right) :
            CompoundPattern(PATTERN_TYPE_DIFF, left, right) {}
    bool next();
    void discard();
};

/**
 * Union pattern
 */
class UnionPattern : public CompoundPattern {
    bool onRightBranch; //!< exploring the left (false) or right (true) branch
public:
    UnionPattern(Pattern *left, Pattern *right) :
            CompoundPattern(PATTERN_TYPE_UNION, left, right),
            onRightBranch(false) {}
    bool next();
    void discard();
};

}

#endif // CASTOR_PATTERN_H
