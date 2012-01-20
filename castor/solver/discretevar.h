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
#ifndef CASTOR_CP_DISCRETEVAR_H
#define CASTOR_CP_DISCRETEVAR_H

#include <vector>
#include <cassert>

#include "solver.h"
#include "variable.h"

namespace castor {
namespace cp {

class Constraint;

/**
 * Discrete variable with auxiliary bounds representation.
 *
 * Variables have two representations: a discrete (unordered) representation
 * with every values still in the domain and a bounds representation. Those two
 * representations are not tightly linked to avoid overhead. The following
 * assertions hold:
 * - size = number of values left in the discrete representation
 * - size == 1 <=> min == max == value
 *
 * @param T the type of the values (should be an integer type)
 */
template<class T>
class DiscreteVariable : public Variable {
public:
    /**
     * Construct a variable with domain minVal..maxVal.
     *
     * @param solver attached solver
     * @param minVal lowest value in the domain
     * @param maxVal highest value in the domain
     */
    DiscreteVariable(Solver *solver, T minVal, T maxVal);

    /**
     * Destructor.
     */
    ~DiscreteVariable();

    // Implementation of virtual functions
    void checkpoint(void *trail) const;
    void restore(const void *trail);
    void select();
    void unselect();

    /**
     * @pre isBound() == true
     * @return the value bound to this variable
     */
    T getValue() { return domain[0]; }

    /**
     * Get the domain array. Beware that this is a pointer directly to the
     * internal structure, so you should not mess with it. Removing a value
     * from the domain only affects values after itself in the array. Marking
     * a value only affects values before itself.
     *
     * @return pointer to the domain array
     */
    const T* getDomain() { return domain; }

    /**
     * @param v a value
     * @return whether value v is in the domain (intersection of both
     *         representations)
     */
    bool contains(T v) { return v >= min && v <= max && map[v-minVal] < size; }

    /**
     * @return the lower bound (may not be consistent)
     */
    T getMin() { return min; }
    /**
     * @return the upper bound (may not be consistent)
     */
    T getMax() { return max; }

    /**
     * Mark a value in the domain. Do nothing if the value is not in
     * the domain.
     *
     * @param v the value to be marked
     */
    void mark(T v);

    /**
     * Clear marks of a variable.
     */
    void clearMarks();

    /**
     * Bind a value to a variable. This also clear the marks of the variable.
     *
     * @note Should only be called during constraint propagation.
     *
     * @param v the value
     * @return false if the domain becomes empty, true otherwise
     */
    bool bind(T v);

    /**
     * Remove a value from the domain of a variable. This also clears the marks
     * of the variable.
     *
     * @note Should only be called during constraint propagation.
     *
     * @param v the value to remove
     * @return false if the domain becomes empty, true otherwise
     */
    bool remove(T v);

    /**
     * Restrict the domain of a variable to its marked values only. This also
     * clears the marks of the variable afterwards.
     *
     * @note Should only be called during constraint propagation.
     *
     * @return false if the domain becomes empty, true otherwise
     */
    bool restrictToMarks();

    /**
     * Remove all values < v from the domain of the variable. This also clears
     * the marks of the variable.
     *
     * @note Should only be called during constraint propagation.
     *
     * @param v the new lower bound
     * @return false if the domain becomes empty, true otherwise
     */
    bool updateMin(T v);

    /**
     * Remove all values > v from the domain of the variable. This also clears
     * the marks of the variable.
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
     * Register constraint c to the change event of this variable. A constraint
     * must not register twice for the same variable.
     *
     * @param c the constraint
     */
    void registerChange(Constraint *c) { evChange.push_back(c); }

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
     * Lowest and highest value in the initial domain.
     */
    T minVal, maxVal;

    /**
     * Lower and upper bounds of the values in the domain.
     * The thightness of the bounds is not guaranteed when size > 1.
     * In other words, these bounds may be inconsistent, except when the
     * variable is bound.
     */
    T min, max;

    /**
     * @invariant domain[0..size-1] = domain of the variable
     */
    T* domain;

    /**
     * @invariant map[v-minVal] = position of value v in domain
     * @invariant map[v-minVal] = i <=> domain[i] = v
     * @invariant contains(v) <=> map[v-minVal] < size
     */
    unsigned* map;

    /**
     * Number of marked values.
     * The marked values are domain[0..marked-1].
     *
     * @invariant marked <= size
     */
    unsigned marked;

    /**
     * Lowest and highest marked values.
     */
    T markedmin, markedmax;

