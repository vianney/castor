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
#include "triplecache.h"

#include <cstring>
#include <cassert>

namespace castor {

unsigned Triple::readPage(Cursor cur, Cursor end, Triple *triples) {
    Triple* it = triples;

    // read first triple
    Triple t;
    for(int i = 0; i < COMPONENTS; i++)
        t[i] = cur.readInt();
    (*it++) = t;

    // Unpack other triples
    while(cur < end) {
        unsigned header = cur.readByte();
        if(header < 0x80) {
            // small gap in last component
            if(header == 0)
                break;
            t[2] += header;
        } else {
            switch(header & 127) {
            case 0: t[2] += 128; break;
            case 1: t[2] += cur.readDelta1()+128; break;
            case 2: t[2] += cur.readDelta2()+128; break;
            case 3: t[2] += cur.readDelta3()+128; break;
            case 4: t[2] += cur.readDelta4()+128; break;
            case 5: t[1] += cur.readDelta1(); t[2] = 1; break;
            case 6: t[1] += cur.readDelta1(); t[2] = cur.readDelta1()+1; break;
            case 7: t[1] += cur.readDelta1(); t[2] = cur.readDelta2()+1; break;
            case 8: t[1] += cur.readDelta1(); t[2] = cur.readDelta3()+1; break;
            case 9: t[1] += cur.readDelta1(); t[2] = cur.readDelta4()+1; break;
            case 10: t[1] += cur.readDelta2(); t[2] = 1; break;
            case 11: t[1] += cur.readDelta2(); t[2] = cur.readDelta1()+1; break;
            case 12: t[1] += cur.readDelta2(); t[2] = cur.readDelta2()+1; break;
            case 13: t[1] += cur.readDelta2(); t[2] = cur.readDelta3()+1; break;
            case 14: t[1] += cur.readDelta2(); t[2] = cur.readDelta4()+1; break;
            case 15: t[1] += cur.readDelta3(); t[2] = 1; break;
            case 16: t[1] += cur.readDelta3(); t[2] = cur.readDelta1()+1; break;
            case 17: t[1] += cur.readDelta3(); t[2] = cur.readDelta2()+1; break;
            case 18: t[1] += cur.readDelta3(); t[2] = cur.readDelta3()+1; break;
            case 19: t[1] += cur.readDelta3(); t[2] = cur.readDelta4()+1; break;
            case 20: t[1] += cur.readDelta4(); t[2] = 1; break;
            case 21: t[1] += cur.readDelta4(); t[2] = cur.readDelta1()+1; break;
            case 22: t[1] += cur.readDelta4(); t[2] = cur.readDelta2()+1; break;
            case 23: t[1] += cur.readDelta4(); t[2] = cur.readDelta3()+1; break;
            case 24: t[1] += cur.readDelta4(); t[2] = cur.readDelta4()+1; break;
            case 25: t[0] += cur.readDelta1(); t[1] = 1; t[2] = 1; break;
            case 26: t[0] += cur.readDelta1(); t[1] = 1; t[2] = cur.readDelta1()+1; break;
            case 27: t[0] += cur.readDelta1(); t[1] = 1; t[2] = cur.readDelta2()+1; break;
            case 28: t[0] += cur.readDelta1(); t[1] = 1; t[2] = cur.readDelta3()+1; break;
            case 29: t[0] += cur.readDelta1(); t[1] = 1; t[2] = cur.readDelta4()+1; break;
            case 30: t[0] += cur.readDelta1(); t[1] = cur.readDelta1()+1; t[2] = 1; break;
            case 31: t[0] += cur.readDelta1(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta1()+1; break;
            case 32: t[0] += cur.readDelta1(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta2()+1; break;
            case 33: t[0] += cur.readDelta1(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta3()+1; break;
            case 34: t[0] += cur.readDelta1(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta4()+1; break;
            case 35: t[0] += cur.readDelta1(); t[1] = cur.readDelta2()+1; t[2] = 1; break;
            case 36: t[0] += cur.readDelta1(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta1()+1; break;
            case 37: t[0] += cur.readDelta1(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta2()+1; break;
            case 38: t[0] += cur.readDelta1(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta3()+1; break;
            case 39: t[0] += cur.readDelta1(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta4()+1; break;
            case 40: t[0] += cur.readDelta1(); t[1] = cur.readDelta3()+1; t[2] = 1; break;
            case 41: t[0] += cur.readDelta1(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta1()+1; break;
            case 42: t[0] += cur.readDelta1(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta2()+1; break;
            case 43: t[0] += cur.readDelta1(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta3()+1; break;
            case 44: t[0] += cur.readDelta1(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta4()+1; break;
            case 45: t[0] += cur.readDelta1(); t[1] = cur.readDelta4()+1; t[2] = 1; break;
            case 46: t[0] += cur.readDelta1(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta1()+1; break;
            case 47: t[0] += cur.readDelta1(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta2()+1; break;
            case 48: t[0] += cur.readDelta1(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta3()+1; break;
            case 49: t[0] += cur.readDelta1(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta4()+1; break;
            case 50: t[0] += cur.readDelta2(); t[1] = 1; t[2] = 1; break;
            case 51: t[0] += cur.readDelta2(); t[1] = 1; t[2] = cur.readDelta1()+1; break;
            case 52: t[0] += cur.readDelta2(); t[1] = 1; t[2] = cur.readDelta2()+1; break;
            case 53: t[0] += cur.readDelta2(); t[1] = 1; t[2] = cur.readDelta3()+1; break;
            case 54: t[0] += cur.readDelta2(); t[1] = 1; t[2] = cur.readDelta4()+1; break;
            case 55: t[0] += cur.readDelta2(); t[1] = cur.readDelta1()+1; t[2] = 1; break;
            case 56: t[0] += cur.readDelta2(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta1()+1; break;
            case 57: t[0] += cur.readDelta2(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta2()+1; break;
            case 58: t[0] += cur.readDelta2(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta3()+1; break;
            case 59: t[0] += cur.readDelta2(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta4()+1; break;
            case 60: t[0] += cur.readDelta2(); t[1] = cur.readDelta2()+1; t[2] = 1; break;
            case 61: t[0] += cur.readDelta2(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta1()+1; break;
            case 62: t[0] += cur.readDelta2(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta2()+1; break;
            case 63: t[0] += cur.readDelta2(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta3()+1; break;
            case 64: t[0] += cur.readDelta2(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta4()+1; break;
            case 65: t[0] += cur.readDelta2(); t[1] = cur.readDelta3()+1; t[2] = 1; break;
            case 66: t[0] += cur.readDelta2(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta1()+1; break;
            case 67: t[0] += cur.readDelta2(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta2()+1; break;
            case 68: t[0] += cur.readDelta2(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta3()+1; break;
            case 69: t[0] += cur.readDelta2(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta4()+1; break;
            case 70: t[0] += cur.readDelta2(); t[1] = cur.readDelta4()+1; t[2] = 1; break;
            case 71: t[0] += cur.readDelta2(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta1()+1; break;
            case 72: t[0] += cur.readDelta2(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta2()+1; break;
            case 73: t[0] += cur.readDelta2(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta3()+1; break;
            case 74: t[0] += cur.readDelta2(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta4()+1; break;
            case 75: t[0] += cur.readDelta3(); t[1] = 1; t[2] = 1; break;
            case 76: t[0] += cur.readDelta3(); t[1] = 1; t[2] = cur.readDelta1()+1; break;
            case 77: t[0] += cur.readDelta3(); t[1] = 1; t[2] = cur.readDelta2()+1; break;
            case 78: t[0] += cur.readDelta3(); t[1] = 1; t[2] = cur.readDelta3()+1; break;
            case 79: t[0] += cur.readDelta3(); t[1] = 1; t[2] = cur.readDelta4()+1; break;
            case 80: t[0] += cur.readDelta3(); t[1] = cur.readDelta1()+1; t[2] = 1; break;
            case 81: t[0] += cur.readDelta3(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta1()+1; break;
            case 82: t[0] += cur.readDelta3(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta2()+1; break;
            case 83: t[0] += cur.readDelta3(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta3()+1; break;
            case 84: t[0] += cur.readDelta3(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta4()+1; break;
            case 85: t[0] += cur.readDelta3(); t[1] = cur.readDelta2()+1; t[2] = 1; break;
            case 86: t[0] += cur.readDelta3(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta1()+1; break;
            case 87: t[0] += cur.readDelta3(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta2()+1; break;
            case 88: t[0] += cur.readDelta3(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta3()+1; break;
            case 89: t[0] += cur.readDelta3(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta4()+1; break;
            case 90: t[0] += cur.readDelta3(); t[1] = cur.readDelta3()+1; t[2] = 1; break;
            case 91: t[0] += cur.readDelta3(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta1()+1; break;
            case 92: t[0] += cur.readDelta3(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta2()+1; break;
            case 93: t[0] += cur.readDelta3(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta3()+1; break;
            case 94: t[0] += cur.readDelta3(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta4()+1; break;
            case 95: t[0] += cur.readDelta3(); t[1] = cur.readDelta4()+1; t[2] = 1; break;
            case 96: t[0] += cur.readDelta3(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta1()+1; break;
            case 97: t[0] += cur.readDelta3(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta2()+1; break;
            case 98: t[0] += cur.readDelta3(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta3()+1; break;
            case 99: t[0] += cur.readDelta3(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta4()+1; break;
            case 100: t[0] += cur.readDelta4(); t[1] = 1; t[2] = 1; break;
            case 101: t[0] += cur.readDelta4(); t[1] = 1; t[2] = cur.readDelta1()+1; break;
            case 102: t[0] += cur.readDelta4(); t[1] = 1; t[2] = cur.readDelta2()+1; break;
            case 103: t[0] += cur.readDelta4(); t[1] = 1; t[2] = cur.readDelta3()+1; break;
            case 104: t[0] += cur.readDelta4(); t[1] = 1; t[2] = cur.readDelta4()+1; break;
            case 105: t[0] += cur.readDelta4(); t[1] = cur.readDelta1()+1; t[2] = 1; break;
            case 106: t[0] += cur.readDelta4(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta1()+1; break;
            case 107: t[0] += cur.readDelta4(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta2()+1; break;
            case 108: t[0] += cur.readDelta4(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta3()+1; break;
            case 109: t[0] += cur.readDelta4(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta4()+1; break;
            case 110: t[0] += cur.readDelta4(); t[1] = cur.readDelta2()+1; t[2] = 1; break;
            case 111: t[0] += cur.readDelta4(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta1()+1; break;
            case 112: t[0] += cur.readDelta4(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta2()+1; break;
            case 113: t[0] += cur.readDelta4(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta3()+1; break;
            case 114: t[0] += cur.readDelta4(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta4()+1; break;
            case 115: t[0] += cur.readDelta4(); t[1] = cur.readDelta3()+1; t[2] = 1; break;
            case 116: t[0] += cur.readDelta4(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta1()+1; break;
            case 117: t[0] += cur.readDelta4(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta2()+1; break;
            case 118: t[0] += cur.readDelta4(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta3()+1; break;
            case 119: t[0] += cur.readDelta4(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta4()+1; break;
            case 120: t[0] += cur.readDelta4(); t[1] = cur.readDelta4()+1; t[2] = 1; break;
            case 121: t[0] += cur.readDelta4(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta1()+1; break;
            case 122: t[0] += cur.readDelta4(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta2()+1; break;
            case 123: t[0] += cur.readDelta4(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta3()+1; break;
            case 124: t[0] += cur.readDelta4(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta4()+1; break;
            default: assert(false); // should not happen
            }
        }
        (*it++) = t;
    }

    return it - triples;
}


unsigned AggregatedTriple::readPage(Cursor cur, Cursor end, Triple *triples) {
    Triple* it = triples;

    // read first triple
    AggregatedTriple t;
    for(int i = 0; i < COMPONENTS; i++)
        t[i] = cur.readInt();
    (*it++) = t;

    // Unpack other triples
    while(cur < end) {
        unsigned header = cur.readByte();
        if(header < 0x80) {
            // small gap in last component
            if(header == 0)
                break;
            t[1] += header & 31;
            t[2] = (header >> 5) + 1;
        } else {
            switch(header & 127) {
            case 0: t[1] += 1; t[2] = 1; break;
            case 1: t[1] += 1; t[2] = cur.readDelta1()+1; break;
            case 2: t[1] += 1; t[2] = cur.readDelta2()+1; break;
            case 3: t[1] += 1; t[2] = cur.readDelta3()+1; break;
            case 4: t[1] += 1; t[2] = cur.readDelta4()+1; break;
            case 5: t[1] += cur.readDelta1()+1; t[2] = 1; break;
            case 6: t[1] += cur.readDelta1()+1; t[2] = cur.readDelta1()+1; break;
            case 7: t[1] += cur.readDelta1()+1; t[2] = cur.readDelta2()+1; break;
            case 8: t[1] += cur.readDelta1()+1; t[2] = cur.readDelta3()+1; break;
            case 9: t[1] += cur.readDelta1()+1; t[2] = cur.readDelta4()+1; break;
            case 10: t[1] += cur.readDelta2()+1; t[2] = 1; break;
            case 11: t[1] += cur.readDelta2()+1; t[2] = cur.readDelta1()+1; break;
            case 12: t[1] += cur.readDelta2()+1; t[2] = cur.readDelta2()+1; break;
            case 13: t[1] += cur.readDelta2()+1; t[2] = cur.readDelta3()+1; break;
            case 14: t[1] += cur.readDelta2()+1; t[2] = cur.readDelta4()+1; break;
            case 15: t[1] += cur.readDelta3()+1; t[2] = 1; break;
            case 16: t[1] += cur.readDelta3()+1; t[2] = cur.readDelta1()+1; break;
            case 17: t[1] += cur.readDelta3()+1; t[2] = cur.readDelta2()+1; break;
            case 18: t[1] += cur.readDelta3()+1; t[2] = cur.readDelta3()+1; break;
            case 19: t[1] += cur.readDelta3()+1; t[2] = cur.readDelta4()+1; break;
            case 20: t[1] += cur.readDelta4()+1; t[2] = 1; break;
            case 21: t[1] += cur.readDelta4()+1; t[2] = cur.readDelta1()+1; break;
            case 22: t[1] += cur.readDelta4()+1; t[2] = cur.readDelta2()+1; break;
            case 23: t[1] += cur.readDelta4()+1; t[2] = cur.readDelta3()+1; break;
            case 24: t[1] += cur.readDelta4()+1; t[2] = cur.readDelta4()+1; break;
            case 25: t[0] += cur.readDelta1(); t[1] = 1; t[2] = 1; break;
            case 26: t[0] += cur.readDelta1(); t[1] = 1; t[2] = cur.readDelta1()+1; break;
            case 27: t[0] += cur.readDelta1(); t[1] = 1; t[2] = cur.readDelta2()+1; break;
            case 28: t[0] += cur.readDelta1(); t[1] = 1; t[2] = cur.readDelta3()+1; break;
            case 29: t[0] += cur.readDelta1(); t[1] = 1; t[2] = cur.readDelta4()+1; break;
            case 30: t[0] += cur.readDelta1(); t[1] = cur.readDelta1()+1; t[2] = 1; break;
            case 31: t[0] += cur.readDelta1(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta1()+1; break;
            case 32: t[0] += cur.readDelta1(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta2()+1; break;
            case 33: t[0] += cur.readDelta1(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta3()+1; break;
            case 34: t[0] += cur.readDelta1(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta4()+1; break;
            case 35: t[0] += cur.readDelta1(); t[1] = cur.readDelta2()+1; t[2] = 1; break;
            case 36: t[0] += cur.readDelta1(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta1()+1; break;
            case 37: t[0] += cur.readDelta1(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta2()+1; break;
            case 38: t[0] += cur.readDelta1(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta3()+1; break;
            case 39: t[0] += cur.readDelta1(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta4()+1; break;
            case 40: t[0] += cur.readDelta1(); t[1] = cur.readDelta3()+1; t[2] = 1; break;
            case 41: t[0] += cur.readDelta1(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta1()+1; break;
            case 42: t[0] += cur.readDelta1(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta2()+1; break;
            case 43: t[0] += cur.readDelta1(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta3()+1; break;
            case 44: t[0] += cur.readDelta1(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta4()+1; break;
            case 45: t[0] += cur.readDelta1(); t[1] = cur.readDelta4()+1; t[2] = 1; break;
            case 46: t[0] += cur.readDelta1(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta1()+1; break;
            case 47: t[0] += cur.readDelta1(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta2()+1; break;
            case 48: t[0] += cur.readDelta1(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta3()+1; break;
            case 49: t[0] += cur.readDelta1(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta4()+1; break;
            case 50: t[0] += cur.readDelta2(); t[1] = 1; t[2] = 1; break;
            case 51: t[0] += cur.readDelta2(); t[1] = 1; t[2] = cur.readDelta1()+1; break;
            case 52: t[0] += cur.readDelta2(); t[1] = 1; t[2] = cur.readDelta2()+1; break;
            case 53: t[0] += cur.readDelta2(); t[1] = 1; t[2] = cur.readDelta3()+1; break;
            case 54: t[0] += cur.readDelta2(); t[1] = 1; t[2] = cur.readDelta4()+1; break;
            case 55: t[0] += cur.readDelta2(); t[1] = cur.readDelta1()+1; t[2] = 1; break;
            case 56: t[0] += cur.readDelta2(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta1()+1; break;
            case 57: t[0] += cur.readDelta2(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta2()+1; break;
            case 58: t[0] += cur.readDelta2(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta3()+1; break;
            case 59: t[0] += cur.readDelta2(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta4()+1; break;
            case 60: t[0] += cur.readDelta2(); t[1] = cur.readDelta2()+1; t[2] = 1; break;
            case 61: t[0] += cur.readDelta2(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta1()+1; break;
            case 62: t[0] += cur.readDelta2(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta2()+1; break;
            case 63: t[0] += cur.readDelta2(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta3()+1; break;
            case 64: t[0] += cur.readDelta2(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta4()+1; break;
            case 65: t[0] += cur.readDelta2(); t[1] = cur.readDelta3()+1; t[2] = 1; break;
            case 66: t[0] += cur.readDelta2(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta1()+1; break;
            case 67: t[0] += cur.readDelta2(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta2()+1; break;
            case 68: t[0] += cur.readDelta2(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta3()+1; break;
            case 69: t[0] += cur.readDelta2(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta4()+1; break;
            case 70: t[0] += cur.readDelta2(); t[1] = cur.readDelta4()+1; t[2] = 1; break;
            case 71: t[0] += cur.readDelta2(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta1()+1; break;
            case 72: t[0] += cur.readDelta2(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta2()+1; break;
            case 73: t[0] += cur.readDelta2(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta3()+1; break;
            case 74: t[0] += cur.readDelta2(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta4()+1; break;
            case 75: t[0] += cur.readDelta3(); t[1] = 1; t[2] = 1; break;
            case 76: t[0] += cur.readDelta3(); t[1] = 1; t[2] = cur.readDelta1()+1; break;
            case 77: t[0] += cur.readDelta3(); t[1] = 1; t[2] = cur.readDelta2()+1; break;
            case 78: t[0] += cur.readDelta3(); t[1] = 1; t[2] = cur.readDelta3()+1; break;
            case 79: t[0] += cur.readDelta3(); t[1] = 1; t[2] = cur.readDelta4()+1; break;
            case 80: t[0] += cur.readDelta3(); t[1] = cur.readDelta1()+1; t[2] = 1; break;
            case 81: t[0] += cur.readDelta3(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta1()+1; break;
            case 82: t[0] += cur.readDelta3(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta2()+1; break;
            case 83: t[0] += cur.readDelta3(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta3()+1; break;
            case 84: t[0] += cur.readDelta3(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta4()+1; break;
            case 85: t[0] += cur.readDelta3(); t[1] = cur.readDelta2()+1; t[2] = 1; break;
            case 86: t[0] += cur.readDelta3(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta1()+1; break;
            case 87: t[0] += cur.readDelta3(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta2()+1; break;
            case 88: t[0] += cur.readDelta3(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta3()+1; break;
            case 89: t[0] += cur.readDelta3(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta4()+1; break;
            case 90: t[0] += cur.readDelta3(); t[1] = cur.readDelta3()+1; t[2] = 1; break;
            case 91: t[0] += cur.readDelta3(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta1()+1; break;
            case 92: t[0] += cur.readDelta3(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta2()+1; break;
            case 93: t[0] += cur.readDelta3(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta3()+1; break;
            case 94: t[0] += cur.readDelta3(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta4()+1; break;
            case 95: t[0] += cur.readDelta3(); t[1] = cur.readDelta4()+1; t[2] = 1; break;
            case 96: t[0] += cur.readDelta3(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta1()+1; break;
            case 97: t[0] += cur.readDelta3(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta2()+1; break;
            case 98: t[0] += cur.readDelta3(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta3()+1; break;
            case 99: t[0] += cur.readDelta3(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta4()+1; break;
            case 100: t[0] += cur.readDelta4(); t[1] = 1; t[2] = 1; break;
            case 101: t[0] += cur.readDelta4(); t[1] = 1; t[2] = cur.readDelta1()+1; break;
            case 102: t[0] += cur.readDelta4(); t[1] = 1; t[2] = cur.readDelta2()+1; break;
            case 103: t[0] += cur.readDelta4(); t[1] = 1; t[2] = cur.readDelta3()+1; break;
            case 104: t[0] += cur.readDelta4(); t[1] = 1; t[2] = cur.readDelta4()+1; break;
            case 105: t[0] += cur.readDelta4(); t[1] = cur.readDelta1()+1; t[2] = 1; break;
            case 106: t[0] += cur.readDelta4(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta1()+1; break;
            case 107: t[0] += cur.readDelta4(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta2()+1; break;
            case 108: t[0] += cur.readDelta4(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta3()+1; break;
            case 109: t[0] += cur.readDelta4(); t[1] = cur.readDelta1()+1; t[2] = cur.readDelta4()+1; break;
            case 110: t[0] += cur.readDelta4(); t[1] = cur.readDelta2()+1; t[2] = 1; break;
            case 111: t[0] += cur.readDelta4(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta1()+1; break;
            case 112: t[0] += cur.readDelta4(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta2()+1; break;
            case 113: t[0] += cur.readDelta4(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta3()+1; break;
            case 114: t[0] += cur.readDelta4(); t[1] = cur.readDelta2()+1; t[2] = cur.readDelta4()+1; break;
            case 115: t[0] += cur.readDelta4(); t[1] = cur.readDelta3()+1; t[2] = 1; break;
            case 116: t[0] += cur.readDelta4(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta1()+1; break;
            case 117: t[0] += cur.readDelta4(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta2()+1; break;
            case 118: t[0] += cur.readDelta4(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta3()+1; break;
            case 119: t[0] += cur.readDelta4(); t[1] = cur.readDelta3()+1; t[2] = cur.readDelta4()+1; break;
            case 120: t[0] += cur.readDelta4(); t[1] = cur.readDelta4()+1; t[2] = 1; break;
            case 121: t[0] += cur.readDelta4(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta1()+1; break;
            case 122: t[0] += cur.readDelta4(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta2()+1; break;
            case 123: t[0] += cur.readDelta4(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta3()+1; break;
            case 124: t[0] += cur.readDelta4(); t[1] = cur.readDelta4()+1; t[2] = cur.readDelta4()+1; break;
            default: assert(false); // should not happen
            }
        }
        (*it++) = t;
    }

    return it - triples;
}

unsigned FullyAggregatedTriple::readPage(Cursor cur, Cursor end, Triple *triples) {
    Triple* it = triples;

    // read first triple
    AggregatedTriple t;
    for(int i = 0; i < COMPONENTS; i++)
        t[i] = cur.readInt();
    (*it++) = t;

    // Unpack other triples
    while(cur < end) {
        unsigned header = cur.readByte();
        if(header < 0x80) {
            // small gap in last component
            if(header == 0)
                break;
            t[0] += header & 15;
            t[1] = (header >> 4) + 1;
        } else {
            switch(header & 127) {
            case 0: t[0] += 1; t[1] = 1; break;
            case 1: t[0] += 1; t[1] = cur.readDelta1()+1; break;
            case 2: t[0] += 1; t[1] = cur.readDelta2()+1; break;
            case 3: t[0] += 1; t[1] = cur.readDelta3()+1; break;
            case 4: t[0] += 1; t[1] = cur.readDelta4()+1; break;
            case 5: t[0] += cur.readDelta1()+1; t[1] = 1; break;
            case 6: t[0] += cur.readDelta1()+1; t[1] = cur.readDelta1()+1; break;
            case 7: t[0] += cur.readDelta1()+1; t[1] = cur.readDelta2()+1; break;
            case 8: t[0] += cur.readDelta1()+1; t[1] = cur.readDelta3()+1; break;
            case 9: t[0] += cur.readDelta1()+1; t[1] = cur.readDelta4()+1; break;
            case 10: t[0] += cur.readDelta2()+1; t[1] = 1; break;
            case 11: t[0] += cur.readDelta2()+1; t[1] = cur.readDelta1()+1; break;
            case 12: t[0] += cur.readDelta2()+1; t[1] = cur.readDelta2()+1; break;
            case 13: t[0] += cur.readDelta2()+1; t[1] = cur.readDelta3()+1; break;
            case 14: t[0] += cur.readDelta2()+1; t[1] = cur.readDelta4()+1; break;
            case 15: t[0] += cur.readDelta3()+1; t[1] = 1; break;
            case 16: t[0] += cur.readDelta3()+1; t[1] = cur.readDelta1()+1; break;
            case 17: t[0] += cur.readDelta3()+1; t[1] = cur.readDelta2()+1; break;
            case 18: t[0] += cur.readDelta3()+1; t[1] = cur.readDelta3()+1; break;
            case 19: t[0] += cur.readDelta3()+1; t[1] = cur.readDelta4()+1; break;
            case 20: t[0] += cur.readDelta4()+1; t[1] = 1; break;
            case 21: t[0] += cur.readDelta4()+1; t[1] = cur.readDelta1()+1; break;
            case 22: t[0] += cur.readDelta4()+1; t[1] = cur.readDelta2()+1; break;
            case 23: t[0] += cur.readDelta4()+1; t[1] = cur.readDelta3()+1; break;
            case 24: t[0] += cur.readDelta4()+1; t[1] = cur.readDelta4()+1; break;
            default: assert(false); // should not happen
            }
        }
        (*it++) = t;
    }

    return it - triples;
}

TripleCache::TripleCache() {
    statHits_ = 0;
    statMisses_ = 0;
}

TripleCache::~TripleCache() {
    delete [] map_;
    // Clean used lines
    for(Line* line : lines_)
        delete line;
}

void TripleCache::initialize(PageReader* db, unsigned maxPage, unsigned initCapacity) {
    db_ = db;
    lines_.reserve(initCapacity);
    map_ = new Line*[maxPage + 1];
    memset(map_, 0, (maxPage + 1) * sizeof(Line*));
    head_ = nullptr;
    tail_ = nullptr;
}

void TripleCache::release(const Line* cline) {
    Line* line = const_cast<Line*>(cline);
    assert(line->uses_ > 0);
    --line->uses_;
    if(line->uses_ == 0) {
        // add cache line to head of LRU list
        if(head_ == nullptr) {
            head_ = tail_ = line;
            line->prev_ = nullptr;
            line->next_ = nullptr;
        } else {
            head_->prev_ = line;
            line->next_ = head_;
            line->prev_ = nullptr;
            head_ = line;
        }
    }
}

void TripleCache::peek(unsigned page, bool& first, bool& last, Triple& firstKey) {
    Cursor cur = db_->page(page);
    BTreeFlags flags = cur.readInt();
    assert(!flags.inner());
    first = flags.firstLeaf();
    last = flags.lastLeaf();
    for(int i = 0; i < firstKey.COMPONENTS; i++)
        firstKey[i] = cur.readInt();
}

}
