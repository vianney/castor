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

#include <string>
#include <iostream>
#include <sstream>

#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <sqlite3.h>
#include <raptor.h>

#include "librdf.h"
#include "model.h"

using namespace std;
using namespace castor;

////////////////////////////////////////////////////////////////////////////////
// Utilities

#define XSD_PREFIX "http://www.w3.org/2001/XMLSchema#"
#define XSD_PREFIX_LEN (sizeof(XSD_PREFIX) - 1)

ValueType get_type(char *uri) {
    if(uri == NULL || uri[0] == '\0')
        return VALUE_TYPE_PLAIN_STRING;

    if(strncmp(uri, XSD_PREFIX, XSD_PREFIX_LEN) != 0)
        return VALUE_TYPE_UNKOWN;

    char *fragment = &uri[XSD_PREFIX_LEN];
    for(ValueType t = VALUE_TYPE_FIRST_XSD; t <= VALUE_TYPE_LAST_XSD;
        t = (ValueType)(t+1)) {
        if(strcmp(fragment, &VALUETYPE_URIS[t][XSD_PREFIX_LEN]) == 0)
            return t;
    }
    return VALUE_TYPE_UNKOWN;
}

////////////////////////////////////////////////////////////////////////////////
// Sqlite wrappers

class SqliteException : public std::exception {
    string msg;

public:
    SqliteException(sqlite3 *db) : msg(sqlite3_errmsg(db)) {}
    SqliteException(sqlite3_stmt *stmt) : msg(sqlite3_errmsg(sqlite3_db_handle(stmt))) {}
    SqliteException(string msg) : msg(msg) {}
    SqliteException(const char* msg) : msg(msg) {}
    SqliteException(const SqliteException &o) : msg(o.msg) {}
    ~SqliteException() throw() {}

    const char *what() const throw() { return msg.c_str(); }
};

class SqliteDb {
    sqlite3 *db;
public:
    SqliteDb(const char* path, bool append) throw(SqliteException) {
        if(sqlite3_open_v2(path, &db,
                           SQLITE_OPEN_READWRITE | (append ? 0 : SQLITE_OPEN_CREATE),
                           NULL) != SQLITE_OK)
            throw SqliteException("Unable to create database.");
    }
    ~SqliteDb() { sqlite3_close(db); }
    operator sqlite3*() { return db; }
    void execute(const char* sql) throw(SqliteException) {
        if(sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK)
            throw SqliteException(db);
    }
};

class SqliteStatement {
    sqlite3_stmt *stmt;
public:
    SqliteStatement(SqliteDb *db, const char* sql) throw(SqliteException) {
        if(sqlite3_prepare_v2(*db, sql, -1, &stmt, NULL) != SQLITE_OK)
            throw SqliteException(*db);
    }
    ~SqliteStatement() {
        sqlite3_finalize(stmt);
    }

    void reset() throw(SqliteException) {
        if(sqlite3_reset(stmt) != SQLITE_OK)
            throw SqliteException(stmt);
    }
    void done() throw(SqliteException) {
        if(sqlite3_step(stmt) != SQLITE_DONE)
            throw SqliteException(stmt);
    }
    void bindStr(int col, const char *str) throw(SqliteException) {
        if(sqlite3_bind_text(stmt, col, str, -1, SQLITE_STATIC) != SQLITE_OK)
            throw SqliteException(stmt);
    }
    void bindInt(int col, int i) throw(SqliteException) {
        if(sqlite3_bind_int(stmt, col, i) != SQLITE_OK)
            throw SqliteException(stmt);
    }
    void bindFloat(int col, double f) throw(SqliteException) {
        if(sqlite3_bind_double(stmt, col, f) != SQLITE_OK)
            throw SqliteException(stmt);
    }
    void bindNull(int col) throw(SqliteException) {
        if(sqlite3_bind_null(stmt, col) != SQLITE_OK)
            throw SqliteException(stmt);
    }
    void bindVal(int col, ValueType type, const char *lex) throw(SqliteException) {
        if(type == VALUE_TYPE_BOOLEAN) {
            bindInt(col, strcmp(lex, "true") == 0 || strcmp(lex, "1") == 0 ? 1 : 0);
        } else if(type >= VALUE_TYPE_FIRST_INTEGER &&
                  type <= VALUE_TYPE_LAST_INTEGER) {
            bindInt(col, atoi(lex));
        } else if(type >= VALUE_TYPE_FIRST_FLOATING &&
                  type <= VALUE_TYPE_LAST_FLOATING) {
            bindFloat(col, atof(lex));
        } else {
            bindNull(col);
        }
    }
};

