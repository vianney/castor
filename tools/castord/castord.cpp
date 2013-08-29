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

#include "mongoose.h"

#include <iostream>
#include <sstream>

#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <csignal>

#if CASTOR_SEARCH == CASTOR_SEARCH_random
#include <cstdlib>
#include <ctime>
#endif

#include "store.h"
#include "query.h"

using namespace std;
using namespace castor;

namespace {

////////////////////////////////////////////////////////////////////////////////
// Default parameters

static const char* DEFAULT_PORT = "8000";
static const char* PATH = "/sparql";
static const char* HOMEPATH = "/";
static const unsigned DEFAULT_CACHE = 100;

static constexpr size_t MAX_QUERY_LEN = 32768;
static constexpr size_t MAX_POST_LEN = MAX_QUERY_LEN * 2;

////////////////////////////////////////////////////////////////////////////////
// Global variables

static const char* progname;
static bool verbose;
static const char* mimetype = "application/sparql-results+xml";

////////////////////////////////////////////////////////////////////////////////
// HTTP handler

static void start_response(mg_connection* conn, const char* content_type) {
    if(verbose)
        cout << "200 (OK)" << endl;
    mg_printf(conn,
              "HTTP/1.0 200 OK\r\n"
              "Content-Type: %s\r\n"
              "\r\n",
              content_type);
}

static int send_error(mg_connection* conn, unsigned status, const char* msg) {
    if(verbose)
        cout << status << " (" << msg << ")" << endl;
    mg_printf(conn, "HTTP/1.0 %d %s\r\n\r\n", status, msg);
    return 1;
}

static void escape_xml(mg_connection* conn, const char* str) {
    while(*str) {
        switch(*str) {
        case '<':
            mg_write(conn, "&lt;", 4);
            break;
        case '>':
            mg_write(conn, "&gt;", 4);
            break;
        case '&':
            mg_write(conn, "&amp;", 5);
            break;
        case '"':
            mg_write(conn, "&quot;", 6);
            break;
        default:
            mg_write(conn, str, 1);
        }
        str++;
    }
}

static int handler(mg_connection* conn) {
    const mg_request_info* req = mg_get_request_info(conn);
    Store* store = reinterpret_cast<Store*>(req->user_data);

    if(verbose)
        cout << req->request_method << " " << req->uri << " ";

    if(strcmp(req->uri, HOMEPATH) == 0 &&
            strcmp(req->request_method, "GET") == 0) {
        start_response(conn, "text/html");
        mg_printf(conn,
                  "<html>"
                  "<head><title>Castor SPARQL Endpoint</title></head>"
                  "<body>"
                  "<h1>Castor SPARQL Endpoint</h1>"
                  "<form action=\"%s\" method=\"POST\">"
                  "<textarea name=\"query\" cols=\"80\" rows=\"15\">"
                  "SELECT * WHERE { ?s ?p ?o }"
                  "</textarea>"
                  "<input type=\"submit\" value=\"Run\" />"
                  "</form>"
                  "</body></html>", PATH);
        return 1;
    }

    if(strcmp(req->uri, PATH) != 0)
        return send_error(conn, 404, "Not found.");

    char querystr[MAX_QUERY_LEN];
    int ret;
    if(strcmp(req->request_method, "GET") == 0) {
        ret = mg_get_var(req->query_string, strlen(req->query_string), "query",
                         querystr, sizeof(querystr));
    } else if(strcmp(req->request_method, "POST") == 0) {
        char data[MAX_POST_LEN];
        int len = mg_read(conn, data, sizeof(data));
        ret = mg_get_var(data, len, "query", querystr, sizeof(querystr));
    } else {
        return send_error(conn, 405, "Unsupported method.");
    }
    if(ret == -1)
        return send_error(conn, 400, "Need to specify query.");
    else if(ret == -2)
        return send_error(conn, 500, "Query too long.");

    try {
        Query query(store, querystr);
        start_response(conn, mimetype);
        if(verbose)
            cout << "--" << endl << querystr << endl << "--" << endl;
        mg_printf(conn,
                  "<?xml version=\"1.0\"?>\n"
                  "<sparql xmlns=\"http://www.w3.org/2005/sparql-results#\">\n"
                  "  <head>\n");
        for(unsigned i = 0; i < query.requested(); ++i) {
            mg_printf(conn, "    <variable name=\"");
            escape_xml(conn, query.variable(i)->name().c_str());
            mg_printf(conn, "\"/>\n");
        }
        mg_printf(conn, "  </head>\n");
        if(query.requested() == 0) {
            query.next();
            mg_printf(conn, "  <boolean>%s</boolean>\n",
                      query.count() == 0 ? "false" : "true");
        } else {
            mg_printf(conn, "  <results distinct=\"%s\" ordered=\"%s\">\n",
                      query.isDistinct() ? "true" : "false",
                      query.orders().empty() ? "false" : "true");
            while(query.next()) {
                mg_printf(conn, "    <result>\n");
                for(unsigned i = 0; i < query.requested(); ++i) {
                    Variable* var = query.variable(i);
                    Value::id_t id = var->valueId();
                    if(id != 0) {
                        mg_printf(conn, "      <binding name=\"");
                        escape_xml(conn, var->name().c_str());
                        mg_printf(conn, "\">");
                        Value val = store->lookupValue(id);
                        val.ensureDirectStrings(*store);
                        switch(val.category()) {
                        case Value::CAT_BLANK:
                            mg_printf(conn, "<bnode>");
                            escape_xml(conn, val.lexical().str());
                            mg_printf(conn, "</bnode>");
                            break;
                        case Value::CAT_URI:
                            mg_printf(conn, "<uri>");
                            escape_xml(conn, val.lexical().str());
                            mg_printf(conn, "</uri>");
                            break;
                        case Value::CAT_SIMPLE_LITERAL:
                            mg_printf(conn, "<literal>");
                            escape_xml(conn, val.lexical().str());
                            mg_printf(conn, "</literal>");
                            break;
                        case Value::CAT_PLAIN_LANG:
                            mg_printf(conn, "<literal xml:lang=\"");
                            escape_xml(conn, val.language().str());
                            mg_printf(conn, "\">");
                            escape_xml(conn, val.lexical().str());
                            mg_printf(conn, "</literal>");
                            break;
                        default:
                            mg_printf(conn, "<literal datatype=\"");
                            escape_xml(conn, val.datatypeLex().str());
                            mg_printf(conn, "\">");
                            escape_xml(conn, val.lexical().str());
                            mg_printf(conn, "</literal>");
                        }
                        mg_printf(conn, "</binding>\n");
                    }
                }
                mg_printf(conn, "    </result>\n");
            }
            mg_printf(conn, "  </results>\n");
        }
        mg_printf(conn, "</sparql>");
        if(verbose) {
            cout << "  Solutions: " << query.count() << endl;
            cout << "  Backtracks: " << query.solver()->statBacktracks() << endl;
            cout << "  Subtrees: " << query.solver()->statSubtrees() << endl;
            cout << "  Post: " << query.solver()->statPost() << endl;
            cout << "  Propagate: " << query.solver()->statPropagate() << endl;
            cout << "  Cache hit: " << store->statTripleCacheHits() << endl;
            cout << "  Cache miss: " << store->statTripleCacheMisses() << endl;
#ifdef CASTOR_CSTR_TIMING
            cout << "  Constraints:" << endl;
            for(const auto& item : query.solver()->statCstrCount()) {
                cout << "    " << item.first.name() << ": " << item.second
                     << " (" << query.solver()->statCstrTime().at(item.first)
                     << "ms)" << endl;
            }
#endif
        }
    } catch(CastorException e) {
        return send_error(conn, 400, e.what());
    }
    return 1;
}

////////////////////////////////////////////////////////////////////////////////
// CLI

static void usage() {
    cout << "Usage: " << progname << " [options] -d DB" << endl;
    cout << endl << "Options:" << endl;
    cout << "  -d DB         Dataset to load" << endl;
    cout << "  -p PORT       Port to listen on (default: " << DEFAULT_PORT << ")" << endl;
    cout << "  -c CAPACITY   Triple cache capacity (default: " << DEFAULT_CACHE << ")" << endl;
    cout << "  -x            Use application/xml content type for results." << endl;
    cout << "  -v            Be verbose" << endl;
    exit(1);
}

}

