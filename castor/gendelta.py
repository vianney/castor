#!/usr/bin/env python3
# Generate delta-decoding C++ switch
# This file is part of Castor
#
# Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
# Copyright (C) 2010 - UCLouvain
#
# Castor is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# Castor is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Castor; if not, see <http://www.gnu.org/licenses/>.

def printc(*args):
    print(*args, sep='', end='')

def read(component, length, offset=0, add=False):
    if length == 0 and add and offset == 0:
        return False
    printc(" t.", component, (" += " if add else " = "))
    if length > 0:
        printc("cur.readDelta", length, "()")
        if offset != 0:
            printc("+")
    if offset != 0:
        printc(offset)
    printc(";")
    return True

print("switch(header & 127) {")
for h in range(128):
    lengths = [h // 25, (h % 25) // 5, h % 5]
    if any(l > 4 for l in lengths):
        continue
    printc("case ", h, ":")
    add = not read('a', lengths[0], add=True)
    add = not read('b', lengths[1], add=add, offset=(0 if add else 1))
    add = not read('c', lengths[2], add=add, offset=(128 if add else 1))
    print(" break;")
print("default: assert(false); // should not happen")
print("}")
