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
#include <string.h>
#include <sqlite3.h>

#include "store_sqlite.h"

/**
 * Internal store structure.
 */
typedef struct {
    /**
     * Inherited public interface implementation
     */
    Store pub;
    /**
     * Handle to the database
     */
    sqlite3* db;
    /**
     * Number of values in the store
     */
    int nbValues;
    /**
     * Value cache
     */
    Value* values;
    /**
     * Number of datatypes
     */
    int nbDatatypes;
    /**
     * Datatypes cache
     */
    char** datatypes;
    /**
     * Number of languages
     */
    int nbLanguages;
    /**
     * Languages cache
     */
    char** languages;
} SqliteStore;

////////////////////////////////////////////////////////////////////////////////
// Implementations

void sqlite_store_close(SqliteStore* self) {
    sqlite3_close(self->db);
    if(self->datatypes != NULL) {
        for(i = VALUE_TYPE_FIRST_CUSTOM; i < self->nbDatatypes; i++)
            free(self->datatypes[i]);
        free(self->datatypes);
    }
    if(self->languages != NULL) {
        for(i = 0; i < self->nbLanguages; i++)
            free(self->languages[i]);
        free(self->languages);
    }
    if(self->values != NULL) {
        for(i = 0; i < self->nbValues; i++)
            free(self->values[i].lexical);
        free(self->values);
    }
    free(self);
}

int sqlite_store_value_count(SqliteStore* self) {
    return self->nbValues;
}

Value* sqlite_store_value_get(SqliteStore* self, int id) {
    if(id < 0 || id >= self->nbValues)
        return NULL;
    return &self->values[id];
}

Value* sqlite_store_value_create(SqliteStore* self, ValueType type,
                                 char* typeUri, char* lexical, char* language) {
    sqlite3_stmt *sql;
    int lang;
    Value* v;

    if(type == VALUE_TYPE_UNKOWN) {
        if(sqlite3_prepare_v2(self->db,
                              "SELECT id FROM datatypes WHERE uri = ?",
                              -1, &sql, NULL) != SQLITE_OK)
            return NULL;
        if(sqlite3_bind_text(sql, 1, typeUri, -1, SQLITE_STATIC) != SQLITE_OK)
            goto cleansql;
        switch(sqlite3_step(sql)) {
        case SQLITE_ROW:
            type = sqlite3_column_int(sql, 0);
            typeUri = self->datatypes[type];
            break;
        case SQLITE_DONE:
            break;
        default:
            goto cleansql;
        }
        sqlite3_finalize(sql);
    }

    lang = -1;
    if(language != NULL) {
        if(sqlite3_prepare_v2(self->db,
                              "SELECT id FROM languages WHERE tag = ?",
                              -1, &sql, NULL) != SQLITE_OK)
            return NULL;
        if(sqlite3_bind_text(sql, 1, language, -1, SQLITE_STATIC) != SQLITE_OK)
            goto cleansql;
        switch(sqlite3_step(sql)) {
        case SQLITE_ROW:
            lang = sqlite3_column_int(sql, 0);
            language = self->languages[lang];
            break;
        case SQLITE_DONE:
            break;
        default:
            goto cleansql;
        }
        sqlite3_finalize(sql);
    }

    v = NULL;

    if(type != VALUE_TYPE_UNKOWN && lang != -1) {
        if(sqlite3_prepare_v2(self->db,
                              "SELECT id FROM values"
                              "WHERE type = ? AND lexical = ? AND language = ?",
                              -1, &sql, NULL) != SQLITE_OK)
            return NULL;
        if(sqlite3_bind_int(sql, 1, type) != SQLITE_OK)
            goto cleansql;
        if(sqlite3_bind_text(sql, 2, lexical, -1, SQLITE_STATIC) != SQLITE_OK)
            goto cleansql;
        if(sqlite3_bind_int(sql, 3, lang) != SQLITE_OK)
            goto cleansql;
        switch(sqlite3_step(sql)) {
        case SQLITE_ROW:
            v = &self->values[sqlite3_column_int(sql, 0)];
            break;
        case SQLITE_DONE:
            break;
        default:
            goto cleansql;
        }
        sqlite3_finalize(sql);
    }

    if(v == NULL) {
        v = (Value*) malloc(sizeof(Value));
        v->id = -1;
        v->type = type;
        v->typeUri = typeUri;
        v->lexical = lexical;
        v->language = lang;
        v->languageTag = language;
        if(type >= VALUE_TYPE_FIRST_INTEGER && type <= VALUE_TYPE_LAST_INTEGER)
            v->integer = atoi(lexical);
        else if(type >= VALUE_TYPE_FIRST_FLOATING &&
                type <= VALUE_TYPE_LAST_FLOATING)
            v->floating = atof(lexical);
        //else if   TODO: datetime
    }

    return v;

cleansql:
    sqlite3_finalize(sql);
    return NULL;
}

