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
#ifndef CASTOR_LIBRDF_H
#define CASTOR_LIBRDF_H

#include <cassert>
#include <raptor2.h>
#include <rasqal.h>

#include "util.h"

namespace castor {
/**
 * librdf wrappers for Castor
 */
namespace librdf {

/**
 * Structure for global librdf worlds
 */
struct World {
    raptor_world* raptor;
    rasqal_world* rasqal;

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
public:
    Sequence(raptor_sequence* seq = nullptr) : seq_(seq) {}
    Sequence(const Sequence&) = default;
    Sequence& operator=(const Sequence&) = default;
    Sequence& operator=(raptor_sequence* s) { seq_ = s; return *this; }
    int size() { return seq_ ? raptor_sequence_size(seq_) : 0; }
    T* operator[](int i) {
        return reinterpret_cast<T*>(raptor_sequence_get_at(seq_, i));
    }
    bool operator==(const Sequence& o) const { return seq_ == o.seq_; }
    bool operator!=(const Sequence& o) const { return seq_ != o.seq_; }

    // C++11 iterator
    class Iterator {
    public:
        Iterator(Sequence<T> seq, int i) : seq_(seq), i_(i) {}
        T* operator*() { return seq_[i_]; }
        bool operator!=(const Iterator& o) const {
            assert(seq_ == o.seq_);
            return i_ != o.i_;
        }
        Iterator& operator++() { ++i_; return *this; }
    private:
        Sequence<T> seq_;
        int i_;
    };
    Iterator begin() { return Iterator(*this, 0);      }
    Iterator end()   { return Iterator(*this, size()); }

private:
    raptor_sequence* seq_;
};


/**
 * Intarface for an RDF Parser handler
 */
class RdfParseHandler {
public:
    virtual ~RdfParseHandler() {}
    virtual void parseTriple(raptor_statement* triple) = 0;
};

/**
 * Wrapper for RDF parser
 */
class RdfParser {
public:
    RdfParser(const char* syntax, const char* path) {
        parser_ = raptor_new_parser(World::instance().raptor, syntax);
        if(parser_ == nullptr)
            throw CastorException() << "Unable to create raptor parser";
        uriString_ = raptor_uri_filename_to_uri_string(path);
        uri_ = raptor_new_uri(World::instance().raptor, uriString_);
    }
    ~RdfParser() {
        raptor_free_parser(parser_);
        raptor_free_uri   (uri_);
        raptor_free_memory(uriString_);
    }

    //! Non-copyable
    RdfParser(const RdfParser&) = delete;
    RdfParser& operator=(const RdfParser&) = delete;

    /**
     * Parse the RDF calling handler->parseTriple() on every triple.
     *
     * @param handler handler to call
     */
    void parse(RdfParseHandler* handler) {
        raptor_parser_set_statement_handler(parser_, handler, stmt_handler);
        raptor_parser_parse_file(parser_, uri_, nullptr);
    }

private:
    static void stmt_handler(void* user_data, raptor_statement* triple) {
        static_cast<RdfParseHandler*>(user_data)->parseTriple(triple);
    }

private:
    raptor_parser* parser_;
    raptor_uri*    uri_;
    unsigned char* uriString_;
};


}
}

#endif // CASTOR_LIBRDF_H
