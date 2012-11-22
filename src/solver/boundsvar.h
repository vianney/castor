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
 * @param T the type of the values
 */
template<class T>
class BoundsVariable : public virtual Trailable {
public:
    /**
     * Construct a variable with domain min..max.
     *
     * @param solver attached solver
     * @param min initial lower bound
     * @param max initial upper bound
     */
    BoundsVariable(Solver* solver, T min, T max);

    // Implementation of virtual functions
    void save(Trail& trail) const;
    void restore(Trail& trail);

    /**
     * @param v a value
     * @return whether value v is in the domain
     */
    bool contains(T v) const { return v >= min_ && v <= max_; }

    /**
     * @return the lower bound
     */
    T min() const { return min_; }
    /**
     * @return the upper bound
     */
    T max() const { return max_; }

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
    void registerBind(Constraint* c) { evBind_.push_back(c); }

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

protected:

    T min_; //!< lower bound
    T max_; //!< upper bound

private:
    Solver* solver_; //!< attached solver

    /**
     * List of constraints registered to the bind event.
     */
    std::vector<Constraint*> evBind_;
    /**
     * List of constraints registered to the update min event
     */
    std::vector<Constraint*> evMin_;
    /**
     * List of constraints registered to the update max event
     */
    std::vector<Constraint*> evMax_;
};

/**
 * Extension of BoundsVariable to be a decision variable, i.e., it is labelable.
 * However, this adds the restriction that T is of integer type (i.e., the
 * operation +1 is well defined).
 *
 * BoundsVariable is a private base class to disallow casting to it.
 *
 * @param T the type of the values (should be an integer type)
 */
template<class T>
class BoundsDecisionVariable : private BoundsVariable<T>, public DecisionVariable {
public:
    /**
     * Construct a variable with domain min..max.
     *
     * @param solver attached solver
     * @param min initial lower bound
     * @param max initial upper bound
     */
    BoundsDecisionVariable(Solver* solver, T min, T max);

    /**
     * @pre bound() == true
     * @return the value bound to this variable
     */
    T value() const { return this->min_; }

    // Implementation of virtual functions
    void label();
    void unlabel();

    // Overrides to update size_
    void restore(Trail& trail) {
        BoundsVariable<T>::restore(trail);
        updateSize();
    }

    bool bind(T v) {
        if(size_ == 1 && this->min_ == v)
            return true;
        if(!BoundsVariable<T>::bind(v))
            return false;
        updateSize();
        return true;
    }

    bool updateMin(T v) {
        if(!BoundsVariable<T>::updateMin(v))
            return false;
        updateSize();
        return true;
    }

    bool updateMax(T v) {
        if(!BoundsVariable<T>::updateMax(v))
            return false;
        updateSize();
        return true;
    }

    // Passthrough methods
    void save(Trail& trail) const { BoundsVariable<T>::save(trail); }
    bool contains(T v) const { return BoundsVariable<T>::contains(v); }
    T    min     ()    const { return BoundsVariable<T>::min(); }
    T    max     ()    const { return BoundsVariable<T>::max(); }
    void registerBind(Constraint* c) { BoundsVariable<T>::registerBind(c); }
    void registerMin (Constraint* c) { BoundsVariable<T>::registerMin (c); }
    void registerMax (Constraint* c) { BoundsVariable<T>::registerMax (c); }

private:
    /**
     * Update size_ of DecisionVariable.
     */
    void updateSize() {
        size_ = this->max_ - this->min_ + 1;
    }
};


////////////////////////////////////////////////////////////////////////////////
// Template implementation

template<class T>
BoundsVariable<T>::BoundsVariable(Solver* solver, T min, T max) :
    Trailable(solver->trail()),
    min_(min),
    max_(max),
    solver_(solver) {}

template<class T>
void BoundsVariable<T>::save(Trail& trail) const {
    trail.push(min_);
    trail.push(max_);
}

template<class T>
void BoundsVariable<T>::restore(Trail &trail) {
    max_ = trail.pop<T>();
    min_ = trail.pop<T>();
}

template<class T>
bool BoundsVariable<T>::bind(T v) {
    if(v < min_ || v > max_)
        return false;
    if(min_ == max_)
        return true;
    modifying();
    if(v != min_) {
        min_ = v;
        solver_->enqueue(evMin_);
    }
    if(v != max_) {
        max_ = v;
        solver_->enqueue(evMax_);
    }
    solver_->enqueue(evBind_);
    return true;
}

template<class T>
bool BoundsVariable<T>::updateMin(T v) {
    if(v > max_) {
        return false;
    } else if(v == max_) {
        return bind(v);
    } else if(v > min_) {
        modifying();
        min_ = v;
        solver_->enqueue(evMin_);
        return true;
    } else {
        return true;
    }
}

template<class T>
bool BoundsVariable<T>::updateMax(T v) {
    if(v < min_) {
        return false;
    } else if(v == min_) {
        return bind(v);
    } else if(v < max_) {
        modifying();
        max_ = v;
        solver_->enqueue(evMax_);
        return true;
    } else {
        return true;
    }
}


template<class T>
BoundsDecisionVariable<T>::BoundsDecisionVariable(Solver* solver, T min, T max) :
        Trailable(solver->trail()),
        BoundsVariable<T>(solver, min, max) {
    updateSize();
}

template<class T>
void BoundsDecisionVariable<T>::label() {
    assert(this->min_ < this->max_ && size_ > 1);
    bind(this->min_);
}

template<class T>
void BoundsDecisionVariable<T>::unlabel() {
    assert(this->min_ < this->max_ && size_ > 1);
    updateMin(this->min_ + 1);
}

}
}

#endif // CASTOR_CP_BOUNDSVAR_H
