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
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "castor.h"
#include "stores/store_sqlite.h"

#define BUFFER_SIZE 1024

#define DIFFTIME(start, stop) \
    ((long)((stop).ru_utime.tv_sec + (stop).ru_stime.tv_sec - \
           (start).ru_utime.tv_sec - (start).ru_stime.tv_sec) * 1000L + \
    (long)((stop).ru_utime.tv_usec + (stop).ru_stime.tv_usec - \
           (start).ru_utime.tv_usec - (start).ru_stime.tv_usec) / 1000L)

#define PRINT_TIME(msg, time) \
    printf(msg ": %ld.%03ld s\n", (time) / 1000, (time) % 1000); \
    fflush(stdout);

int main(int argc, char* argv[]) {
    char *dbpath, *rqpath, *solpath;
    FILE *f;
    char *queryString, buffer[BUFFER_SIZE];
    size_t read;
    int queryLen;
    Store *store;
    Query *query;
    Castor *engine;
    int nbSols, i;
    char *str;
    struct rusage ru[5];
    long diff;

    if(argc < 3 || argc > 4) {
        printf("Usage: %s DB QUERY [SOL]\n", argv[0]);
        return 1;
    }
    dbpath = argv[1];
    rqpath = argv[2];
    solpath = argc > 3 ? argv[3] : NULL;

    queryString = NULL;
    queryLen = 0;
    f = fopen(rqpath, "r");
    if(f == NULL) {
        perror("castor");
        return 2;
    }
    while((read = fread(buffer, sizeof(char), BUFFER_SIZE, f)) > 0) {
        queryString = (char*) realloc(queryString, (queryLen + read + 1) * sizeof(char));
        memcpy(queryString + queryLen, buffer, read);
        queryLen += read;
    }
    if(ferror(f)) {
        perror("castor");
        fclose(f);
        return 2;
    }
    fclose(f);
    if(queryString == NULL) {
        fprintf(stderr, "Empty query\n");
        return 0;
    }
    queryString[queryLen] = '\0';

    f = solpath == NULL ? stdout : fopen(solpath, "w");
    if(f == NULL) {
        perror("castor");
        f = stdout;
    }

    getrusage(RUSAGE_SELF, &ru[0]);

    store = sqlite_store_open(dbpath);
    if(store == NULL) {
        fprintf(stderr, "Unable to open %s\n", dbpath);
        goto error;
    }

    getrusage(RUSAGE_SELF, &ru[1]);

    diff = DIFFTIME(ru[0], ru[1]);
    PRINT_TIME("Store open", diff)

    query = new_query(store, queryString);
    if(query == NULL) {
        fprintf(stderr, "Unable to parse query\n");
        goto cleanstore;
    }

    query_print(query, stdout);

    getrusage(RUSAGE_SELF, &ru[2]);

    diff = DIFFTIME(ru[1], ru[2]);
    PRINT_TIME("Query parse", diff)

    engine = new_castor(store, query);
    if(engine == NULL)  {
        fprintf(stderr, "Unable to initialize Castor engine\n");
        goto cleanquery;
    }

    getrusage(RUSAGE_SELF, &ru[3]);

    diff = DIFFTIME(ru[2], ru[3]);
    PRINT_TIME("Engine init", diff)

    nbSols = 0;
    while(castor_next(engine)) {
        nbSols++;
        if(query->nbRequestedVars == 0) {
            fprintf(f, "YES\n");
        } else {
            for(i = 0; i < query->nbRequestedVars; i++) {
                str = model_value_string(query->vars[i].value);
                fprintf(f, "%s ", str);
                free(str);
            }
            fprintf(f, "\n");
        }
        if(query->limit > 0 && nbSols >= query->limit)
            break;
    }

    getrusage(RUSAGE_SELF, &ru[4]);

    diff = DIFFTIME(ru[3], ru[4]);
    PRINT_TIME("Engine search", diff)

    if(query->nbRequestedVars == 0 && nbSols == 0)
        fprintf(f, "NO\n");

    if(solpath != NULL)
        fclose(f);

    printf("Found: %d\n", nbSols);
    printf("Time: %ld\n", DIFFTIME(ru[2], ru[4]));
    printf("Memory: %ld\n", ru[4].ru_maxrss);

    free_castor(engine);
    free_query(query);
    store_close(store);
    free(queryString);
    return 0;

cleanquery:
    free_query(query);
cleanstore:
    store_close(store);
error:
    free(queryString);
    return 2;
}
