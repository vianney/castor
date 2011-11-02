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

/**
 * Sum a[i] * x[i] = b
 */
class SumConstraint : public StatelessConstraint {
    int n, *a;
    VarInt **x;
    int b;

public:
    SumConstraint(int n, int *a, VarInt **x, int b) : n(n), a(a), x(x), b(b) {
        for(int i = 0; i < n; i++) {
            x[i]->registerMin(this);
            x[i]->registerMax(this);
        }
    }

    bool propagate() {
        int minSum = b, maxSum = b;
        for(int i = 0; i < n; i++) {
            if(a[i] > 0) {
                minSum -= a[i] * x[i]->getMax();
                maxSum -= a[i] * x[i]->getMin();
            } else {
                minSum -= a[i] * x[i]->getMin();
                maxSum -= a[i] * x[i]->getMax();
            }
        }
        for(int i = 0; i < n; i++) {
            if(a[i] == 0)
                continue;
            int resMinSum, resMaxSum;
            if(a[i] > 0) {
                resMinSum = minSum + a[i] * x[i]->getMax();
                resMaxSum = maxSum + a[i] * x[i]->getMin();
            } else {
                resMinSum = maxSum + a[i] * x[i]->getMax();
                resMaxSum = minSum + a[i] * x[i]->getMin();
            }
            if(!x[i]->updateMin(resMinSum / a[i] + (resMinSum % a[i] ? 1 : 0)))
                return false;
            if(!x[i]->updateMax(resMaxSum / a[i]))
                return false;
        }
        return true;
    }
};

class Maximize : public Constraint {
    VarInt *x;
    int currentBound;

public:
    Maximize(VarInt *x) : x(x) {
        currentBound = x->getMin();
    }

    void update() {
        currentBound = x->getValue() + 1;
        solver->refresh(this);
    }

    bool post() {
        return x->updateMin(currentBound);
    }
};

int main(int argc, const char* argv[]) {
    int n = 8;
    if(argc >= 2)
        n = atoi(argv[1]);

    Solver solver;
    VarInt **vars = new VarInt*[n+1];
    for(int i = 0; i < n; i++)
        vars[i] = new VarInt(&solver, 0, n-1);
    vars[n] = new VarInt(&solver, 0, n*n*n);

    Subtree sub(&solver, vars, n+1, n);
    for(int i = 0; i < n - 1; i++) {
        for(int j = i + 1; j < n; j++) {
            sub.add(new DiffConstraint(vars[i], vars[j], 0));
            sub.add(new DiffConstraint(vars[i], vars[j], i - j));
            sub.add(new DiffConstraint(vars[i], vars[j], j - i));
        }
    }

    sub.add(new LessConstraint(vars[0], vars[1]));

    int *a = new int[n+1];
    for(int i = 0; i < n; i++) a[i] = i;
    a[n] = -1;
    sub.add(new SumConstraint(n+1, a, vars, 0));

    Maximize *max = new Maximize(vars[n]);
    solver.add(max);

    sub.activate();
    int nbSols = 0;
    while(sub.search()) {
        nbSols++;
        cout << "[";
        for(int i = 0; i < n; i++)
            cout << (i == 0 ? "" : ", ") << vars[i]->getValue();
        cout << "], sum = " << vars[n]->getValue() << endl;
        max->update();
    }
    cout << "Found " << nbSols << " solutions." << endl;

    cout << "Backtracks: " << solver.getStatBacktracks() << endl;
    cout << "Subtrees: " << solver.getStatSubtrees() << endl;
    cout << "Post: " << solver.getStatPost() << endl;
    cout << "Propagate: " << solver.getStatPropagate() << endl;

    for(int i = 0; i <= n; i++)
        delete vars[i];
    delete [] vars;
    delete [] a;
    return 0;
}
