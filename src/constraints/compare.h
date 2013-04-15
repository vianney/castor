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
#ifndef CASTOR_CONSTRAINTS_COMPARE_H
#define CASTOR_CONSTRAINTS_COMPARE_H

#include "solver/constraint.h"
#include "query.h"

namespace castor {

/**
 * Equality constraint: x1 = x2 <=> b.
 * If x1 != x2, b is RDF_ERROR if both x1 and x2 are literals of different
 * categories (or of categories Value::CAT_PLAIN_LANG or Value::CAT_OTHER).
 * Otherwise, b is RDF_FALSE.
 */
class VarEqConstraint : public cp::Constraint, cp::TrailListener {
public:
    VarEqConstraint(Query* query, cp::RDFVar* x1, cp::RDFVar* x2,
                    cp::TriStateVar* b);
    void restored(cp::Trailable* obj) override;
    bool post() override;
    bool propagate() override;

private:
    Store* store_;
    cp::RDFVar* x1_;
    cp::RDFVar* x2_;
    cp::TriStateVar* b_;
    unsigned s1_, s2_; //!< previous size of the domain
};

/**
 * Inequality constraint: x1 {<,<=} x2 <=> b.
 * If x1 and x2 are not comparable, b is RDF_ERROR.
 */
class VarLessConstraint : public cp::Constraint {
public:
    VarLessConstraint(Query* query, cp::RDFVar* x1, cp::RDFVar* x2,
                      cp::TriStateVar* b, bool equality);
    bool propagate() override;

private:
    Store* store_;
    cp::RDFVar* x1_;
    cp::RDFVar* x2_;
    cp::TriStateVar* b_;
    bool equality_; //!< true for <=, false for <
};

/**
 * Equality in sameTerm sense: sameTerm(x1, x2) <=> b.
 * No type error may occur, so b is never RDF_ERROR.
 */
class VarSameTermConstraint : public cp::Constraint, cp::TrailListener {
public:
    VarSameTermConstraint(Query* query, cp::RDFVar* x1, cp::RDFVar* x2,
                          cp::TriStateVar* b);
    void restored(cp::Trailable* obj) override;
    bool post() override;
    bool propagate() override;

private:
    cp::RDFVar* x1_;
    cp::RDFVar* x2_;
    cp::TriStateVar* b_;
    unsigned s1_, s2_; //!< previous size of the domain
};

}

#endif // CASTOR_CONSTRAINTS_COMPARE_H
