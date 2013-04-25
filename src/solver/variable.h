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
#ifndef CASTOR_CP_VARIABLE_H
#define CASTOR_CP_VARIABLE_H

#include "trail.h"

namespace castor {
namespace cp {

/**
 * Base class for a CP decision variable. A decision variable is a trailable
 * object which can be labeled. Labeling implies that the variable is discrete.
 */
class DecisionVariable : public virtual Trailable {
public:
    /**
     * @return the current size of the domain of this variable
     */
    unsigned size() const { return size_; }

    /**
     * @return the degree, i.e., the number of registered constraints
     */
    unsigned degree() const { return degree_; }

    /**
     * @return the dynamic degree, i.e., the number of non-entailed registered
     *         constraints
     */
    virtual unsigned dyndegree() const = 0;

    /**
     * @return whether this variable is bound
     */
    bool bound() const { return size_ == 1; }

    /**
     * Bind this variable to a value of its domain. This is called during the
     * labeling phase of the search. Appropriate events should be triggered.
     *
     * @pre !bound()
     * @post if return value is true => bound()
     * @return false if the domain is empty, true otherwise
     */
    virtual bool label() = 0;

    /**
     * Remove from the domain the value that would be assigned to this variable
     * by label(). It is called after backtracking.
     *
     * @pre !bound()
     * @return false if the domain is empty, true otherwise
     */
    virtual bool unlabel() = 0;

protected:
    /**
     * Default constructor with bogus values for Trailable. As Trailable is
     * a virtual base class, subclasses of DecisionVariable would have to
     * initialize Trailable by themselves anyway.
     */
    DecisionVariable() : Trailable(nullptr), degree_(0) {}

    /**
     * Current size of the domain. Must be updated by the subclass.
     */
    unsigned size_;

    /**
     * Number of constraints registered to this variable.
     * Must be updated by the subclass.
     */
    unsigned degree_;
};

}
}

#endif // CASTOR_CP_VARIABLE_H
