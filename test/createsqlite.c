/* This file is part of Castor
 *
 * Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>
 * Copyright (C) 2010 - UCLouvain
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <sqlite3.h>
#include <raptor.h>

#include "../model.h"

#define NODE_SQL "INSERT INTO vals (type, lexical) VALUES (?1, ?2); " \
                 "SELECT id FROM vals WHERE type = ?1 AND lexical = ?2;"
#define LITERAL_SQL ""
#define STATEMENT_SQL "INSERT INTO statements (subject, predicate, object) " \
                      "  VALUES (?1, ?2, ?3);"

typedef struct {
    sqlite3 *db;
    sqlite3_stmt *sqlNode;
    sqlite3_stmt *sqlLiteral;
    sqlite3_stmt *sqlStatement;

    raptor_parser *parser;
    unsigned char *fileURIstr;
    raptor_uri *fileURI;
} Data;

void cleanup(Data *d) {
    if(d->sqlNode != NULL)
        sqlite3_finalize(d->sqlNode);
    if(d->sqlLiteral != NULL)
        sqlite3_finalize(d->sqlLiteral);
    if(d->sqlStatement != NULL)
        sqlite3_finalize(d->sqlStatement);
    if(d->db != NULL)
        sqlite3_close(d->db);

    if(d->parser != NULL)
        raptor_free_parser(d->parser);
    if(d->fileURI != NULL)
        raptor_free_uri(d->fileURI);
    if(d->fileURIstr != NULL)
        raptor_free_memory(d->fileURIstr);
    raptor_finish();
}

void sqlerror(Data* d) {
    fprintf(stderr, "SQLite error: %s\n", sqlite3_errmsg(d->db));
    cleanup(d);
    exit(2);
}

int get_node_id(Data* d, char* uri, bool blank) {
    sqlite3_stmt *sql;
    ValueType type;
    int id;

    type = blank ? VALUE_TYPE_BLANK : VALUE_TYPE_IRI;

    if(sqlite3_prepare_v2(d->db,
                          "SELECT id FROM vals "
                          "WHERE type = ? AND lexical = ?",
                          -1, &sql, NULL) != SQLITE_OK)
        goto error;
    if(sqlite3_bind_int(sql, 1, type) != SQLITE_OK)
        goto cleansql;
    if(sqlite3_bind_text(sql, 2, uri, -1, SQLITE_STATIC) != SQLITE_OK)
        goto cleansql;
    switch(sqlite3_step(sql)) {
    case SQLITE_ROW:
        id = sqlite3_column_int(sql, 0);
        break;
    case SQLITE_DONE:
        id = -1;
        break;
    default:
        goto cleansql;
    }
    sqlite3_finalize(sql);

    if(id == -1) {
        if(sqlite3_prepare_v2(d->db,
                              "INSERT INTO vals (type, lexical) VALUES (?, ?)",
                              -1, &sql, NULL) != SQLITE_OK)
            goto error;
        if(sqlite3_bind_int(sql, 1, type) != SQLITE_OK)
            goto cleansql;
        if(sqlite3_bind_text(sql, 2, uri, -1, SQLITE_STATIC) != SQLITE_OK)
            goto cleansql;
        if(sqlite3_step(sql) != SQLITE_DONE)
            goto cleansql;
        sqlite3_finalize(sql);
        id = sqlite3_last_insert_rowid(d->db);
    }

    return id;

cleansql:
    sqlite3_finalize(sql);
error:
    sqlerror(d);
    return 2;
}

int get_literal_id(Data* d, char* lexical, char* typeUri, char* language) {
    sqlite3_stmt *sql;
    ValueType type;
    int lang;
    bool doCreate;
    int id;

    doCreate = false;

    // type
    if(typeUri == NULL) {
        type = VALUE_TYPE_PLAIN_STRING;
    } else {
        if(sqlite3_prepare_v2(d->db,
                              "SELECT id FROM datatypes WHERE uri = ?",
                              -1, &sql, NULL) != SQLITE_OK)
            goto error;
        if(sqlite3_bind_text(sql, 1, typeUri, -1, SQLITE_STATIC) != SQLITE_OK)
            goto cleansql;
        switch(sqlite3_step(sql)) {
        case SQLITE_ROW:
            type = sqlite3_column_int(sql, 0);
            break;
        case SQLITE_DONE:
            type = VALUE_TYPE_UNKOWN;
            break;
        default:
            goto cleansql;
        }
        sqlite3_finalize(sql);
        if(type == VALUE_TYPE_UNKOWN) {
            if(sqlite3_prepare_v2(d->db,
                                  "INSERT INTO datatypes (uri) VALUES (?)",
                                  -1, &sql, NULL) != SQLITE_OK)
                goto error;
            if(sqlite3_bind_text(sql, 1, typeUri, -1, SQLITE_STATIC) != SQLITE_OK)
                goto cleansql;
            if(sqlite3_step(sql) != SQLITE_DONE)
                goto cleansql;
            sqlite3_finalize(sql);
            type = sqlite3_last_insert_rowid(d->db);
            doCreate = true;
        }
    }

    // language
    if(sqlite3_prepare_v2(d->db,
                          "SELECT id FROM languages WHERE tag = ?",
                          -1, &sql, NULL) != SQLITE_OK)
        goto error;
    if(sqlite3_bind_text(sql, 1, language, -1, SQLITE_STATIC) != SQLITE_OK)
        goto cleansql;
    switch(sqlite3_step(sql)) {
    case SQLITE_ROW:
        lang = sqlite3_column_int(sql, 0);
        break;
    case SQLITE_DONE:
        lang = -1;
        break;
    default:
        goto cleansql;
    }
    sqlite3_finalize(sql);
    if(lang == -1) {
        if(sqlite3_prepare_v2(d->db,
                              "INSERT INTO languages (tag) VALUES (?)",
                              -1, &sql, NULL) != SQLITE_OK)
            goto error;
        if(sqlite3_bind_text(sql, 1, language, -1, SQLITE_STATIC) != SQLITE_OK)
            goto cleansql;
        if(sqlite3_step(sql) != SQLITE_DONE)
            goto cleansql;
        sqlite3_finalize(sql);
        lang = sqlite3_last_insert_rowid(d->db);
        doCreate = true;
    }

    // get value
    if(!doCreate) {
        if(sqlite3_prepare_v2(d->db,
                              "SELECT id FROM vals "
                              "WHERE type = ? AND lexical = ? AND language = ?",
                              -1, &sql, NULL) != SQLITE_OK)
            goto error;
        if(sqlite3_bind_int(sql, 1, type) != SQLITE_OK)
            goto cleansql;
        if(sqlite3_bind_text(sql, 2, lexical, -1, SQLITE_STATIC) != SQLITE_OK)
            goto cleansql;
        if(sqlite3_bind_int(sql, 3, lang) != SQLITE_OK)
            goto cleansql;
        switch(sqlite3_step(sql)) {
        case SQLITE_ROW:
            id = sqlite3_column_int(sql, 0);
            break;
        case SQLITE_DONE:
            doCreate = true;
            break;
        default:
            goto cleansql;
        }
        sqlite3_finalize(sql);
    }

    // create value if needed
    if(doCreate) {
        if(sqlite3_prepare_v2(d->db,
                              "INSERT INTO vals"
                              "  (type, lexical, language, value)"
                              "  VALUES (?, ?, ?, ?)",
                              -1, &sql, NULL) != SQLITE_OK)
            goto error;
        if(sqlite3_bind_int(sql, 1, type) != SQLITE_OK)
            goto cleansql;
        if(sqlite3_bind_text(sql, 2, lexical, -1, SQLITE_STATIC) != SQLITE_OK)
            goto cleansql;
        if(sqlite3_bind_int(sql, 3, lang) != SQLITE_OK)
            goto cleansql;
        if(type >= VALUE_TYPE_FIRST_INTEGER && type <= VALUE_TYPE_LAST_INTEGER) {
            if(sqlite3_bind_int(sql, 4, atoi(lexical)) != SQLITE_OK)
                goto cleansql;
        } else if(type >= VALUE_TYPE_FIRST_FLOATING && type <= VALUE_TYPE_LAST_FLOATING) {
            if(sqlite3_bind_double(sql, 4, atof(lexical)) != SQLITE_OK)
                goto cleansql;
        } else {
            if(sqlite3_bind_null(sql, 4) != SQLITE_OK)
                goto cleansql;
        }
        if(sqlite3_step(sql) != SQLITE_DONE)
            goto cleansql;
        sqlite3_finalize(sql);
        id = sqlite3_last_insert_rowid(d->db);
    }

    return id;

cleansql:
    sqlite3_finalize(sql);
error:
    sqlerror(d);
    return 2;
}

void add_triple(void* user_data, const raptor_statement* triple) {
    Data *d;
    int s, p, o;
    unsigned char *typeUri;
    sqlite3_stmt *sql;

    d = (Data*) user_data;

    s = get_node_id(d, (char*) triple->subject,
                    triple->subject_type == RAPTOR_IDENTIFIER_TYPE_ANONYMOUS);
    p = get_node_id(d, (char*) triple->predicate,
                    triple->predicate_type == RAPTOR_IDENTIFIER_TYPE_ANONYMOUS);
    switch(triple->object_type) {
    case RAPTOR_IDENTIFIER_TYPE_ANONYMOUS:
        o = get_node_id(d, (char*) triple->object, true);
        break;
    case RAPTOR_IDENTIFIER_TYPE_RESOURCE:
        o = get_node_id(d, (char*) triple->object, false);
        break;
    case RAPTOR_IDENTIFIER_TYPE_LITERAL:
        if(triple->object_literal_datatype == NULL)
            typeUri = NULL;
        else
            typeUri = raptor_uri_as_string(triple->object_literal_datatype);
        o = get_literal_id(d, (char*) triple->object, (char*) typeUri,
                           (char*) triple->object_literal_language);
        break;
    default:
        fprintf(stderr, "Unknown object type %d\n", triple->object_type);
        goto error;
    }

    if(sqlite3_prepare_v2(d->db,
                          "INSERT INTO statements (subject, predicate, object)"
                          "  VALUES (?, ?, ?)",
                          -1, &sql, NULL) != SQLITE_OK)
        goto error;
    if(sqlite3_bind_int(sql, 1, s) != SQLITE_OK)
        goto cleansql;
    if(sqlite3_bind_int(sql, 2, p) != SQLITE_OK)
        goto cleansql;
    if(sqlite3_bind_int(sql, 3, o) != SQLITE_OK)
        goto cleansql;
    if(sqlite3_step(sql) != SQLITE_DONE)
        goto cleansql;
    sqlite3_finalize(sql);

    return;

cleansql:
    sqlite3_finalize(sql);
error:
    sqlerror(d);
}

int main(int argc, char* argv[]) {
    char *rdfpath, *dbpath;
    Data d;
    sqlite3_stmt *sql;
    ValueType t;

    if(argc != 3) {
        printf("Usage: %s RDF DB\n", argv[0]);
        return 1;
    }
    rdfpath = argv[1];
    dbpath = argv[2];

    if(access(argv[2], F_OK) == 0) {
        fprintf(stderr, "Database already exists. Exiting.\n");
        return 1;
    }

    memset(&d, 0, sizeof(Data));

    if(sqlite3_open_v2(dbpath, &d.db,
                       SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK) {
        fprintf(stderr, "Unable to create database.\n");
        return 2;
    }

    raptor_init();

    if(sqlite3_exec(d.db,
                    "CREATE TABLE datatypes ("
                    "  id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
                    "  uri TEXT UNIQUE ON CONFLICT IGNORE"
                    ");"
                    "CREATE TABLE languages ("
                    "  id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
                    "  tag TEXT UNIQUE ON CONFLICT IGNORE"
                    ");"
                    "CREATE TABLE vals ("
                    "  id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
                    "  type INTEGER NOT NULL REFERENCES datatypes(id),"
                    "  lexical TEXT,"
                    "  language INTEGER REFERENCES languages(id) DEFAULT 0,"
                    "  value,"
                    "  UNIQUE (type, lexical, language) ON CONFLICT IGNORE"
                    ");"
                    "CREATE TABLE statements ("
                    "  subject INTEGER NOT NULL REFERENCES vals(id),"
                    "  predicate INTEGER NOT NULL REFERENCES vals(id),"
                    "  object INTEGER NOT NULL REFERENCES vals(id),"
                    "  PRIMARY KEY (predicate, subject, object) ON CONFLICT IGNORE"
                    ");"
                    "INSERT INTO languages (id, tag) VALUES (0, '');",
                    NULL, NULL, NULL) != SQLITE_OK)
        goto cleandb;

    if(sqlite3_prepare_v2(d.db,
                          "INSERT INTO datatypes (id, uri) VALUES (?, ?)",
                          -1, &sql, NULL) != SQLITE_OK)
        goto cleandb;
    for(t = VALUE_TYPE_BLANK; t < VALUE_TYPE_FIRST_CUSTOM; t++) {
        if(sqlite3_reset(sql) != SQLITE_OK)
            goto cleansql;
        if(sqlite3_bind_int(sql, 1, t) != SQLITE_OK)
            goto cleansql;
        if(VALUETYPE_URIS[t] == NULL) {
            if(sqlite3_bind_null(sql, 2) != SQLITE_OK)
                goto cleansql;
        } else {
            if(sqlite3_bind_text(sql, 2, VALUETYPE_URIS[t], -1, SQLITE_STATIC) != SQLITE_OK)
                goto cleansql;
        }
        if(sqlite3_step(sql) != SQLITE_DONE)
            goto cleansql;
    }
    sqlite3_finalize(sql);

//    d.parser = raptor_new_parser("turtle");
//    raptor_set_statement_handler(d.parser, &d, add_triple);
//    d.fileURIstr = raptor_uri_filename_to_uri_string(rdfpath);
//    d.fileURI = raptor_new_uri(d.fileURIstr);
//    raptor_parse_file(d.parser, d.fileURI, NULL);

    cleanup(&d);
    return 0;

cleansql:
    sqlite3_finalize(sql);
cleandb:
    sqlerror(&d);
    return 2;
}
