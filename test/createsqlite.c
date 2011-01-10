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

typedef struct {
    sqlite3 *db;
    sqlite3_stmt *sqlInsertDatatype;
    sqlite3_stmt *sqlInsertLanguage;
    sqlite3_stmt *sqlInsertVal;
    sqlite3_stmt *sqlInsertValUnkLang;
    sqlite3_stmt *sqlInsertValUnkTypeLang;
    sqlite3_stmt *sqlInsertStmt;
    sqlite3_stmt *sqlInsertStmtUnkLang;
    sqlite3_stmt *sqlInsertStmtUnkTypeLang;

    raptor_parser *parser;
    unsigned char *fileURIstr;
    raptor_uri *fileURI;

    int count;
} Data;

void cleanup(Data *d) {
#define CLEAN(sql) if(d->sql != NULL) sqlite3_finalize(d->sql);
    CLEAN(sqlInsertDatatype);
    CLEAN(sqlInsertLanguage);
    CLEAN(sqlInsertVal);
    CLEAN(sqlInsertValUnkLang);
    CLEAN(sqlInsertValUnkTypeLang);
    CLEAN(sqlInsertStmt);
    CLEAN(sqlInsertStmtUnkLang);
    CLEAN(sqlInsertStmtUnkTypeLang);
#undef CLEAN
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

#define XSD_PREFIX "http://www.w3.org/2001/XMLSchema#"
#define XSD_PREFIX_LEN (sizeof(XSD_PREFIX) - 1)

ValueType get_type(char *uri) {
    ValueType t;
    char *fragment;

    if(uri == NULL || uri[0] == '\0')
        return VALUE_TYPE_PLAIN_STRING;

    if(strncmp(uri, XSD_PREFIX, XSD_PREFIX_LEN) != 0)
        return VALUE_TYPE_UNKOWN;

    fragment = &uri[XSD_PREFIX_LEN];
    for(t = VALUE_TYPE_FIRST_XSD; t <= VALUE_TYPE_LAST_XSD; t++) {
        if(strcmp(fragment, &VALUETYPE_URIS[t][XSD_PREFIX_LEN]) == 0)
            return t;
    }
    return VALUE_TYPE_UNKOWN;
}

void add_triple(void* user_data, const raptor_statement* triple) {
    Data *d;
    sqlite3_stmt *sql;
    ValueType ts, tp, to;
    char *typeUri;
    int lang;

    d = (Data*) user_data;
    d->count++;
    if(d->count % 10000 == 0) {
        putchar('.');
        fflush(stdout);
    }

#define START_SQL(stmt) \
    sql = stmt; \
    if(sqlite3_reset(sql) != SQLITE_OK) goto error;
#define STOP_SQL \
    if(sqlite3_step(sql) != SQLITE_DONE) goto error;
#define BIND_STR(col, s) \
    if(sqlite3_bind_text(sql, (col), (s), -1, SQLITE_STATIC) != SQLITE_OK) goto error;
#define BIND_INT(col, i) \
    if(sqlite3_bind_int(sql, (col), (i)) != SQLITE_OK) goto error;
#define BIND_DOUBLE(col, f) \
    if(sqlite3_bind_double(sql, (col), (f)) != SQLITE_OK) goto error;
#define BIND_NULL(col) \
    if(sqlite3_bind_null(sql, (col)) != SQLITE_OK) goto error;

#define BIND_VALUE(col, type, lex) \
    if((type) == VALUE_TYPE_BOOLEAN) { \
        BIND_INT(col, strcmp(lex, "true") == 0 || strcmp(lex, "1") == 0 ? 1 : 0); \
    } else if((type) >= VALUE_TYPE_FIRST_INTEGER && \
              (type) <= VALUE_TYPE_LAST_INTEGER) { \
        BIND_INT(col, atoi(lex)); \
    } else if((type) >= VALUE_TYPE_FIRST_FLOATING && \
              (type) <= VALUE_TYPE_LAST_FLOATING) { \
        BIND_DOUBLE(col, atof(lex)); \
    } else { \
        BIND_NULL(col); \
    }

    ts = triple->subject_type == RAPTOR_IDENTIFIER_TYPE_ANONYMOUS
         ? VALUE_TYPE_BLANK : VALUE_TYPE_IRI;
    START_SQL(d->sqlInsertVal);
    BIND_INT(1, ts);
    BIND_STR(2, (char*) triple->subject);
    BIND_INT(3, 0);
    BIND_NULL(4);
    STOP_SQL;

    tp = triple->predicate_type == RAPTOR_IDENTIFIER_TYPE_ANONYMOUS
         ? VALUE_TYPE_BLANK : VALUE_TYPE_IRI;
    START_SQL(d->sqlInsertVal);
    BIND_INT(1, tp);
    BIND_STR(2, (char*) triple->predicate);
    BIND_INT(3, 0);
    BIND_NULL(4);
    STOP_SQL;

    switch(triple->object_type) {
    case RAPTOR_IDENTIFIER_TYPE_ANONYMOUS:
        to = VALUE_TYPE_BLANK;
        lang = 0;
        break;
    case RAPTOR_IDENTIFIER_TYPE_RESOURCE:
        to = VALUE_TYPE_IRI;
        lang = 0;
        break;
    case RAPTOR_IDENTIFIER_TYPE_LITERAL:
        if(triple->object_literal_datatype == NULL)
            typeUri = NULL;
        else
            typeUri = (char*) raptor_uri_as_string(triple->object_literal_datatype);
        to = get_type(typeUri);
        if(triple->object_literal_language == NULL ||
           triple->object_literal_language[0] == '\0')
            lang = 0;
        else
            lang = -1;
        break;
    default:
        fprintf(stderr, "Unknown object type %d\n", triple->object_type);
        goto error;
    }

    if(to == VALUE_TYPE_UNKOWN) {
        START_SQL(d->sqlInsertDatatype);
        BIND_STR(1, typeUri);
        STOP_SQL;
    }

    if(lang == -1) {
        START_SQL(d->sqlInsertLanguage);
        BIND_STR(1, (char*) triple->object_literal_language);
        STOP_SQL;
    }

    if(to == VALUE_TYPE_UNKOWN) {
        START_SQL(d->sqlInsertValUnkTypeLang);
        BIND_STR(1, typeUri);
        BIND_STR(2, (char*) triple->object);
        BIND_STR(3, (char*) triple->object_literal_language);
        STOP_SQL;
        START_SQL(d->sqlInsertStmtUnkTypeLang);
        BIND_STR(1, (char*) triple->subject);
        BIND_INT(2, ts);
        BIND_STR(3, (char*) triple->predicate);
        BIND_INT(4, tp);
        BIND_STR(5, (char*) triple->object);
        BIND_STR(6, typeUri);
        BIND_STR(7, (char*) triple->object_literal_language);
        STOP_SQL;
    } else if(lang == 0) {
        START_SQL(d->sqlInsertVal);
        BIND_INT(1, to);
        BIND_STR(2, (char*) triple->object);
        BIND_INT(3, lang);
        BIND_VALUE(4, to, (char*) triple->object);
        STOP_SQL;
        START_SQL(d->sqlInsertStmt);
        BIND_STR(1, (char*) triple->subject);
        BIND_INT(2, ts);
        BIND_STR(3, (char*) triple->predicate);
        BIND_INT(4, tp);
        BIND_STR(5, (char*) triple->object);
        BIND_INT(6, to);
        BIND_INT(7, lang);
        STOP_SQL;
    } else {
        START_SQL(d->sqlInsertValUnkLang);
        BIND_INT(1, to);
        BIND_STR(2, (char*) triple->object);
        BIND_STR(3, (char*) triple->object_literal_language);
        BIND_VALUE(4, to, (char*) triple->object);
        STOP_SQL;
        START_SQL(d->sqlInsertStmtUnkLang);
        BIND_STR(1, (char*) triple->subject);
        BIND_INT(2, ts);
        BIND_STR(3, (char*) triple->predicate);
        BIND_INT(4, tp);
        BIND_STR(5, (char*) triple->object);
        BIND_INT(6, to);
        BIND_STR(7, (char*) triple->object_literal_language);
        STOP_SQL;
    }

#undef START_SQL
#undef STOP_SQL
#undef BIND_STR
#undef BIND_INT
#undef BIND_DOUBLE
#undef BIND_NULL

    return;

error:
    sqlerror(d);
}

int main(int argc, char* argv[]) {
    int c;
    bool force;
    char *syntax, *rdfpath, *dbpath;
    Data d;
    sqlite3_stmt *sql;
    ValueType t;

    force = false;
    syntax = "rdfxml";
    while((c = getopt(argc, argv, "s:f")) != -1) {
        switch(c) {
        case 's':
            syntax = optarg;
            break;
        case 'f':
            force = true;
            break;
        default:
            return 1;
        }
    }

    if(argc - optind < 1) {
        printf("Usage: %s [options] DB [RDF]\n", argv[0]);
        return 1;
    }
    dbpath = argv[optind++];
    rdfpath = optind < argc ? argv[optind++] : NULL;

    if(access(dbpath, F_OK) == 0) {
        if(force) {
            if(unlink(dbpath)) {
                perror("createsqlite");
                return 2;
            }
        } else {
            fprintf(stderr, "Database already exists. Exiting.\n");
            return 1;
        }
    }

    memset(&d, 0, sizeof(Data));

    if(sqlite3_open_v2(dbpath, &d.db,
                       SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                       NULL) != SQLITE_OK) {
        fprintf(stderr, "Unable to create database.\n");
        return 2;
    }

    raptor_init();

    if(sqlite3_exec(d.db,
                    "CREATE TABLE datatypes ("
                    "  id INTEGER PRIMARY KEY NOT NULL,"
                    "  uri TEXT UNIQUE ON CONFLICT IGNORE"
                    ");"
                    "CREATE TABLE languages ("
                    "  id INTEGER PRIMARY KEY NOT NULL,"
                    "  tag TEXT UNIQUE ON CONFLICT IGNORE"
                    ");"
                    "CREATE TABLE vals ("
                    "  id INTEGER PRIMARY KEY NOT NULL,"
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
                    "CREATE INDEX statements_spo"
                    "    ON statements (subject, predicate, object);"
                    "CREATE INDEX statements_sop"
                    "    ON statements (subject, object, predicate);"
                    "CREATE INDEX statements_pos"
                    "    ON statements (predicate, object, subject);"
                    "CREATE INDEX statements_osp"
                    "    ON statements (object, subject, predicate);"
                    "CREATE INDEX statements_ops"
                    "    ON statements (object, predicate, subject);"
                    "BEGIN TRANSACTION;"
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

#define SQL(var, sql) \
    if(sqlite3_prepare_v2(d.db, sql, -1, &d.var, NULL) != SQLITE_OK) \
        goto cleandb;

    SQL(sqlInsertDatatype, "INSERT INTO datatypes (uri) VALUES (?1)");
    SQL(sqlInsertLanguage, "INSERT INTO languages (tag) VALUES (?1)");
    SQL(sqlInsertVal,
        "INSERT INTO vals (type, lexical, language, value) "
        "    VALUES (?1, ?2, ?3, ?4)");
    SQL(sqlInsertValUnkLang,
        "INSERT INTO vals (type, lexical, language, value) "
        "    SELECT ?1, ?2, id, ?4 FROM languages WHERE tag = ?3");
    SQL(sqlInsertValUnkTypeLang,
        "INSERT INTO vals (type, lexical, language)"
        "    SELECT datatypes.id, ?2, languages.id"
        "    FROM datatypes, languages"
        "    WHERE datatypes.uri = ?1 AND languages.tag = ?3");
    SQL(sqlInsertStmt,
        "INSERT INTO statements (subject, predicate, object)"
        "    SELECT s.id, p.id, o.id"
        "    FROM vals AS s, vals AS p, vals AS o"
        "    WHERE s.lexical = ?1 AND s.type = ?2 AND"
        "          p.lexical = ?3 AND p.type = ?4 AND"
        "          o.lexical = ?5 AND o.type = ?6 AND o.language = ?7");
    SQL(sqlInsertStmtUnkLang,
        "INSERT INTO statements (subject, predicate, object)"
        "    SELECT s.id, p.id, o.id"
        "    FROM vals AS s, vals AS p, vals AS o,"
        "         languages ON languages.id = o.language"
        "    WHERE s.lexical = ?1 AND s.type = ?2 AND"
        "          p.lexical = ?3 AND p.type = ?4 AND"
        "          o.lexical = ?5 AND o.type = ?6 AND languages.tag = ?7;");
    SQL(sqlInsertStmtUnkTypeLang,
        "INSERT INTO statements (subject, predicate, object)"
        "    SELECT s.id, p.id, o.id"
        "    FROM vals AS s, vals AS p, vals AS o,"
        "         datatypes ON datatypes.id = o.type,"
        "         languages ON languages.id = o.language"
        "    WHERE s.lexical = ?1 AND s.type = ?2 AND"
        "          p.lexical = ?3 AND p.type = ?4 AND"
        "          o.lexical = ?5 AND datatypes.uri = ?6 AND languages.tag = ?7;");
#undef SQL

    d.parser = raptor_new_parser(syntax);
    if(d.parser == NULL) {
        fprintf(stderr, "Unable to create parser\n");
        goto cleandb;
    }
    raptor_set_statement_handler(d.parser, &d, add_triple);
    if(rdfpath == NULL) {
        d.fileURI = raptor_new_uri((unsigned char*) "http://www.example.org/");
        raptor_parse_file_stream(d.parser, stdin, NULL, d.fileURI);
    } else {
        d.fileURIstr = raptor_uri_filename_to_uri_string(rdfpath);
        d.fileURI = raptor_new_uri(d.fileURIstr);
        raptor_parse_file(d.parser, d.fileURI, NULL);
    }

    if(d.count >= 10000)
        printf("\n");
    printf("Imported %d triples.\n", d.count);

    if(sqlite3_exec(d.db, "COMMIT;", NULL, NULL, NULL) != SQLITE_OK)
        goto cleandb;

    cleanup(&d);
    return 0;

cleansql:
    sqlite3_finalize(sql);
cleandb:
    sqlerror(&d);
    return 2;
}
