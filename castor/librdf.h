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
#ifndef CASTOR_LIBRDF_H
#define CASTOR_LIBRDF_H

#include <raptor.h>
#include <rasqal.h>

namespace castor {
/**
 * librdf wrappers for Castor
 */
namespace librdf {

/**
 * Structure for global librdf worlds
 */
struct World {
    raptor_world *raptor;
    rasqal_world *rasqal;

    /**
     * Return the singleton World instance
     */
    static World& instance();

private:
    World() {
        rasqal = rasqal_new_world();
        raptor = raptor_new_world();
        rasqal_world_set_raptor(rasqal, raptor);
    }

    ~World() {
        rasqal_free_world(rasqal);
        raptor_free_world(raptor);
    }
};

/**
 * Wrapper around raptor_sequence
 */
template <class T>
class Sequence {
    raptor_sequence *seq;
public:
    Sequence(raptor_sequence *seq = NULL) : seq(seq) {}
    Sequence& operator=(raptor_sequence *s) { seq = s; return *this; }
    int size() { return raptor_sequence_size(seq); }
    T* operator[](int i) {
        return reinterpret_cast<T*>(raptor_sequence_get_at(seq, i));
    }
};

}
}

#endif // CASTOR_LIBRDF_H
