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
#ifndef CASTOR_CONSTRAINTS_COMPARE_H
#define CASTOR_CONSTRAINTS_COMPARE_H

#include "solver/constraint.h"
#include "query.h"

namespace castor {

/**
 * Variables must take values of the same classes.
 */
class SameClassConstraint : public cp::StatelessConstraint {
public:
    SameClassConstraint(Query* query, cp::RDFVar* x1, cp::RDFVar* x2);
    bool propagate();

private:
    Store* store_;
    cp::RDFVar* x1_;
    cp::RDFVar* x2_;
};

/**
 * Variable difference constraint x1 != x2
 */
class VarDiffConstraint : public cp::StatelessConstraint {
public:
    VarDiffConstraint(Query* query, cp::RDFVar* x1, cp::RDFVar* x2);
    bool propagate();

private:
    Store* store_;
    cp::RDFVar* x1_;
    cp::RDFVar* x2_;
};

/**
 * Variable equality constraint x1 = x2
 */
class VarEqConstraint : public cp::Constraint, cp::TrailListener {
public:
    VarEqConstraint(Query* query, cp::RDFVar* x1, cp::RDFVar* x2);
    void restored(cp::Trailable* obj);
    bool post();
    bool propagate();

private:
    Store* store_;
    cp::RDFVar* x1_;
    cp::RDFVar* x2_;
    unsigned s1_, s2_; //!< previous size of the domain
};

/**
 * Variable inequality constraint x1 {<,<=} x2
 */
class VarLessConstraint : public cp::StatelessConstraint {
public:
    VarLessConstraint(Query* query, cp::RDFVar* x1, cp::RDFVar* x2,
                      bool equality);
    bool propagate();

private:
    Store* store_;
    cp::RDFVar* x1_;
    cp::RDFVar* x2_;
    bool equality; //!< true for <=, false for <
};

/**
 * Variable difference in sameTerm sense
 */
class VarDiffTermConstraint : public cp::StatelessConstraint {
public:
    VarDiffTermConstraint(Query* query, cp::RDFVar* x1, cp::RDFVar* x2);
    bool propagate();

private:
    cp::RDFVar* x1_;
    cp::RDFVar* x2_;
};

/**
 * Variable equality in sameTerm sense
 */
class VarSameTermConstraint : public cp::Constraint, cp::TrailListener {
public:
    VarSameTermConstraint(Query* query, cp::RDFVar* x1, cp::RDFVar* x2);
    void restored(cp::Trailable* obj);
    bool post();
    bool propagate();

private:
    cp::RDFVar* x1_;
    cp::RDFVar* x2_;
    unsigned s1_, s2_; //!< previous size of the domain
};

}

#endif // CASTOR_CONSTRAINTS_COMPARE_H
