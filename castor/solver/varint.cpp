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

#include "../config.h"

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
    min = minVal;
    max = maxVal;
    clearMarks();
    strategy = SELECT_RANDOM;
}

VarInt::~VarInt() {
    delete[] domain;
    delete[] map;
}

int VarInt::select() {
    switch(strategy) {
    case SELECT_RANDOM:
        return domain[0];
    case SELECT_MIN:
        return min; // FIXME handle inconsistent value
    case SELECT_MAX:
        return max; // FIXME handle inconsistent value
    }
    assert(false); // should not happen
    return 0;
}

void VarInt::mark(int v) {
    if(v < min || v > max)
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
    if(v < min || v > max)
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

bool VarInt::remove(int v) {
    clearMarks();
    if(v < minVal || v > maxVal)
        return true;
    int i = map[v-minVal];
    if(i >= size)
        return true;
    if(size <= 1)
        return false;
    size--;
    if(i != size) {
        int v2 = domain[size];
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
            min++; // not perfect bound
            solver->enqueue(evMin);
        }
        if(v == max) {
            max--; // not perfect bound
            solver->enqueue(evMax);
        }
    }
    solver->enqueue(evChange);
    return true;
}

bool VarInt::restrictToMarks() {
    int m = marked, mmin = markedmin, mmax = markedmax;
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

bool VarInt::updateMin(int v) {
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

bool VarInt::updateMax(int v) {
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
