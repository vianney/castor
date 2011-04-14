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
class DiffConstraint : public Constraint {
    VarInt *x, *y;
    int d;

public:
    DiffConstraint(VarInt *x, VarInt *y, int d) :
            x(x), y(y), d(d) {
        x->registerBind(this);
        y->registerBind(this);
    }

    bool post() {
        if(x->isBound() || y->isBound())
            return propagate();
        else
            return true;
    }

    bool propagate() {
        if(x->isBound())
            return y->remove(x->getValue() - d);
        else
            return x->remove(y->getValue() + d);
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
