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
#ifndef CASTOR_CP_BOUNDSVAR_H
#define CASTOR_CP_BOUNDSVAR_H

#include <vector>
#include <cassert>
#include <iostream>

#include "solver.h"
#include "variable.h"

namespace castor {
namespace cp {

class Constraint;

/**
 * Variable with only bounds representation. Values between the two bounds
 * are assumed to be in the domain.
 *
 * @param T the type of the values (should be an integer type)
 */
template<class T>
class BoundsVariable : public Variable {
public:
    /**
     * Construct a variable with domain min..max.
     *
     * @param solver attached solver
     * @param min initial lower bound
     * @param max initial upper bound
     */
    BoundsVariable(Solver *solver, T min, T max);

    // Implementation of virtual functions
    void checkpoint(void *trail) const;
    void restore(const void *trail);
    void select();
    void unselect();

    /**
     * @pre isBound() == true
     * @return the value bound to this variable
     */
    T getValue() const { return min; }

    /**
     * @param v a value
     * @return whether value v is in the domain
     */
    bool contains(T v) const { return v >= min && v <= max; }

    /**
     * @return the lower bound
     */
    T getMin() const { return min; }
    /**
     * @return the upper bound
     */
    T getMax() const { return max; }

    /**
     * Bind a value to a variable.
     *
     * @note Should only be called during constraint propagation.
     *
     * @param v the value
     * @return false if the domain becomes empty, true otherwise
     */
    bool bind(T v);

    /**
     * Remove all values < v from the domain of the variable.
     *
     * @note Should only be called during constraint propagation.
     *
     * @param v the new lower bound
     * @return false if the domain becomes empty, true otherwise
     */
    bool updateMin(T v);

    /**
     * Remove all values > v from the domain of the variable.
     *
     * @note Should only be called during constraint propagation.
     *
     * @param v the new upper bound
     * @return false if the domain becomes empty, true otherwise
     */
    bool updateMax(T v);

    /**
     * Register constraint c to the bind event of this variable. A constraint
     * must not register twice for the same variable.
     *
     * @param c the constraint
     */
    void registerBind(Constraint *c) { evBind.push_back(c); }

    /**
     * Register constraint c to the update min event of this variable.
     * A constraint must not register twice for the same variable.
     *
     * @param c the constraint
     */
    void registerMin(Constraint *c) { evMin.push_back(c); }

    /**
     * Register constraint c to the update max event of this variable.
     * A constraint must not register twice for the same variable.
     *
     * @param c the constraint
     */
    void registerMax(Constraint *c) { evMax.push_back(c); }

private:
    /**
     * Lower and upper bounds of the values in the domain.
     */
    T min, max;

    /**
     * List of constraints registered to the bind event.
     */
    std::vector<Constraint*> evBind;
    /**
     * List of constraints registered to the update min event
     */
    std::vector<Constraint*> evMin;
    /**
     * List of constraints registered to the update max event
     */
    std::vector<Constraint*> evMax;
};


////////////////////////////////////////////////////////////////////////////////
// Template implementation

template<class T>
BoundsVariable<T>::BoundsVariable(Solver *solver, T min, T max) :
        Variable(solver, 2 * sizeof(T)),
        min(min),
        max(max) {
    size = max - min + 1;
}

template<class T>
void BoundsVariable<T>::checkpoint(void *trail) const {
    *((reinterpret_cast<T*&>(trail))++) = min;
    *((reinterpret_cast<T*&>(trail))++) = max;
}

template<class T>
void BoundsVariable<T>::restore(const void *trail) {
    min  = *((reinterpret_cast<const T*&>(trail))++);
    max  = *((reinterpret_cast<const T*&>(trail))++);
    size = max - min + 1;
}

template<class T>
void BoundsVariable<T>::select() {
    assert(min < max && size > 1);
    bool ret = bind(min);
    assert(ret);
}

template<class T>
void BoundsVariable<T>::unselect() {
    assert(min < max && size > 1);
    bool ret = updateMin(min + 1);
    assert(ret);
}

template<class T>
bool BoundsVariable<T>::bind(T v) {
    if(v < min || v > max)
        return false;
    if(size == 1)
        return true;
    size = 1;
    if(v != min) {
        min = v;
        solver->enqueue(evMin);
    }
    if(v != max) {
        max = v;
        solver->enqueue(evMax);
    }
    solver->enqueue(evBind);
    assert(size == max - min + 1);
    return true;
}

template<class T>
bool BoundsVariable<T>::updateMin(T v) {
    if(v <= min)
        return true;
    if(v > max)
        return false;
    if(v == max)
        return bind(v);
    min = v;
    size = max - min + 1;
    solver->enqueue(evMin);
    return true;
}

template<class T>
bool BoundsVariable<T>::updateMax(T v) {
    if(v >= max)
        return true;
    if(v < min)
        return false;
    if(v == min)
        return bind(v);
    max = v;
    size = max - min + 1;
    solver->enqueue(evMax);
    return true;
}

}
}

#endif // CASTOR_CP_BOUNDSVAR_H
