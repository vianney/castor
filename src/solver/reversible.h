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

/**
 * Reversible set represented as a sparse set without map.
 *
 * @param T the type of the values
 */
template<class T>
class ReversibleSet : public Trailable {
public:
    /**
     * Construct a reversible set with values min..max.
     */
    ReversibleSet(Trail* trail, T min, T max);
    ReversibleSet(Trail& trail, T min, T max) :
        ReversibleSet(&trail, min, max) {}

    /**
     * Destructor.
     */
    ~ReversibleSet();

    // Implementation
    void save   (Trail& trail) const { trail.push(size_); }
    void restore(Trail& trail)       { size_ = trail.pop<unsigned>(); }

    /**
     * Get the values array. Beware that this is a pointer directly to the
     * internal structure, so you should not mess with it. Removing a value
     * only affects values after and including itself in the array.
     *
     * @return pointer to the values array
     */
    const T* values() const { return values_; }

    /**
     * @return the size of the set
     */
    unsigned size() const { return size_; }

    /**
     * @param index
     * @return the value at index index
     */
    T operator[](unsigned index) const { return values_[index]; }

    /**
     * Clear all values from the set.
     */
    void clear() { size_ = 0; }

    /**
     * Remove a value from the set.
     * The value at index index may be replaced by another value in the set and
     * should thus be rechecked if iterating over the set.
     *
     * @param index index of the value to remove
     */
    void remove(unsigned index);

private:
    /**
     * Size of the current set.
     */
    unsigned size_;

    /**
     * The values.
     *
     * @invariant values_[0..size_-1] = the current set
     */
    T* values_;
};

////////////////////////////////////////////////////////////////////////////////
// Template implementation

template<class T>
ReversibleSet<T>::ReversibleSet(Trail* trail, T min, T max) : Trailable(trail) {
    size_ = max - min + 1;
    values_ = reinterpret_cast<T*>(malloc(size_ * sizeof(T)));
    for(unsigned i = 0; i < size_; i++)
        values_[i] = min + i;
}

template<class T>
ReversibleSet<T>::~ReversibleSet() {
    free(values_);
}

template<class T>
void ReversibleSet<T>::remove(unsigned index) {
    assert(index >= 0 && index < size_);
    modifying();
    size_--;
    T v = values_[size_];
    values_[size_] = values_[index];
    values_[index] = v;
}

}
}

#endif // CASTOR_CP_REVERSIBLE_H
