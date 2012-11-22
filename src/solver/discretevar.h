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
#ifndef CASTOR_CP_DISCRETEVAR_H
#define CASTOR_CP_DISCRETEVAR_H

#include <vector>
#include <cassert>
#include <iostream>

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
class DiscreteVariable : public DecisionVariable {
public:
    /**
     * Construct a variable with domain minVal..maxVal.
     *
     * @param solver attached solver
     * @param minVal lowest value in the domain
     * @param maxVal highest value in the domain
     */
    DiscreteVariable(Solver* solver, T minVal, T maxVal);

    /**
     * Destructor.
     */
    ~DiscreteVariable();

    // Implementation of virtual functions
    void checkpoint(void* trail) const;
    void restore(const void* trail);
    void label();
    void unlabel();

    /**
     * @pre bound() == true
     * @return the value bound to this variable
     */
    T value() const { return domain_[0]; }

    /**
     * Get the domain array. Beware that this is a pointer directly to the
     * internal structure, so you should not mess with it. Removing a value
     * from the domain only affects values after itself in the array. Marking
     * a value only affects values before itself.
     *
     * @return pointer to the domain array
     */
    const T* domain() const { return domain_; }

    /**
     * @param v a value
     * @return whether value v is in the domain (intersection of both
     *         representations)
     */
    bool contains(T v) const {
        return v >= min_ && v <= max_ && map_[v-minVal_] < size_;
    }

    /**
     * @return the lower bound (may not be consistent)
     */
    T min() const { return min_; }
    /**
     * @return the upper bound (may not be consistent)
     */
    T max() const { return max_; }

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
     * Lower bound of the values in the domain.
     * The thightness of the bounds is not guaranteed when size > 1.
     * In other words, these bounds may be inconsistent, except when the
     * variable is bound.
     */
    T min_;
    /**
     * Upper bound of the values in the domain.
     * @sa min_
     */
    T max_;

    /**
     * @invariant domain[0..size-1] = domain of the variable
     */
    T* domain_;

    /**
     * @invariant map[v-minVal] = position of value v in domain
     * @invariant map[v-minVal] = i <=> domain[i] = v
     * @invariant contains(v) <=> map[v-minVal] < size
     */
    unsigned* map_;

    /**
     * Number of marked values.
     * The marked values are domain[0..marked-1].
     *
     * @invariant marked <= size
     */
    unsigned marked_;

