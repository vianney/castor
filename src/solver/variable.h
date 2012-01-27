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

namespace castor {
namespace cp {

class Solver;

/**
 * Base class for a CP variable.
 *
 * This abstract class only contains the interface for backtracking. Operations
 * on a variable are dependent of its domain implementation.
 */
class Variable {
public:
    /**
     * Construct a variable.
     *
     * @param solver attached solver
     * @param trailSize size in bytes needed for the trail
     */
    Variable(Solver *solver, std::size_t trailSize) :
        solver(solver), trailSize(trailSize) {}

    virtual ~Variable() {}

    /**
     * @return the parent solver
     */
    Solver* getSolver() { return solver; }

    /**
     * @return the number of bytes to allocate for the trailing structure
     */
    std::size_t getTrailSize() const { return trailSize; }

    /**
     * @return the current size of the domain of this variable
     */
    unsigned getSize() const { return size; }

    /**
     * @return whether this variable is bound
     */
    bool isBound() const { return size == 1; }

    /**
     * Write a checkpoint of the current domain to the memory pointed to by
     * trail. The allocated space is of size trailSize.
     *
     * @param trail the target trail memory where the checkpoint must be
     *              written
     */
    virtual void checkpoint(void *trail) const = 0;

    /**
     * Restore the domain from the memory pointed to by trail. This memory, of
     * size trailSize, had been written to by checkpoint() before.
     *
     * @param trail the source memory where the checkpoint has been written
     */
    virtual void restore(const void *trail) = 0;

    /**
     * Bind this variable to a value of its domain. This is called during the
     * labeling phase of the search. Appropriate events should be triggered.
     *
     * @pre getSize() > 1
     * @post isBound()
     */
    virtual void select() = 0;

    /**
     * Remove from the domain the value that would be assigned to this variable
     * by select(). It is called after backtracking.
     *
     * @pre getSize() > 1
     * @post getSize() := getSize() - 1
     */
    virtual void unselect() = 0;

protected:
    /**
     * Parent solver class.
     */
    Solver *solver;

    /**
     * Current size of the domain.
     */
    unsigned size;

private:
    /**
     * Number of bytes to allocate for the trailing structure. This number does
     * not change after the construction of the variable.
     */
    std::size_t trailSize;
};

}
}

#endif // CASTOR_CP_VARIABLE_H