////////////////////////////////////////////////////////////////////////////////
// Store class

class ConvertException : public std::exception {
    string msg;

public:
    ConvertException(string msg) : msg(msg) {}
    ConvertException(const char* msg) : msg(msg) {}
    ConvertException(const char* msg, int val) {
        ostringstream str;
        str << msg << " " << val;
        this->msg = str.str();
    }
    ConvertException(const ConvertException &o) : msg(o.msg) {}
    ~ConvertException() throw() {}

    const char *what() const throw() { return msg.c_str(); }
};

class AppendStore {
    SqliteDb *db;
    SqliteStatement sqlInsertDatatype;
    SqliteStatement sqlInsertLanguage;
    SqliteStatement sqlInsertVal;
    SqliteStatement sqlInsertValUnkLang;
    SqliteStatement sqlInsertValUnkTypeLang;
    SqliteStatement sqlInsertStmt;
    SqliteStatement sqlInsertStmtUnkLang;
    SqliteStatement sqlInsertStmtUnkTypeLang;

    int count;

public:
    AppendStore(SqliteDb *db) : db(db),
        sqlInsertDatatype(db, "INSERT INTO datatypes (uri) VALUES (?1)"),
        sqlInsertLanguage(db, "INSERT INTO languages (tag) VALUES (?1)"),
        sqlInsertVal(db,
            "INSERT INTO vals (type, lexical, language, value) "
            "    VALUES (?1, ?2, ?3, ?4)"),
        sqlInsertValUnkLang(db,
            "INSERT INTO vals (type, lexical, language, value) "
            "    SELECT ?1, ?2, id, ?4 FROM languages WHERE tag = ?3"),
        sqlInsertValUnkTypeLang(db,
            "INSERT INTO vals (type, lexical, language)"
            "    SELECT datatypes.id, ?2, languages.id"
            "    FROM datatypes, languages"
            "    WHERE datatypes.uri = ?1 AND languages.tag = ?3"),
        sqlInsertStmt(db,
            "INSERT INTO statements (subject, predicate, object)"
            "    SELECT s.id, p.id, o.id"
            "    FROM vals AS s, vals AS p, vals AS o"
            "    WHERE s.lexical = ?1 AND s.type = ?2 AND"
            "          p.lexical = ?3 AND p.type = ?4 AND"
            "          o.lexical = ?5 AND o.type = ?6 AND o.language = ?7"),
        sqlInsertStmtUnkLang(db,
            "INSERT INTO statements (subject, predicate, object)"
            "    SELECT s.id, p.id, o.id"
            "    FROM vals AS s, vals AS p, vals AS o,"
            "         languages ON languages.id = o.language"
            "    WHERE s.lexical = ?1 AND s.type = ?2 AND"
            "          p.lexical = ?3 AND p.type = ?4 AND"
            "          o.lexical = ?5 AND o.type = ?6 AND languages.tag = ?7;"),
        sqlInsertStmtUnkTypeLang(db,
            "INSERT INTO statements (subject, predicate, object)"
            "    SELECT s.id, p.id, o.id"
            "    FROM vals AS s, vals AS p, vals AS o,"
            "         datatypes ON datatypes.id = o.type,"
            "         languages ON languages.id = o.language"
            "    WHERE s.lexical = ?1 AND s.type = ?2 AND"
            "          p.lexical = ?3 AND p.type = ?4 AND"
            "          o.lexical = ?5 AND datatypes.uri = ?6 AND languages.tag = ?7;"),
        count(0) {}

    int getCount() { return count; }

