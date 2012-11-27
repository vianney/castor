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
#ifndef CASTOR_CP_TRAIL_H
#define CASTOR_CP_TRAIL_H

#include <cstdlib>
#include <cassert>

namespace castor {
namespace cp {

/**
 * A trail is a stack of information used to restore trailable objects.
 */
class Trail {
public:
    /**
     * Type of the timestamps. Defined to be at least 64bits long.
     */
    typedef unsigned long long timestamp_t;

    /**
     * Type of a checkpoint.
     */
    typedef std::ptrdiff_t checkpoint_t;

    /**
     * Construct a trail.
     *
     * @param capacity initial capacity of the trail
     */
    Trail(std::size_t capacity=4096);

    ~Trail();

    /**
     * @return the timestamp of the latest checkpoint/restore.
     */
    timestamp_t timestamp() const { return timestamp_; }

    /**
     * Make a checkpoint of the trail.
     *
     * @return the new checkpoint
     */
    checkpoint_t checkpoint();

    /**
     * Restore a checkpoint.
     *
     * @param chkp a previous checkpoint
     */
    void restore(checkpoint_t chkp);

    /**
     * Push a value on the trail.
     */
    template<class T>
    void push(const T& val) {
        ensureSpace(sizeof(T));
        *((reinterpret_cast<T*&>(ptr_))++) = val;
    }

    /**
     * Pop a value from the trail.
     */
    template<class T>
    T pop() {
        assert(ptr_ - sizeof(T) >= trail_);
        return *(--(reinterpret_cast<T*&>(ptr_)));
    }

private:
    /**
     * Enlarge the allocated space for the stack to be at least of the given
     * capacity.
     *
     * @param capacity
     */
    void enlargeSpace(std::size_t capacity);

    /**
     * Ensure there are at least size bytes free at the top of the stack.
     * @param size
     */
    void ensureSpace(std::size_t size) {
        if(static_cast<std::size_t>(end_ - ptr_) < size)
            enlargeSpace(ptr_ - trail_ + size);
    }

private:
    /**
     * The trail stack.
     */
    char* trail_;

    /**
     * Pointer to the byte just after the allocated trail stack.
     */
    char* end_;

    /**
     * Pointer to the top of the trail stack, i.e., the first non-written
     * byte.
     */
    char* ptr_;

    /**
     * Current timestamp.
     */
    timestamp_t timestamp_;
};


/**
 * Base class for a trailable object.
 */
class Trailable {
public:
    /**
     * Save the current state to the trail.
     *
     * @param trail the trail
     */
    virtual void save(Trail& trail) const = 0;

    /**
     * Restore the state from the trail.
     *
     * @param trail the trail
     */
    virtual void restore(Trail& trail) = 0;

protected:
    /**
     * Construct a trailable object.
     *
     * @param trail attached trail
     */
    Trailable(Trail* trail) : trail_(trail), timestamp_(0) {
        assert(trail != nullptr);
    }
    Trailable(Trail& trail) : Trailable(&trail) {}

    //! Non-copyable
    Trailable(const Trailable&) = delete;
    Trailable& operator=(const Trailable&) = delete;

    virtual ~Trailable() {}

    /**
     * Save the variable to the trail if necessary.
     *
     * @note This method shall be called by implementations before any
     *       modification of the state.
     */
    void modifying();

private:
    /**
     * The trail where this object writes its restore information.
     */
    Trail* trail_;

    /**
     * Timestamp of the latest trail checkpoint.
     */
    Trail::timestamp_t timestamp_;
};

}
}

#endif // CASTOR_CP_TRAIL_H
