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
#ifndef CASTOR_TOOLS_CASTORLD_SORT_H
#define CASTOR_TOOLS_CASTORLD_SORT_H

#include "tempfile.h"

namespace castor {

class FileSorter {
public:

    /**
     * Sort a file
     */
    static void sort(TempFile& in, TempFile& out,
                     void (*skip)(Cursor&),
                     int (*compare)(Cursor, Cursor),
                     bool eliminateDuplicates = false);

};

}

#endif // CASTOR_TOOLS_CASTORLD_SORT_H