    /**
     * List of constraints registered to the bind event.
     */
    std::vector<Constraint*> evBind;
    /**
     * List of constraints registered to the change event.
     */
    std::vector<Constraint*> evChange;
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
DiscreteVariable<T>::DiscreteVariable(Solver *solver, T minVal, T maxVal) :
        Variable(solver, sizeof(unsigned) + 2 * sizeof(T)),
        minVal(minVal),
        maxVal(maxVal) {
    size = maxVal - minVal + 1;
    domain = new T[size];
    map = new unsigned[size];
    for(T v = minVal, i = 0; v <= maxVal; ++v, ++i) {
        domain[i] = v;
        map[v] = i;
    }
    min = minVal;
    max = maxVal;
    clearMarks();
}

template<class T>
DiscreteVariable<T>::~DiscreteVariable() {
    delete[] domain;
    delete[] map;
}

template<class T>
void DiscreteVariable<T>::checkpoint(void *trail) const {
    *((reinterpret_cast<unsigned*&>(trail))++) = size;
    *((reinterpret_cast<T*&>       (trail))++) = min;
    *((reinterpret_cast<T*&>       (trail))++) = max;
}

template<class T>
void DiscreteVariable<T>::restore(const void *trail) {
    size = *((reinterpret_cast<const unsigned*&>(trail))++);
    min  = *((reinterpret_cast<const T*&>       (trail))++);
    max  = *((reinterpret_cast<const T*&>       (trail))++);
}

template<class T>
void DiscreteVariable<T>::select() {
    assert(bind(domain[0]));
}

template<class T>
void DiscreteVariable<T>::unselect() {
    assert(remove(domain[0]));
}

template<class T>
void DiscreteVariable<T>::mark(T v) {
    if(v < min || v > max)
        return;
    unsigned i = map[v-minVal];
    if(i >= size || i < marked)
        return;
    if(i != marked) {
        T v2 = domain[marked];
        domain[i] = v2;
        domain[marked] = v;
        map[v2-minVal] = i;
        map[v-minVal] = marked;
    }
    if(marked == 0 || v < markedmin)
        markedmin = v;
    if(marked == 0 || v > markedmax)
        markedmax = v;
    ++marked;
}

template<class T>
void DiscreteVariable<T>::clearMarks() {
    marked = 0;
}

template<class T>
bool DiscreteVariable<T>::bind(T v) {
    clearMarks();
    if(v < min || v > max)
        return false;
    unsigned i = map[v-minVal];
    if(i >= size)
        return false;
    if(size == 1)
        return true;
    if(i != 0) {
        T v2 = domain[0];
        domain[i] = v2;
        domain[0] = v;
        map[v2-minVal] = i;
        map[v-minVal] = 0;
    }
    size = 1;
    if(v != min) {
        min = v;
        solver->enqueue(evMin);
    }
    if(v != max) {
        max = v;
        solver->enqueue(evMax);
    }
    solver->enqueue(evChange);
    solver->enqueue(evBind);
    return true;
}

template<class T>
bool DiscreteVariable<T>::remove(T v) {
    clearMarks();
    if(v < minVal || v > maxVal)
        return true;
    unsigned i = map[v-minVal];
    if(i >= size)
        return true;
    if(size <= 1)
        return false;
    size--;
    if(i != size) {
        T v2 = domain[size];
        domain[i] = v2;
        domain[size] = v;
        map[v2-minVal] = i;
        map[v-minVal] = size;
    }
    if(size == 1) {
        if(domain[0] < min || domain[0] > max)  // check representations consistency
            return false;
        solver->enqueue(evBind);
        if(v != min) {
            min = v;
            solver->enqueue(evMin);
        }
        if(v != max) {
            max = v;
            solver->enqueue(evMax);
        }
    } else {
        if(v == min) {
            // TODO is this usefull?
            min++; // not perfect bound
            solver->enqueue(evMin);
        }
        if(v == max) {
            // TODO is this usefull?
            max--; // not perfect bound
            solver->enqueue(evMax);
        }
    }
    solver->enqueue(evChange);
    return true;
}

template<class T>
bool DiscreteVariable<T>::restrictToMarks() {
    unsigned m = marked;
    T mmin = markedmin, mmax = markedmax;
    clearMarks();
    if(m != size) {
        size = m;
        if(m == 0)
            return false;
        if(min != mmin) {
            min = mmin;
            solver->enqueue(evMin);
        }
        if(max != mmax) {
            max = mmax;
            solver->enqueue(evMax);
        }
        solver->enqueue(evChange);
        if(m == 1)
            solver->enqueue(evBind);
    }
    return true;
}

template<class T>
bool DiscreteVariable<T>::updateMin(T v) {
    clearMarks();
    if(v <= min)
        return true;
    if(v > max)
        return false;
    if(v == max)
        return bind(v);
    min = v;
    solver->enqueue(evChange);
    solver->enqueue(evMin);
    return true;
}

template<class T>
bool DiscreteVariable<T>::updateMax(T v) {
    clearMarks();
    if(v >= max)
        return true;
    if(v < min)
        return false;
    if(v == min)
        return bind(v);
    max = v;
    solver->enqueue(evChange);
    solver->enqueue(evMax);
    return true;
}

}
}

#endif // CASTOR_CP_DISCRETEVAR_H