int main(int argc, char* argv[]) {
    progname = argv[0];
    int c;
    char* dbpath = nullptr;
    const char* port = DEFAULT_PORT;
    unsigned cache = DEFAULT_CACHE;
    verbose = false;
    while((c = getopt(argc, argv, "d:p:c:xv")) != -1) {
        switch(c) {
        case 'd': dbpath = optarg;                   break;
        case 'p': port = optarg;                     break;
        case 'c': cache = atoi(optarg);              break;
        case 'x': mimetype = "application/xml";      break;
        case 'v': verbose = true;                    break;
        default: usage();
        }
    }

#if CASTOR_SEARCH == CASTOR_SEARCH_random
    srand(time(nullptr));
#endif

    // Load database
    if(dbpath == nullptr)
        usage();
    if(verbose)
        cout << "Loading " << dbpath << "." << endl;
    Store store(dbpath, cache);

    // Start HTTP server
    mg_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.begin_request = handler;

    const char* options[] = {"listening_ports", port,
                             "num_threads", "1",
                             nullptr};

    mg_context* ctx = mg_start(&callbacks, &store, options);
    if(ctx == nullptr) {
        perror("castord");
        return 2;
    }
    if(verbose)
        cout << "Listening on :" << port << "." << endl;

    // Wait for SIGINT or SIGTERM
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigprocmask(SIG_BLOCK, &set, nullptr);
    int sig;
    sigwait(&set, &sig);

    // Clean up
    if(verbose)
        cout << "Exiting." << endl;
    mg_stop(ctx);
    return 0;
}
