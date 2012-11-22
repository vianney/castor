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
#ifndef CASTOR_CP_SMALLVAR_H
#define CASTOR_CP_SMALLVAR_H

#include <vector>
#include <cstdint>
#include <cassert>
#include <iostream>

#include "../util.h"
#include "solver.h"
#include "variable.h"

namespace castor {
namespace cp {

class Constraint;

/**
 * Discrete variable with a small domain (<= 32 values).
 *
 * The domain is represented as a bitset.
 *
 * @param T the type of the values (should be an integer type)
 */
template<class T>
class SmallVariable : public Trailable {
public:
    /**
     * Construct a small variable.
     *
     * @param solver attached solver
     * @param minVal lowest value in the domain
     * @param maxVal highest value in the domain
     * @pre minVal <= maxVal <= minVal + 32
     */
    SmallVariable(Solver* solver, T minVal, T maxVal);

    // Implementation of virtual functions
    void checkpoint(void* trail) const;
    void restore(const void* trail);

    /**
     * @param v a value
     * @return whether value v is in the domain (intersection of both
     *         representations)
     */
    bool contains(T v) const {
        return v >= minVal_ && v <= maxVal_ && domain_ & 1 << (v-minVal_);
    }

    /**
     * @return the lower bound
     */
    T min() const { return minVal_ + ffs(domain_) - 1; }
    /**
     * @return the upper bound
     */
    T max() const { return minVal_ + fls(domain_); }
    // FIXME: why not -1??

    /**
     * @return whether this variable is bound
     */
    bool bound() const { return min() == max(); }

    /**
     * @pre bound() == true
     * @return the value bound to this variable
     */
    T value() const { return min(); }

    /**
     * @return the domain bitset
     */
    unsigned domain() const { return domain_; }

    /**
     * Mark a value in the domain. Do nothing if the value is not in
     * the domain.
     *
     * @param v the value to be marked
     */
    void mark(T v) { if(contains(v)) marked_ |= 1 << (v - minVal_); }

    /**
     * Clear marks of a variable.
     */
    void clearMarks() { marked_ = 0; }

    /**
     * Bind a value to this variable.
     * This also clear the marks of the variable.
     *
     * @note Should only be called during constraint propagation.
     *
     * @param v the value
     * @return false if the domain becomes empty, true otherwise
     */
    bool bind(T v);

    /**
     * Remove a value from the domain.
     * This also clear the marks of the variable.
     *
     * @note Should only be called during constraint propagation.
     *
     * @param v the value to remove
     * @return false if the domain becomes empty, true otherwise
     */
    bool remove(T v);

    /**
     * Restrict the domain of a variable to its marked values only.
     * This also clears the marks of the variable afterwards.
     *
     * @note Should only be called during constraint propagation.
     *
     * @return false if the domain becomes empty, true otherwise
     */
    bool restrictToMarks();

    /**
     * Remove all values < v from the domain of the variable.
     * This also clear the marks of the variable.
     *
     * @note Should only be called during constraint propagation.
     *
     * @param v the new lower bound
     * @return false if the domain becomes empty, true otherwise
     */
    bool updateMin(T v);

    /**
     * Remove all values > v from the domain of the variable.
     * This also clear the marks of the variable.
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
    void registerBind(Constraint* c) { evBind_.push_back(c); }

    /**
     * Register constraint c to the change event of this variable. A constraint
     * must not register twice for the same variable.
     *
     * @param c the constraint
     */
    void registerChange(Constraint* c) { evChange_.push_back(c); }

    /**
     * Register constraint c to the update min event of this variable.
     * A constraint must not register twice for the same variable.
     *
     * @param c the constraint
     */
    void registerMin(Constraint* c) { evMin_.push_back(c); }

    /**
     * Register constraint c to the update max event of this variable.
     * A constraint must not register twice for the same variable.
     *
     * @param c the constraint
     */
    void registerMax(Constraint* c) { evMax_.push_back(c); }

private:
    T minVal_; //!< lowest value in the initial domain
    T maxVal_; //!< highest value in the initial domain

    /**
     * Bitset representing the domain. The least significant bit corresponds
     * to minVal_.
     */
    unsigned domain_;

    /**
     * Bitset representing the marked values. The least significant bit
     * corresponds to minVal_.
     */
    unsigned marked_;

