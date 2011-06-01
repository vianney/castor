/* This file is part of Castor
 *
 * Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
 * Copyright (C) 2010-2011  UCLouvain
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

#include <cstdlib>
#include <iostream>
#include <vector>
#include "solver/solver.h"

using namespace std;
using namespace castor;

/**
 * Constraint x != y + d
 */
class DiffConstraint : public StatelessConstraint {
    VarInt *x, *y;
    int d;

public:
    DiffConstraint(VarInt *x, VarInt *y, int d) :
            x(x), y(y), d(d) {
        x->registerBind(this);
        y->registerBind(this);
    }

    bool propagate() {
        if(x->isBound())
            return y->remove(x->getValue() - d);
        else if(y->isBound())
            return x->remove(y->getValue() + d);
        return true;
    }
};

/**
 * Constraint x < y
 */
class LessConstraint : public StatelessConstraint {
    VarInt *x, *y;

public:
    LessConstraint(VarInt *x, VarInt *y) : x(x), y(y) {
        x->registerMin(this);
        x->registerMax(this);
        y->registerMin(this);
        y->registerMax(this);
    }

    void restore() {
        done = x->getMax() < y->getMin();
    }

    bool propagate() {
        StatelessConstraint::propagate();
        if(x->getMax() < y->getMin()) {
            done = true;
            return true;
        }
        if(!x->updateMax(y->getMax() - 1))
            return false;
        return y->updateMin(x->getMin() + 1);
    }
};

int main(int argc, const char* argv[]) {
    int n = 8;
    if(argc >= 2)
        n = atoi(argv[1]);

    Solver solver;
    VarInt **vars = new VarInt*[n];
    for(int i = 0; i < n; i++)
        vars[i] = new VarInt(&solver, 0, n-1);

    Subtree sub(&solver, vars, n);
    for(int i = 0; i < n - 1; i++) {
        for(int j = i + 1; j < n; j++) {
            sub.add(new DiffConstraint(vars[i], vars[j], 0));
            sub.add(new DiffConstraint(vars[i], vars[j], i - j));
            sub.add(new DiffConstraint(vars[i], vars[j], j - i));
        }
    }

    sub.add(new LessConstraint(vars[0], vars[1]));

    sub.activate();
    int nbSols = 0;
    while(sub.search()) {
        nbSols++;
        cout << "[";
        for(int i = 0; i < n; i++)
            cout << (i == 0 ? "" : ", ") << vars[i]->getValue();
        cout << "]" << endl;
    }
    cout << "Found " << nbSols << " solutions." << endl;

    for(int i = 0; i < n; i++)
        delete vars[i];
    delete [] vars;
    return 0;
}