    T markedmin_; //!< lowest marked value
    T markedmax_; //!< highest marked value

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
std::ostream& operator<<(std::ostream& out, const DiscreteVariable<T>& x);


////////////////////////////////////////////////////////////////////////////////
// Template implementation

template<class T>
DiscreteVariable<T>::DiscreteVariable(Solver* solver, T minVal, T maxVal) :
        Trailable(solver, sizeof(unsigned) + 2 * sizeof(T)),
        minVal_(minVal),
        maxVal_(maxVal) {
    size_ = maxVal - minVal + 1;
    domain_ = new T[size_];
    map_ = new unsigned[size_];
    for(T v = minVal, i = 0; v <= maxVal; ++v, ++i) {
        domain_[i] = v;
        map_[v-minVal] = i;
    }
    min_ = minVal;
    max_ = maxVal;
    clearMarks();
}

template<class T>
DiscreteVariable<T>::~DiscreteVariable() {
    delete[] domain_;
    delete[] map_;
}

template<class T>
void DiscreteVariable<T>::checkpoint(void* trail) const {
    *((reinterpret_cast<unsigned*&>(trail))++) = size_;
    *((reinterpret_cast<T*&>       (trail))++) = min_;
    *((reinterpret_cast<T*&>       (trail))++) = max_;
}

template<class T>
void DiscreteVariable<T>::restore(const void* trail) {
    size_ = *((reinterpret_cast<const unsigned*&>(trail))++);
    min_  = *((reinterpret_cast<const T*&>       (trail))++);
    max_  = *((reinterpret_cast<const T*&>       (trail))++);
}

template<class T>
void DiscreteVariable<T>::label() {
    assert(size_ > 1);
    bind(domain_[0]);
}

template<class T>
void DiscreteVariable<T>::unlabel() {
    assert(size_ > 1);
    remove(domain_[0]);
}

template<class T>
void DiscreteVariable<T>::mark(T v) {
    if(v < min_ || v > max_)
        return;
    unsigned i = map_[v-minVal_];
    if(i >= size_ || i < marked_)
        return;
    if(i != marked_) {
        T v2 = domain_[marked_];
        domain_[i] = v2;
        domain_[marked_] = v;
        map_[v2-minVal_] = i;
        map_[v-minVal_] = marked_;
    }
    if(marked_ == 0 || v < markedmin_)
        markedmin_ = v;
    if(marked_ == 0 || v > markedmax_)
        markedmax_ = v;
    ++marked_;
}

template<class T>
void DiscreteVariable<T>::clearMarks() {
    marked_ = 0;
}

template<class T>
bool DiscreteVariable<T>::bind(T v) {
    clearMarks();
    if(v < min_ || v > max_)
        return false;
    unsigned i = map_[v-minVal_];
    if(i >= size_)
        return false;
    if(size_ == 1)
        return true;
    if(i != 0) {
        T v2 = domain_[0];
        domain_[i] = v2;
        domain_[0] = v;
        map_[v2-minVal_] = i;
        map_[v-minVal_] = 0;
    }
    size_ = 1;
    if(v != min_) {
        min_ = v;
        solver()->enqueue(evMin_);
    }
    if(v != max_) {
        max_ = v;
        solver()->enqueue(evMax_);
    }
    solver()->enqueue(evChange_);
    solver()->enqueue(evBind_);
    assert(min_ == max_ && min_ == value());
    return true;
}

template<class T>
bool DiscreteVariable<T>::remove(T v) {
    clearMarks();
    if(v < minVal_ || v > maxVal_)
        return true;
    unsigned i = map_[v-minVal_];
    if(i >= size_)
        return true;
    switch(size_) {
    case 0: case 1:
        return false;
    case 2:
        return bind(domain_[1-i]);
    default:
        size_--;
        if(i != size_) {
            T v2 = domain_[size_];
            domain_[i] = v2;
            domain_[size_] = v;
            map_[v2-minVal_] = i;
            map_[v-minVal_] = size_;
        }
        // FIXME: this may break the invariants
        // e.g.: domain_ = [10 11 12], min_=10, max_=11; remove(10)
        /*if(v == min_) {
            // TODO: is this usefull?
            min_++; // not perfect bound
            solver()->enqueue(evMin_);
        }
        if(v == max_) {
            // TODO: is this usefull?
            max_--; // not perfect bound
            solver()->enqueue(evMax_);
        }*/
        solver()->enqueue(evChange_);
        assert(size_ > 1 || (min_ == max_ && min_ == value()));
        assert(min_ < max_ || (size_ == 1 && min_ == value()));
        return true;
    }
}

template<class T>
bool DiscreteVariable<T>::restrictToMarks() {
    unsigned m = marked_;
    T mmin = markedmin_;
    T mmax = markedmax_;
    clearMarks();
    if(m != size_) {
        size_ = m;
        if(m == 0)
            return false;
        if(min_ != mmin) {
            min_ = mmin;
            solver()->enqueue(evMin_);
        }
        if(max_ != mmax) {
            max_ = mmax;
            solver()->enqueue(evMax_);
        }
        solver()->enqueue(evChange_);
        if(m == 1)
            solver()->enqueue(evBind_);
    }
    assert(size_ > 1 || (min_ == max_ && min_ == value()));
    assert(min_ < max_ || (size_ == 1 && min_ == value()));
    return true;
}

template<class T>
bool DiscreteVariable<T>::updateMin(T v) {
    clearMarks();
    if(v > max_) {
        return false;
    } else if(v == max_) {
        return bind(v);
    } else if(v > min_) {
        min_ = v;
        solver()->enqueue(evChange_);
        solver()->enqueue(evMin_);
        assert(size_ > 1 || (min_ == max_ && min_ == value()));
        assert(min_ < max_ || (size_ == 1 && min_ == value()));
        return true;
    } else {
        return true;
    }
}

template<class T>
bool DiscreteVariable<T>::updateMax(T v) {
    clearMarks();
    if(v < min_) {
        return false;
    } else if(v == min_) {
        return bind(v);
    } else if(v < max_) {
        max_ = v;
        solver()->enqueue(evChange_);
        solver()->enqueue(evMax_);
        assert(size_ > 1 || (min_ == max_ && min_ == value()));
        assert(min_ < max_ || (size_ == 1 && min_ == value()));
        return true;
    } else {
        return true;
    }
}

template<class T>
std::ostream& operator<<(std::ostream& out, const DiscreteVariable<T>& x) {
    out << "(" << x.size() << ")[" << x.min() << ".." << x.max()
        << "]";
    return out;
}

}
}

#endif // CASTOR_CP_DISCRETEVAR_H
