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
#ifndef CASTOR_VARINT_H
#define CASTOR_VARINT_H

#include <vector>

namespace castor {

class Solver;
class Constraint;

/**
 * Integer variable.
 */
class VarInt {
public:
    /**
     * Construct a variable with domain minVal..maxVal.
     *
     * @param solver attached solver
     * @param minVal lowest value in the domain
     * @param maxVal highest value in the domain
     */
    VarInt(Solver *solver, int minVal, int maxVal);

    /**
     * Destructor.
     */
    ~VarInt();

    /**
     * @return the parent solver
     */
    Solver* getSolver() { return solver; }

    /**
     * Maintain lower bound
     */
    void maintainMin() { if(min < minVal) min = searchNextMin(); }
    /**
     * Maintain upper bound
     */
    void maintainMax() { if(max > maxVal) max = searchNextMax(); }

    /**
     * @return the current size of the domain of this variable
     */
    int getSize() { return size; }

    /**
     * @return whether this variable is bound
     */
    bool isBound() { return size == 1; }

    /**
     * @pre isBound() == true
     * @return the value bound to this variable
     */
    int getValue() { return domain[0]; }

    /**
     * Get the domain array. Beware that this is a pointer directly to the
     * internal structure, so you should not mess with it. Removing a value
     * from the domain only affects values after itself in the array. Marking
     * a value only affects values before itself.
     *
     * @return pointer to the domain array
     */
    const int* getDomain() { return domain; }

    /**
     * @param v a value
     * @return whether value v is in the domain
     */
    bool contains(int v) { return map[v-minVal] < size; }

    /**
     * @pre maintainMin() has been called
     * @return the lower bound
     */
    int getMin() { return min; }
    /**
     * @pre maintainMax() has been called
     * @return the upper bound
     */
    int getMax() { return max; }

    /**
     * Mark a value in the domain. Do nothing if the value is not in
     * the domain.
     *
     * @param v the value to be marked
     */
    void mark(int v);

    /**
     * Clear marks of a variable.
     */
    void clearMarks() {
        marked = 0;
        markedmin = maxVal + 1;
        markedmax = minVal - 1;
    }

    /**
     * Bind a value to a variable. This also clear the marks of the variable.
     *
     * @note Should only be called during constraint propagation.
     *
     * @param v the value
     * @return false if the domain becomes empty, true otherwise
     */
    bool bind(int v);

    /**
     * Remove a value from the domain of a variable. This also clears the marks
     * of the variable.
     *
     * @note Should only be called during constraint propagation.
     *
     * @param v the value to remove
     * @return false if the domain becomes empty, true otherwise
     */
    bool remove(int v);

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
    bool updateMin(int v);

    /**
     * Remove all values > v from the domain of the variable. This also clears
     * the marks of the variable.
     *
     * @note Should only be called during constraint propagation.
     *
     * @param v the new upper bound
     * @return false if the domain becomes empty, true otherwise
     */
    bool updateMax(int v);

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
     * @note This automatically calls maintainMin().
     *
     * @param c the constraint
     */
    void registerMin(Constraint *c) { maintainMin(); evMin.push_back(c); }

    /**
     * Register constraint c to the update max event of this variable.
     * A constraint must not register twice for the same variable.
     * @note This automatically calls maintainMax().
     *
     * @param c the constraint
     */
    void registerMax(Constraint *c) { maintainMax(); evMax.push_back(c); }

private:
    /**
     * Parent solver class.
     */
    Solver *solver;

    /**
     * Lowest and highest value in the initial domain.
     */
    int minVal, maxVal;

    /**
     * Current size of the domain.
     */
    int size;

    /**
     * Lowest and highest value in the current domain.
     * Set to minVal-1 (resp. maxVal+1) if not maintained.
     */
    int min, max;

    /**
     * @invariant domain[0..size-1] = domain of the variable
     */
    int* domain;

    /**
     * @invariant map[v-minVal] = position of value v in domain
     * @invariant map[v-minVal] = i <=> domain[i] = v
     * @invariant contains(v) <=> map[v-minVal] < size
     */
    int* map;

    /**
     * Number of marked values.
     * The marked values are domain[0..marked-1].
     *
     * @invariant marked <= size
     */
    int marked;

    /**
     * Lowest and highest marked values.
     */
    int markedmin, markedmax;

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

    /**
     * Removes a value from the domain. Does not update the bounds, nor does it
     * enqueue any events.
     *
     * @param v the value to remove
     * @return 0 if the domain becomes empty, -1 if nothing has changed,
     *         1 otherwise
     */
    int _remove(int v);

    /**
     * @return the lowest value > min in the domain
     */
    int searchNextMin();
    /**
     * @return the highest value < max in the domain
     */
    int searchNextMax();

    friend class Subtree;
};

}

#endif // CASTOR_VARINT_H
