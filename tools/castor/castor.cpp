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
#include <iostream>
#include <iomanip>
#include <fstream>
#include "store.h"
#include "query.h"
#include <sys/time.h>
#include <sys/resource.h>

#ifdef CASTOR_CSTR_TIMING
#include "constraints/triple.h"
#endif

using namespace std;
using namespace castor;

inline long diffTime(const rusage& start, const rusage& stop) {
    return ((long)(stop.ru_utime.tv_sec + stop.ru_stime.tv_sec -
                   start.ru_utime.tv_sec - start.ru_stime.tv_sec) * 1000L +
            (long)(stop.ru_utime.tv_usec + stop.ru_stime.tv_usec -
                   start.ru_utime.tv_usec - start.ru_stime.tv_usec) / 1000L);
}

inline void printTime(const char* msg, long time) {
    cout << msg << ": " << (time / 1000) << "." <<
            setw(3) << setfill('0') << (time % 1000) << endl;
}

int main(int argc, char* argv[]) {
    if(argc < 3 || argc > 4) {
        cout << "Usage: " << argv[0] << " DB QUERY [SOL]" << endl;
        return 1;
    }
    char* dbpath = argv[1];
    char* rqpath = argv[2];
    char* solpath = argc > 3 ? argv[3] : nullptr;

    ifstream f(rqpath, ios::ate);
    unsigned queryLen = f.tellg();
    if(queryLen == 0) {
        cerr << "Empty query" << endl;
        return 2;
    }
    char* queryString = new char[queryLen + 1];
    f.seekg(0, ios::beg);
    f.read(queryString, queryLen);
    queryString[queryLen] = '\0';
    f.close();

    ostream* fsol = (solpath == nullptr ? &cout : new ofstream(solpath));

    rusage ru[4];
    getrusage(RUSAGE_SELF, &ru[0]);

    Store store(dbpath);

    getrusage(RUSAGE_SELF, &ru[1]);
    printTime("Store open", diffTime(ru[0], ru[1]));

    Query query(&store, queryString);
    delete [] queryString;
    cout << query << endl;

    getrusage(RUSAGE_SELF, &ru[2]);
    printTime("Query init", diffTime(ru[1], ru[2]));

    while(query.next()) {
        if(query.requested() == 0) {
            *fsol << "YES" << endl;
        } else {
            for(unsigned i = 0; i < query.requested(); i++) {
                Value::id_t id = query.variable(i)->valueId();
                if(id == 0) {
                    *fsol << " ";
                } else {
                    *fsol << store.lookupValue(id).ensureDirectStrings(store)
                          << " ";
                }
            }
            *fsol << endl;
        }
    }

    getrusage(RUSAGE_SELF, &ru[3]);
    printTime("Search", diffTime(ru[2], ru[3]));

    if(query.requested() == 0 && query.count() == 0)
        *fsol << "NO" << endl;

    if(solpath != nullptr) {
        ofstream* f = static_cast<ofstream*>(fsol);
        f->close();
        delete f;
    }

    cout << "Found: " << query.count() << endl;
    cout << "Time: " << diffTime(ru[1], ru[3]) << endl;
    cout << "Memory: " << ru[3].ru_maxrss << endl;

    cout << "Backtracks: " << query.solver()->statBacktracks() << endl;
    cout << "Subtrees: " << query.solver()->statSubtrees() << endl;
    cout << "Post: " << query.solver()->statPost() << endl;
    cout << "Propagate: " << query.solver()->statPropagate() << endl;

    cout << "Cache hit: " << store.statTripleCacheHits() << endl;
    cout << "Cache miss: " << store.statTripleCacheMisses() << endl;

#ifdef CASTOR_CSTR_TIMING
    cout << "TripleConstraint propagations: "
         << TripleConstraint::count[0] << " (" << TripleConstraint::time[0] << "ms), "
         << TripleConstraint::count[1] << " (" << TripleConstraint::time[1] << "ms), "
         << TripleConstraint::count[2] << " (" << TripleConstraint::time[2] << "ms)" << endl;
#endif

    return 0;
}
