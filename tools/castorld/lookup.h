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
#ifndef CASTOR_TOOLS_CASTORLD_LOOKUP_H
#define CASTOR_TOOLS_CASTORLD_LOOKUP_H

#include <cstring>

#include "tempfile.h"

namespace castor {

/**
 * Lookup cache for early string/value aggregation.
 *
 * Type T must implement the hash() method and the == operator. TempFile must
 * have a write() method for type T.
 */
template<class T>
class Lookup {
public:
    Lookup(TempFile* file_);
    ~Lookup();

    //! Non-copyable
    Lookup(const Lookup&) = delete;
    Lookup& operator=(const Lookup&) = delete;

    /**
     * Lookup an element. Generate an id if necessary and write the mapping
     * to the file.
     *
     * @param e the element to look for.
     * @return the id (!= 0)
     */
    unsigned long lookup(const T& e);

private:
    static constexpr unsigned SIZE = 1009433;  //!< hash table size

    TempFile*      file_;         //!< file for storing the mappings
    T*             elements_;     //!< elements seen so far
    unsigned long* ids_;          //!< ids for the elements
    unsigned long  next_;         //!< next id
};

template<class T>
Lookup<T>::Lookup(TempFile* file) : file_(file) {
    elements_ = new T[SIZE];
    ids_ = new unsigned long[SIZE];
    memset(ids_, 0, SIZE * sizeof(unsigned long));
    next_ = 1;
}

template<class T>
Lookup<T>::~Lookup() {
    delete [] elements_;
    delete [] ids_;
}

template<class T>
unsigned long Lookup<T>::lookup(const T& e) {
    // already in hash table?
    unsigned slot = e.hash() % SIZE;
    if(ids_[slot] != 0 && elements_[slot] == e)
        return ids_[slot];

    // no, construct a new id
    elements_[slot] = e;
    unsigned long id = ids_[slot] = next_++;

    // write mapping to file
    file_->writeBuffer(e.serialize());
    file_->writeVarInt(id);

    return id;
}

}

#endif // CASTOR_TOOLS_CASTORLD_LOOKUP_H