    void addTriple(raptor_statement* triple) {
        count++;
        if(count % 10000 == 0)
            cerr << '.';

        // Add subject value
        ValueType ts;
        char *lexs;
        switch(triple->subject->type) {
        case RAPTOR_TERM_TYPE_BLANK:
            ts = VALUE_TYPE_BLANK;
            lexs = reinterpret_cast<char*>(triple->subject->value.blank.string);
            break;
        case RAPTOR_TERM_TYPE_URI:
            ts = VALUE_TYPE_IRI;
            lexs = reinterpret_cast<char*>(raptor_uri_as_string(triple->subject->value.uri));
            break;
        default:
            throw ConvertException("Unknown subject type", triple->subject->type);
        }
        sqlInsertVal.reset();
        sqlInsertVal.bindInt(1, ts);
        sqlInsertVal.bindStr(2, lexs);
        sqlInsertVal.bindInt(3, 0);
        sqlInsertVal.bindNull(4);
        sqlInsertVal.done();

        // Add predicate value
        if(triple->predicate->type != RAPTOR_TERM_TYPE_URI)
            throw ConvertException("Unknown predicate type", triple->predicate->type);
        char *lexp = reinterpret_cast<char*>(raptor_uri_as_string(triple->predicate->value.uri));
        sqlInsertVal.reset();
        sqlInsertVal.bindInt(1, VALUE_TYPE_IRI);
        sqlInsertVal.bindStr(2, lexp);
        sqlInsertVal.bindInt(3, 0);
        sqlInsertVal.bindNull(4);
        sqlInsertVal.done();

        // Add object value and statement
        ValueType to;
        char *typeUri = NULL;
        char *lexo;
        int lang = 0;
        char *langTag = NULL;
        switch(triple->object->type) {
        case RAPTOR_TERM_TYPE_BLANK:
            to = VALUE_TYPE_BLANK;
            lexo = reinterpret_cast<char*>(triple->object->value.blank.string);
            break;
        case RAPTOR_TERM_TYPE_URI:
            to = VALUE_TYPE_IRI;
            lexo = reinterpret_cast<char*>(raptor_uri_as_string(triple->object->value.uri));
            break;
        case RAPTOR_TERM_TYPE_LITERAL:
            if(triple->object->value.literal.datatype != NULL)
                typeUri = reinterpret_cast<char*>(raptor_uri_as_string(triple->object->value.literal.datatype));
            to = get_type(typeUri);
            lexo = reinterpret_cast<char*>(triple->object->value.literal.string);
            if(triple->object->value.literal.language != NULL &&
               triple->object->value.literal.language[0] != '\0') {
                lang = -1;
                langTag = reinterpret_cast<char*>(triple->object->value.literal.language);
            }
            break;
        default:
            throw ConvertException("Unknown object type", triple->object->type);
        }

        if(to == VALUE_TYPE_UNKOWN) {
            sqlInsertDatatype.reset();
            sqlInsertDatatype.bindStr(1, typeUri);
            sqlInsertDatatype.done();
        }

        if(lang == -1) {
            sqlInsertLanguage.reset();
            sqlInsertLanguage.bindStr(1, langTag);
            sqlInsertLanguage.done();
        }

        if(to == VALUE_TYPE_UNKOWN) {
            sqlInsertValUnkTypeLang.reset();
            sqlInsertValUnkTypeLang.bindStr(1, typeUri);
            sqlInsertValUnkTypeLang.bindStr(2, lexo);
            sqlInsertValUnkTypeLang.bindStr(3, langTag);
            sqlInsertValUnkTypeLang.done();
            sqlInsertStmtUnkTypeLang.reset();
            sqlInsertStmtUnkTypeLang.bindStr(1, lexs);
            sqlInsertStmtUnkTypeLang.bindInt(2, ts);
            sqlInsertStmtUnkTypeLang.bindStr(3, lexp);
            sqlInsertStmtUnkTypeLang.bindInt(4, VALUE_TYPE_IRI);
            sqlInsertStmtUnkTypeLang.bindStr(5, lexo);
            sqlInsertStmtUnkTypeLang.bindStr(6, typeUri);
            sqlInsertStmtUnkTypeLang.bindStr(7, langTag);
            sqlInsertStmtUnkTypeLang.done();
        } else if(lang == 0) {
            sqlInsertVal.reset();
            sqlInsertVal.bindInt(1, to);
            sqlInsertVal.bindStr(2, lexo);
            sqlInsertVal.bindInt(3, lang);
            sqlInsertVal.bindVal(4, to, lexo);
            sqlInsertVal.done();
            sqlInsertStmt.reset();
            sqlInsertStmt.bindStr(1, lexs);
            sqlInsertStmt.bindInt(2, ts);
            sqlInsertStmt.bindStr(3, lexp);
            sqlInsertStmt.bindInt(4, VALUE_TYPE_IRI);
            sqlInsertStmt.bindStr(5, lexo);
            sqlInsertStmt.bindInt(6, to);
            sqlInsertStmt.bindInt(7, lang);
            sqlInsertStmt.done();
        } else {
            sqlInsertValUnkLang.reset();
            sqlInsertValUnkLang.bindInt(1, to);
            sqlInsertValUnkLang.bindStr(2, lexo);
            sqlInsertValUnkLang.bindStr(3, langTag);
            sqlInsertValUnkLang.bindVal(4, to, lexo);
            sqlInsertValUnkLang.done();
            sqlInsertStmtUnkLang.reset();
            sqlInsertStmtUnkLang.bindStr(1, lexs);
            sqlInsertStmtUnkLang.bindInt(2, ts);
            sqlInsertStmtUnkLang.bindStr(3, lexp);
            sqlInsertStmtUnkLang.bindInt(4, VALUE_TYPE_IRI);
            sqlInsertStmtUnkLang.bindStr(5, lexo);
            sqlInsertStmtUnkLang.bindInt(6, to);
            sqlInsertStmtUnkLang.bindStr(7, langTag);
            sqlInsertStmtUnkLang.done();
        }
    }
};