bool sqlite_store_statement_add(SqliteStore* self, Value* source,
                                Value* predicate, Value* object) {
    // TODO
}

bool sqlite_store_statement_query(SqliteStore* self, int source, int predicate,
                                  int object) {
    // TODO
}

bool sqlite_store_statement_fetch(SqliteStore* self, Statement* stmt) {
    // TODO
}

bool sqlite_store_statement_finalize(SqliteStore* self) {
    // TODO
}

////////////////////////////////////////////////////////////////////////////////
// Constructors

/**
 * Create a new Store structure and fill in the public interface
 *
 * @return the new store structure
 */
SqliteStore* sqlite_store_new() {
    SqliteStore* self;

    self = (SqliteStore*) malloc(sizeof(SqliteStore));

    self->pub.close = (void (*)(Store*)) sqlite_store_close;
    self->pub.value_count = (int (*)(Store*)) sqlite_store_value_count;
    self->pub.value_get = (Value* (*)(Store*, int)) sqlite_store_value_get;
    self->pub.value_create = (Value* (*)(Store*, ValueType, char*, char*, char*))
                             sqlite_store_value_create;
    self->pub.statement_add = (bool (*)(Store*, Value*, Value*, Value*))
                              sqlite_store_statement_add;
    self->pub.statement_query = (bool (*)(Store*, int, int, int))
                                sqlite_store_statement_query;
    self->pub.statement_fetch = (bool (*)(Store*, Statement*))
                                sqlite_store_statement_fetch;
    self->pub.statement_finalize = (bool (*)(Store*))
                                   sqlite_store_statement_finalize;

    self->nbValues = 0;
    self->values = NULL;
    self->nbDatatypes = 0;
    self->datatypes = NULL;
    self->nbLanguages = 0;
    self->languages = NULL;

    return self;
}

/**
 * Common initialization of the store structure after the database connection
 * has been initialized.
 *
 * @param self the store instance
 * @return true if all went well, false if error
 */
bool sqlite_store_common_init(SqliteStore *self) {
    // TODO
    return true;
}

