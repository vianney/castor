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
#ifndef CASTOR_CP_REVERSIBLE_H
#define CASTOR_CP_REVERSIBLE_H

#include "trail.h"

namespace castor {
namespace cp {

/**
 * Simple reversible object.
 */
template<class T>
class Reversible : public Trailable {
public:
    Reversible(Trail* trail, const T& val) : Trailable(trail), value_(val) {}
    Reversible(Trail& trail, const T& val) : Trailable(trail), value_(val) {}

    // Implementation
    void save   (Trail& trail) const { trail.push(value_); }
    void restore(Trail& trail)       { value_ = trail.pop<T>(); }

    /**
     * Getter
     *
     * @return the current value
     */
    operator T() { return value_; }

    /**
     * Setter
     *
     * @param val the new value
     * @return
     */
    Reversible<T>& operator=(const T& val) {
        modifying();
        value_ = val;
        return *this;
    }

private:
    T value_;  //!< the current value
};

}
}

#endif // CASTOR_CP_REVERSIBLE_H
