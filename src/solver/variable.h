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
#ifndef CASTOR_CP_VARIABLE_H
#define CASTOR_CP_VARIABLE_H

#include <cstddef>
#include <cassert>

namespace castor {
namespace cp {

class Solver;

/**
 * Base class for a trailable object.
 */
class Trailable {
public:
    /**
     * @return the parent solver
     */
    Solver* solver() { return solver_; }

    /**
     * @return the number of bytes to allocate for the trailing structure
     */
    std::size_t trailSize() const { return trailSize_; }

    /**
     * Write a checkpoint of the current state to the memory pointed to by
     * trail. The allocated space is of size trailSize.
     *
     * This method will only be called if trailSize() > 0.
     *
     * @param trail the target trail memory where the checkpoint must be
     *              written
     */
    virtual void checkpoint(void* trail) const = 0;

    /**
     * Restore the state from the memory pointed to by trail. This memory, of
     * size trailSize, had been written to by checkpoint() before.
     *
     * This method will only be called if trailSize() > 0.
     *
     * @param trail the source memory where the checkpoint has been written
     */
    virtual void restore(const void* trail) = 0;

protected:
    /**
     * Construct a trailable object.
     *
     * @param solver attached solver
     * @param trailSize size in bytes needed for the trail
     */
    Trailable(Solver* solver, std::size_t trailSize) :
        solver_(solver), trailSize_(trailSize) {
        assert(solver != nullptr);
    }

    //! Non-copyable
    Trailable(const Trailable&) = delete;
    Trailable& operator=(const Trailable&) = delete;

    virtual ~Trailable() {}

private:
    /**
     * Parent solver class.
     */
    Solver* solver_;

    /**
     * Number of bytes to allocate for the trailing structure. This number does
     * not change after the construction of the object.
     */
    std::size_t trailSize_;
};

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
     * @return whether this variable is bound
     */
    bool bound() const { return size_ == 1; }

    /**
     * Bind this variable to a value of its domain. This is called during the
     * labeling phase of the search. Appropriate events should be triggered.
     *
     * @pre !bound()
     * @post bound()
     */
    virtual void label() = 0;

    /**
     * Remove from the domain the value that would be assigned to this variable
     * by label(). It is called after backtracking.
     *
     * @pre !bound()
     * @post size() := size() - 1
     */
    virtual void unlabel() = 0;

protected:
    /**
     * Default constructor with bogus values for Trailable. As Trailable is
     * a virtual base class, subclasses of DecisionVariable would have to
     * initialize Trailable by themselves anyway.
     */
    DecisionVariable() : Trailable(nullptr, 0) {}

    /**
     * Current size of the domain. Must be updated by the subclass.
     */
    unsigned size_;
};

}
}

#endif // CASTOR_CP_VARIABLE_H