////////////////////////////////////////////////////////////////////////////////
// Raptor parser wrapper

void stmt_handler(void* user_data, raptor_statement* triple) {
    static_cast<AppendStore*>(user_data)->addTriple(triple);
}

class ParseException : public std::exception {
    string msg;

public:
    ParseException(string msg) : msg(msg) {}
    ParseException(const char* msg) : msg(msg) {}
    ParseException(const ParseException &o) : msg(o.msg) {}
    ~ParseException() throw() {}

    const char *what() const throw() { return msg.c_str(); }
};

class RDFParser {
    raptor_parser *parser;
    raptor_uri *fileURI;
    unsigned char *fileURIstr;

public:
    RDFParser(const char *syntax, const char *path) {
        parser = raptor_new_parser(librdf::World::instance().raptor, syntax);
        if(parser == NULL)
            throw ParseException("Unable to create parser");
        fileURIstr = raptor_uri_filename_to_uri_string(path);
        fileURI = raptor_new_uri(librdf::World::instance().raptor, fileURIstr);
    }
    ~RDFParser() {
        raptor_free_parser(parser);
        raptor_free_uri(fileURI);
        raptor_free_memory(fileURIstr);
    }
    void parse(AppendStore *store) {
        raptor_parser_set_statement_handler(parser, store, stmt_handler);
        raptor_parser_parse_file(parser, fileURI, NULL);
    }
};

////////////////////////////////////////////////////////////////////////////////
// Main routine

int main(int argc, char* argv[]) {
    // Parse options
    bool force = false;
    bool append = false;
    const char *syntax = "rdfxml";
    int c;
    while((c = getopt(argc, argv, "s:fa")) != -1) {
        switch(c) {
        case 's':
            syntax = optarg;
            break;
        case 'f':
            force = true;
            break;
        case 'a':
            append = true;
            break;
        default:
            return 1;
        }
    }

    if(argc - optind != 2) {
        cout << "Usage: " << argv[0] << " [options] DB RDF" << endl;
        return 1;
    }
    char *dbpath = argv[optind++];
    char *rdfpath = argv[optind++];

    // Create database
    if(!append && access(dbpath, F_OK) == 0) {
        if(force) {
            if(unlink(dbpath)) {
                perror("createsqlite");
                return 2;
            }
        } else {
            cerr << "Database already exists. Exiting." << endl;
            return 1;
        }
    }
    SqliteDb db(dbpath, append);

    if(!append) {
        db.execute("CREATE TABLE datatypes ("
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
                   "INSERT INTO languages (id, tag) VALUES (0, '');");

        SqliteStatement stmt(&db,
                             "INSERT INTO datatypes (id, uri) VALUES (?, ?)");
        for(ValueType t = VALUE_TYPE_BLANK; t < VALUE_TYPE_FIRST_CUSTOM;
            t = (ValueType)(t+1)) {
            stmt.reset();
            stmt.bindInt(1, t);
            if(VALUETYPE_URIS[t] == NULL)
                stmt.bindNull(2);
            else
                stmt.bindStr(2, VALUETYPE_URIS[t]);
            stmt.done();
        }
    } else {
        db.execute("BEGIN TRANSACTION;");
    }

    // Parse RDF dataset
    AppendStore store(&db);
    RDFParser parser(syntax, rdfpath);
    parser.parse(&store);

    // Finalize
    if(store.getCount() >= 10000)
        cerr << endl;
    cout << "Imported " << store.getCount() << " triples." << endl;

    db.execute("COMMIT;");
    return 0;
}
