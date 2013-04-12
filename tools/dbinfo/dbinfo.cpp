/* This file is part of Castor
 *
 * Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
 * Copyright (C) 2010-2013, Université catholique de Louvain
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
#include <cstdlib>
#include <unistd.h>
#include "store.h"

using namespace std;
using namespace castor;

static const char* progname;

void usage() {
    cout << "Usage: " << progname << " DB [switches...]" << endl;
    cout << endl << "Switches:" << endl;
    cout << "  -i            Show general information" << endl;
    cout << "  -T            Show all triples" << endl;
    cout << "  -V            Show all values" << endl;
    cout << "  -v ID         Show value with id ID" << endl;
    cout << "  -s ID         Show string with id ID" << endl;
    exit(1);
}

void error(const char* msg) {
    cerr << "ERROR: " << msg << endl;
    exit(2);
}

void show_info(Store& store) {
    cout << "Strings count: " << store.stringsCount() << endl;
    cout << "Values count: " << store.valuesCount() << endl;
    cout << "Triples count: " << store.triplesCount(Triple({0,0,0})) << endl;
}

void show_triples(Store& store) {
    for(unsigned i = 0; i < store.triplesCount(); ++i) {
        Triple t = store.triple(i);
        cout << t[0] << " " << t[1] << " " << t[2] << endl;
    }
}

void show_values(Store& store) {
    for(Value::id_t id = 1; id <= store.valuesCount(); ++id) {
        Value v = store.lookupValue(id);
        v.ensureDirectStrings(store);
        cout << id << " " << v << endl;
    }
}

void show_value(Store& store, Value::id_t id) {
    if(id < 1 || id > store.valuesCount())
        error("Invalid id");
    Value v = store.lookupValue(id);
    v.ensureDirectStrings(store);
    cout << v << endl;
    cout << "Hash: " << hex << v.hash() << dec << endl;
    cout << "Category: " << v.category();
    if(v.isNumeric())
        cout << " (" << v.numCategory() << ")";
    cout << endl;
    cout << "Lexical: " << v.lexical().id() << endl;
    if(v.isTyped()) {
        cout << "Datatype: " << v.datatypeId() << endl;
        cout << "Datatype lex: " << v.datatypeLex().id() << endl;
    } else if(v.isPlainWithLang()) {
        cout << "Language tag: " << v.language().id() << endl;
    }
    if(v.isNumeric())
        cout << "Approximated range: " << v.numapprox() << endl;
}

void show_string(Store& store, String::id_t id) {
    if(id < 1 || id > store.stringsCount())
        error("Invalid id");
    String s = store.lookupString(id);
    cout << s << endl;
    cout << "Hash: " << hex << s.hash() << dec << endl;
}

int main(int argc, char* argv[]) {
    progname = argv[0];
    if(argc < 2)
        usage();

    const char* dbpath = argv[1];
    Store store(dbpath);

    optind = 2;
    int c;
    while((c = getopt(argc, argv, "iTVv:s:")) != -1) {
        switch(c) {
        case 'i': show_info  (store);                break;
        case 'T': show_triples(store);               break;
        case 'V': show_values(store);                break;
        case 'v': show_value (store, atoi(optarg));  break;
        case 's': show_string(store, atoi(optarg));  break;
        default: usage();
        }
    }
    return 0;
}
