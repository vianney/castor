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
#include "varint.h"

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
    marked = 0;
}

VarInt::~VarInt() {
    delete[] domain;
    delete[] map;
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
    solver->enqueue(evChange);
    if(size == 1)
        solver->enqueue(evBind);
    return true;
}

bool VarInt::restrictToMarks() {
    int m = marked;
    clearMarks();
    if(m != size) {
        size = m;
        if(m == 0)
            return false;
        solver->enqueue(evChange);
        if(m == 1)
            solver->enqueue(evBind);
    }
    return true;
}

}