Store* sqlite_store_create(const char* filename) {
    SqliteStore* self;
    ValueType t;
    sqlite3_stmt *sql;

    self = sqlite_store_new();
    if(sqlite3_open_v2(filename, &self->db,
                       SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK)
        goto cleanself;

    if(sqlite3_exec(self->db,
                    "CREATE TABLE datatypes ("
                    "  id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
                    "  uri TEXT UNIQUE,"
                    ");"
                    "CREATE TABLE languages ("
                    "  id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
                    "  tag TEXT UNIQUE"
                    ");"
                    "CREATE TABLE values ("
                    "  id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
                    "  type INTEGER NOT NULL REFERENCES datatypes(id),"
                    "  lexical TEXT,"
                    "  language INTEGER REFERENCES languages(id),"
                    "  value,"
                    "  UNIQUE (type, lexical, language)"
                    ");"
                    "CREATE TABLE statements ("
                    "  subject INTEGER NOT NULL REFERENCES values(id),"
                    "  predicate INTEGER NOT NULL REFERENCES values(id),"
                    "  object INTEGER NOT NULL REFERENCES values(id),"
                    "  PRIMARY KEY (predicate, subject, object)"
                    ");"
                    "INSERT INTO languages (id, tag) VALUES (0, '');",
                    NULL, NULL, NULL) != SQLITE_OK)
        goto cleandb;

    if(sqlite3_prepare_v2(self->db,
                          "INSERT INTO datatypes (id, uri) VALUES (?, ?)",
                          -1, &sql, NULL) != SQLITE_OK)
        goto cleandb;
    for(t = VALUE_TYPE_BLANK; t < VALUE_TYPE_FIRST_CUSTOM; t++) {
        if(sqlite3_reset(sql) != SQLITE_OK)
            goto cleansql;
        if(sqlite3_bind_int(sql, 1, t) != SQLITE_OK)
            goto cleansql;
        if(VALUETYPE_URIS[t] == NULL)
            if(sqlite3_bind_null(sql, 2) != SQLITE_OK)
                goto cleansql;
        else
            if(sqlite3_bind_text(sql, 2, VALUETYPE_URIS[t], -1, SQLITE_STATIC) != SQLITE_OK)
                goto cleansql;
        if(sqlite3_step(sql) != SQLITE_DONE)
            goto cleansql;
    }
    sqlite3_finalize(sql);

    return self;

cleansql:
    sqlite3_finalize(sql);
cleandb:
    sqlite3_close(self->db);
cleanself:
    free(self);
    return NULL;
}

Store* sqlite_store_open(const char* filename) {
    SqliteStore *self;
    sqlite3_stmt *sql;
    int i, rc;
    char *str;
    Value *v;

    self = sqlite_store_new();
    if(sqlite3_open_v2(filename, &self->db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK)
        goto cleanself;

    // Populate datatypes
    if(sqlite3_prepare_v2(self->db,
                          "SELECT COUNT(*) FROM datatypes",
                          -1, &sql, NULL) != SQLITE_OK)
        goto cleandb;
    if(sqlite3_step(sql) != SQLITE_ROW)
        goto cleansql;
    self->nbDatatypes = sqlite3_column_int(sql, 0);
    sqlite3_finalize(sql);

    self->datatypes = (char**) calloc(self->nbDatatypes, sizeof(char*));
    for(i = 0; i < VALUE_TYPE_FIRST_CUSTOM; i++)
        self->datatypes[i] = VALUETYPE_URIS[i];

    if(sqlite3_prepare_v2(self->db,
                          "SELECT id, uri FROM datatypes "
                          "WHERE id >= " #VALUE_TYPE_FIRST_CUSTOM,
                          -1, &sql, NULL) != SQLITE_OK)
        goto cleandb;
    while((rc = sqlite3_step(sql)) != SQLITE_DONE) {
        if(rc != SQLITE_ROW)
            goto cleansql;
        i = sqlite3_column_int(sql, 0);
        str = sqlite3_column_text(sql, 1);
        self->datatypes[i] = (char*) malloc((strlen(str)+1) * sizeof(char));
        strcpy(self->datatypes[i], str);
    }
    sqlite3_finalize(sql);

    // Populate languages
    if(sqlite3_prepare_v2(self->db,
                          "SELECT COUNT(*) FROM languages",
                          -1, &sql, NULL) != SQLITE_OK)
        goto cleandb;
    if(sqlite3_step(sql) != SQLITE_ROW)
        goto cleansql;
    self->nbLanguages = sqlite3_column_int(sql, 0);
    sqlite3_finalize(sql);

    self->languages = (char**) calloc(self->nbLanguages, sizeof(char*));

    if(sqlite3_prepare_v2(self->db,
                          "SELECT id, tag FROM languages",
                          -1, &sql, NULL) != SQLITE_OK)
        goto cleandb;
    while((rc = sqlite3_step(sql)) != SQLITE_DONE) {
        if(rc != SQLITE_ROW)
            goto cleansql;
        i = sqlite3_column_int(sql, 0);
        str = sqlite3_column_text(sql, 1);
        self->languages[i] = (char*) malloc((strlen(str)+1) * sizeof(char));
        strcpy(self->languages[i], str);
    }
    sqlite3_finalize(sql);

    // Populate values
    if(sqlite3_prepare_v2(self->db,
                          "SELECT COUNT(*) FROM values",
                          -1, &sql, NULL) != SQLITE_OK)
        goto cleandb;
    if(sqlite3_step(sql) != SQLITE_ROW)
        goto cleansql;
    self->nbValues = sqlite3_column_int(sql, 0);
    sqlite3_finalize(sql);

    self->values = (Value*) calloc(self->nbValues, sizeof(Value));

    if(sqlite3_prepare_v2(self->db,
                          "SELECT id, type, lexical, language, value FROM values",
                          -1, &sql, NULL) != SQLITE_OK)
        goto cleandb;
    while((rc = sqlite3_step(sql)) != SQLITE_DONE) {
        if(rc != SQLITE_ROW)
            goto cleansql;
        i = sqlite3_column_int(sql, 0);
        v = &self->values[i];
        v->id = i;
        v->type = sqlite3_column_int(sql, 1);
        v->typeUri = self->datatypes[v->type];
        str = sqlite3_column_text(sql, 2);
        v->lexical = (char*) malloc((strlen(str)+1) * sizeof(char));
        strcpy(v->lexical, str);
        v->language = sqlite3_column_int(sql, 3);
        v->languageTag = self->languages[v->language];
        if(v->type >= VALUE_TYPE_FIRST_INTEGER &&
           v->type <= VALUE_TYPE_LAST_INTEGER)
            v->integer = sqlite3_column_int(sql, 4);
        else if(v->type >= VALUE_TYPE_FIRST_FLOATING &&
                v->type <= VALUE_TYPE_LAST_FLOATING)
            v->floating = sqlite3_column_double(sql, 4);
        //else if(v->type == VALUE_TYPE_DATETIME)
        // TODO: v->datetime
    }
    sqlite3_finalize(sql);

    return self;

cleansql:
    sqlite3_finalize(sql);
cleandb:
    sqlite3_close(self->db);
cleanself:
    if(self->datatypes != NULL) {
        for(i = VALUE_TYPE_FIRST_CUSTOM; i < self->nbDatatypes; i++)
            if(self->datatypes[i] != NULL)
                free(self->datatypes[i]);
        free(self->datatypes);
    }
    if(self->languages != NULL) {
        for(i = 0; i < self->nbLanguages; i++)
            if(self->languages[i] != NULL)
                free(self->languages[i]);
        free(self->languages);
    }
    if(self->values != NULL) {
        for(i = 0; i < self->nbValues; i++)
            if(self->values[i].lexical != NULL)
                free(self->values[i].lexical);
        free(self->values);
    }
    free(self);
    return NULL;
}
