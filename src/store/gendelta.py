#!/usr/bin/env python3
# Generate delta-decoding C++ switch
# This file is part of Castor
#
# Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
# Copyright (C) 2010-2012, Université catholique de Louvain
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, version 3 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import sys


if len(sys.argv) != 2:
    print("Usage:", sys.argv[0], "{full|aggregated|fullyaggregated}")
    sys.exit(1)


def printc(*args):
    print(*args, sep='', end='')

def read(component, length, offset=0, add=False):
    if length == 0 and add and offset == 0:
        return False
    printc(" t[", component, "]", (" += " if add else " = "))
    if length > 0:
        printc("cur.readDelta", length, "()")
        if offset != 0:
            printc("+")
    if offset != 0:
        printc(offset)
    printc(";")
    return True


what = sys.argv[1]

print("switch(header & 127) {")
if what == 'full':
    for h in range(128):
        lengths = [h // 25, (h % 25) // 5, h % 5]
        if any(l > 4 for l in lengths):
            continue
        printc("case ", h, ":")
        add = not read(0, lengths[0], add=True)
        add = not read(1, lengths[1], add=add, offset=(0 if add else 1))
        add = not read(2, lengths[2], add=add, offset=(128 if add else 1))
        print(" break;")
elif what == 'aggregated':
    for h in range(128):
        lengths = [h // 25, (h % 25) // 5, h % 5]
        if any(l > 4 for l in lengths):
            continue
        printc("case ", h, ":")
        add = not read(0, lengths[0], add=True)
        add = not read(1, lengths[1], add=add, offset=1)
        read(2, lengths[2], offset=1)
        print(" break;")
elif what == 'fullyaggregated':
    for h in range(25):
        lengths = [h // 5, h % 5]
        if any(l > 4 for l in lengths):
            continue
        printc("case ", h, ":")
        read(0, lengths[0], add=True, offset=1)
        read(1, lengths[1], offset=1)
        print(" break;")
print("default: assert(false); // should not happen")
print("}")
