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

#include <iostream>
#include <iomanip>
#include <fstream>
#include "query.h"
#include <sys/time.h>
#include <sys/resource.h>

using namespace std;
using namespace castor;

inline long diffTime(rusage &start, rusage &stop) {
    return ((long)(stop.ru_utime.tv_sec + stop.ru_stime.tv_sec -
                   start.ru_utime.tv_sec - start.ru_stime.tv_sec) * 1000L +
            (long)(stop.ru_utime.tv_usec + stop.ru_stime.tv_usec -
                   start.ru_utime.tv_usec - start.ru_stime.tv_usec) / 1000L);
}

inline void printTime(const char *msg, long time) {
    cout << msg << ": " << (time / 1000) << "." <<
            setw(3) << setfill('0') << (time % 1000) << endl;
}

int main(int argc, char *argv[]) {
    if(argc < 3 || argc > 4) {
        cout << "Usage: " << argv[0] << " DB QUERY [SOL]" << endl;
        return 1;
    }
    char *dbpath = argv[1];
    char *rqpath = argv[2];
    char *solpath = argc > 3 ? argv[3] : NULL;

    ifstream f(rqpath, ios::ate);
    int queryLen = (int) f.tellg();
    if(queryLen == 0) {
        cerr << "Empty query" << endl;
        return 2;
    }
    char *queryString = new char[queryLen + 1];
    f.seekg(0, ios::beg);
    f.read(queryString, queryLen);
    queryString[queryLen] = '\0';
    f.close();

    ostream *fsol = (solpath == NULL ? &cout : new ofstream(solpath));

    rusage ru[4];
    getrusage(RUSAGE_SELF, &ru[0]);

    // disable sqlite mutexes, assuming we are single-threaded
//    sqlite3_config(SQLITE_CONFIG_SINGLETHREAD);

    Store store(dbpath);

    getrusage(RUSAGE_SELF, &ru[1]);
    printTime("Store open", diffTime(ru[0], ru[1]));

    Query query(&store, queryString);
    delete queryString;
    cout << query << endl;

    getrusage(RUSAGE_SELF, &ru[2]);
    printTime("Query init", diffTime(ru[1], ru[2]));

    while(query.next()) {
        if(query.getRequestedCount() == 0) {
            *fsol << "YES" << endl;
        } else {
            for(int i = 0; i < query.getRequestedCount(); i++)
                *fsol << query.getVariable(i)->getValue()->getString() << " ";
            *fsol << endl;
        }
    }

    getrusage(RUSAGE_SELF, &ru[3]);
    printTime("Search", diffTime(ru[2], ru[3]));

    if(query.getRequestedCount() == 0 && query.getSolutionCount() == 0)
        *fsol << "NO" << endl;

    if(solpath != NULL) {
        ((ofstream*) fsol)->close();
        delete (ofstream*) fsol;
    }

    cout << "Found: " << query.getSolutionCount() << endl;
    cout << "Time: " << diffTime(ru[1], ru[3]) << endl;
    cout << "Memory: " << ru[3].ru_maxrss << endl;

    cout << "Backtracks: " << query.getSolver()->getStatBacktracks() << endl;
    cout << "Subtrees: " << query.getSolver()->getStatSubtrees() << endl;
    cout << "Propagate: " << query.getSolver()->getStatPropagate() << endl;

    return 0;
}