    /**
     * List of constraints registered to the bind event.
     */
    std::vector<Constraint*> evBind_;
    /**
     * List of constraints registered to the change event.
     */
    std::vector<Constraint*> evChange_;
    /**
     * List of constraints registered to the update min event
     */
    std::vector<Constraint*> evMin_;
    /**
     * List of constraints registered to the update max event
     */
    std::vector<Constraint*> evMax_;
};

template<class T>
std::ostream& operator<<(std::ostream& out, const SmallVariable<T>& x);


/**
 * Special case: boolean variable.
 */
class BooleanVariable : public SmallVariable<int> {
public:
    /**
     * Construct a boolean variable.
     *
     * @param solver attached solver
     */
    BooleanVariable(Solver* solver) : SmallVariable(solver, 0, 1) {}

    bool contains(bool v) const { return SmallVariable::contains(v ? 1 : 0); }
    bool value   ()       const { return SmallVariable::min     ();          }
    bool bind    (bool v)       { return SmallVariable::bind    (v ? 1 : 0); }
    bool remove  (bool v)       { return SmallVariable::remove  (v ? 1 : 0); }
};


////////////////////////////////////////////////////////////////////////////////
// Template implementation

template<class T>
SmallVariable<T>::SmallVariable(Solver* solver, T minVal, T maxVal) :
        Trailable(solver, minVal == maxVal ? 0 : sizeof(unsigned)),
        minVal_(minVal),
        maxVal_(maxVal) {
    domain_ = ((1 << (maxVal - minVal)) - 1) | (1 << (maxVal - minVal));
}

template<class T>
void SmallVariable<T>::checkpoint(void* trail) const {
    *((reinterpret_cast<unsigned*&>(trail))++) = domain_;
}

template<class T>
void SmallVariable<T>::restore(const void* trail) {
    domain_ = *((reinterpret_cast<const unsigned*&>(trail))++);
}

template<class T>
bool SmallVariable<T>::bind(T v) {
    clearMarks();
    if(!contains(v))
        return false;
    T m = min(), M = max();
    if(m == M)
        return true;
    domain_ = 1 << (v - minVal_);
    if(v != m)
        solver()->enqueue(evMin_);
    if(v != M)
        solver()->enqueue(evMax_);
    solver()->enqueue(evChange_);
    solver()->enqueue(evBind_);
    return true;
}

template<class T>
bool SmallVariable<T>::remove(T v) {
    clearMarks();
    if(!contains(v))
        return true;
    T m = min(), M = max();
    domain_ &= ~(1 << (v - minVal_));
    if(domain_ == 0)
        return false;
    if(v == m)
        solver()->enqueue(evMin_);
    if(v == M)
        solver()->enqueue(evMax_);
    if(bound())
        solver()->enqueue(evBind_);
    solver()->enqueue(evChange_);
    return true;
}

template<class T>
bool SmallVariable<T>::restrictToMarks() {
    unsigned marked = marked_;
    clearMarks();
    if(marked == 0)
        return false;
    if(marked == domain_)
        return true;
    T m = min(), M = max();
    domain_ = marked;
    if(bound() && m != M)
        solver()->enqueue(evBind_);
    if(m != min())
        solver()->enqueue(evMin_);
    if(M != max())
        solver()->enqueue(evMax_);
    solver()->enqueue(evChange_);
    return true;
}

template<class T>
bool SmallVariable<T>::updateMin(T v) {
    clearMarks();
    if(v < minVal_)
        return true;
    if(v > maxVal_)
        return false;
    unsigned old = domain_;
    domain_ &= ~((1 << (v - minVal_)) - 1);
    if(domain_ == 0)
        return false;
    if(domain_ != old) {
        solver()->enqueue(evChange_);
        solver()->enqueue(evMin_);
        if(bound())
            solver()->enqueue(evBind_);
    }
    return true;
}

template<class T>
bool SmallVariable<T>::updateMax(T v) {
    clearMarks();
    if(v > maxVal_)
        return true;
    if(v < minVal_)
        return false;
    unsigned old = domain_;
    domain_ &= ((1 << (v - minVal_)) - 1) | (1 << (v - minVal_));
    if(domain_ == 0)
        return false;
    if(domain_ != old) {
        solver()->enqueue(evChange_);
        solver()->enqueue(evMax_);
        if(bound())
            solver()->enqueue(evBind_);
    }
    return true;
}

template<class T>
std::ostream& operator<<(std::ostream& out, const SmallVariable<T>& x) {
    out << "(" << std::hex << x.domain() << std::dec << ")["
        << x.min() << ".." << x.max() << "]";
    return out;
}

}
}

#endif // CASTOR_CP_SMALLVAR_H
