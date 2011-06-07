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
#include <cassert>

#include "varint.h"
#include "solver.h"

namespace castor {

VarInt::VarInt(Solver *solver, int minVal, int maxVal) :
        solver(solver),
        minVal(minVal),
        maxVal(maxVal) {
    size = maxVal - minVal + 1;
    domain = new int[size];
    map = new int[size];
    for(int v = minVal, i = 0; v <= maxVal; v++, i++) {
        domain[i] = v;
        map[v] = i;
    }
    bounds = false;
    min = minVal - 1; // set for the first call to _searchMin()
    max = maxVal + 1; // set for the first call to _searchMax()
    clearMarks();
    strategy = SELECT_RANDOM;
}

VarInt::~VarInt() {
    delete[] domain;
    delete[] map;
}

void VarInt::maintainBounds() {
    if(bounds)
        return;
    bounds = true;
    _searchMin();
    _searchMax();
}

int VarInt::select() {
    switch(strategy) {
    case SELECT_RANDOM:
        return domain[0];
    case SELECT_MIN:
        if(!bounds)
            _searchMin();
        return min;
    case SELECT_MAX:
        if(!bounds)
            _searchMax();
        return max;
    }
    assert(false); // should not happen
    return 0;
}

void VarInt::mark(int v) {
    if(v < minVal || v > maxVal)
        return;
    int i = map[v-minVal];
    if(i >= size || i < marked)
        return;
    if(i != marked) {
        int v2 = domain[marked];
        domain[i] = v2;
        domain[marked] = v;
        map[v2-minVal] = i;
        map[v-minVal] = marked;
    }
    marked++;
    if(v < markedmin)
        markedmin = v;
    if(v > markedmax)
        markedmax = v;
}

bool VarInt::bind(int v) {
    clearMarks();
    if(v < minVal || v > maxVal)
        return false;
    int i = map[v-minVal];
    if(i >= size)
        return false;
    if(size == 1)
        return true;
    if(i != 0) {
        int v2 = domain[0];
        domain[i] = v2;
        domain[0] = v;
        map[v2-minVal] = i;
        map[v-minVal] = 0;
    }
    size = 1;
    if(bounds && v != min) {
        min = v;
        solver->enqueue(evMin);
    }
    if(bounds && v != max) {
        max = v;
        solver->enqueue(evMax);
    }
    solver->enqueue(evChange);
    solver->enqueue(evBind);
    return true;
}

inline int VarInt::_remove(int v) {
    if(v < minVal || v > maxVal)
        return -1;
    int i = map[v-minVal];
    if(i >= size)
        return -1;
    if(size <= 1)
        return 0;
    size--;
    if(i != size) {
        int v2 = domain[size];
        domain[i] = v2;
        domain[size] = v;
        map[v2-minVal] = i;
        map[v-minVal] = size;
    }
    return 1;
}

bool VarInt::remove(int v) {
    clearMarks();
    switch(_remove(v)) {
    case -1: return true;
    case 0: return false;
    }
    if(bounds && v == min) {
        _searchMin();
        solver->enqueue(evMin);
    }
    if(bounds && v == max) {
        _searchMax();
        solver->enqueue(evMax);
    }
    solver->enqueue(evChange);
    if(size == 1)
        solver->enqueue(evBind);
    return true;
}

bool VarInt::restrictToMarks() {
    int m = marked, mmin = markedmin, mmax = markedmax;
    clearMarks();
    if(m != size) {
        size = m;
        if(m == 0)
            return false;
        if(bounds && min != mmin) {
            min = mmin;
            solver->enqueue(evMin);
        }
        if(bounds && max != mmax) {
            max = mmax;
            solver->enqueue(evMax);
        }
        solver->enqueue(evChange);
        if(m == 1)
            solver->enqueue(evBind);
    }
    return true;
}

bool VarInt::updateMin(int v) {
    clearMarks();
    if(!bounds)
        _searchMin();
    if(v <= min)
        return true;
    while(v > min) {
        if(!_remove(min))
            return false;
        _searchMin();
    }
    solver->enqueue(evChange);
    if(bounds)
        solver->enqueue(evMin);
    if(size == 1)
        solver->enqueue(evBind);
    return true;
}

bool VarInt::updateMax(int v) {
    clearMarks();
    if(!bounds)
        _searchMax();
    if(v >= max)
        return true;
    while(v < max) {
        if(!_remove(max))
            return false;
        _searchMax();
    }
    solver->enqueue(evChange);
    if(bounds)
        solver->enqueue(evMax);
    if(size == 1)
        solver->enqueue(evBind);
    return true;
}

inline void VarInt::_searchMin() {
    if(max - min + 1 > 2 * size) { // TODO check this condition
        // lots of "holes" in the domain: check all remaining values
        min = maxVal + 1;
        for(int i = 0; i < size; i++)
            if(domain[i] < min)
                min = domain[i];
    } else {
        // else: check values successively from current min upwards
        while(true) {
            min++;
            if(contains(min))
                return;
        }
    }
}

inline void VarInt::_searchMax() {
    if(max - min + 1 > 2 * size) { // TODO check this condition
        // lots of "holes" in the domain: check all remaining values
        max = minVal - 1;
        for(int i = 0; i < size; i++)
            if(domain[i] > max)
                max = domain[i];
    } else {
        // else: check values successively from current max downwards
        while(true) {
            max--;
            if(contains(max))
                return;
        }
    }
}

}
