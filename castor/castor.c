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

#include "castor.h"
#include "solver.h"
#include "constraints.h"

/**
 * Node corresponding to a pattern during the search
 */
typedef struct TPatternNode PatternNode;
struct TPatternNode {
    /**
     * Linked pattern
     */
    Pattern *pat;
    /**
     * PatternNode corresponding to pat->left
     */
    PatternNode *left;
    /**
     * PatternNode corresponding to pat->right
     */
    PatternNode *right;
    /**
     * Flag depending on the pattern type
     * BGP: depth of the posted subtree, or 0 if not posted
     * LEFTJOIN: 1 if the optional subpattern is consistent, 0 otherwise
     * UNION: 0 if exploring the left branch, 1 for the right branch
     */
    int flag;
};

struct TCastor {
    /**
     * Store of the dataset
     */
    Store *store;
    /**
     * Query to execute
     */
    Query *query;
    /**
     * CP solver
     */
    Solver *solver;
    /**
     * Root pattern node corresponding to the root graph pattern
     */
    PatternNode *root;
};

/**
 * @param pat a graph pattern
 * @return a new corresponding pattern node
 */
PatternNode* create_pattern_node(Pattern* pat) {
    PatternNode* node;

    node = (PatternNode*) malloc(sizeof(PatternNode));
    node->pat = pat;
    node->left = NULL;
    node->right = NULL;
    node->flag = 0;
    if(IS_PATTERN_TYPE_COMPOUND(pat->type)) {
        if(pat->left != NULL)
            node->left = create_pattern_node(pat->left);
        if(pat->right != NULL)
            node->right = create_pattern_node(pat->right);
    }
    return node;
}

Castor* new_castor(Store* store, Query* query) {
    Castor *self;

    self = (Castor*) malloc(sizeof(Castor));
    self->store = store;
    self->query = query;
    self->solver = new_solver(query->nbVars,
                              store_value_count(store) + 1);
    if(self->solver == NULL) {
        free(self);
        return NULL;
    }
    self->root = create_pattern_node(query->pattern);
    return self;
}

/**
 * Free a pattern node and all its children
 *
 * @param node a pattern node
 */
void free_pattern_node(PatternNode* node) {
    if(node->left != NULL) free_pattern_node(node->left);
    if(node->right != NULL) free_pattern_node(node->right);
    free(node);
}

void free_castor(Castor* self) {
    free_pattern_node(self->root);
    free_solver(self->solver);
    free(self);
}

/**
 * Visit a filter, break top-level AND-clauses down and post filter constraints.
 *
 * @param solver the solver
 * @param store the store
 * @param query the query
 * @param expr the filter expression
 */
void visit_filter(Solver* solver, Store* store, Expression* expr) {
    int i;
    Value v;

    if(expr->op == EXPR_OP_AND) {
        visit_filter(solver, store, expr->arg1);
        visit_filter(solver, store, expr->arg2);
        return;
    } else if(expr->op == EXPR_OP_EQ) {
        if(expr->arg1->op == EXPR_OP_VARIABLE &&
           expr->arg2->op == EXPR_OP_VARIABLE) {
            post_eq(solver, store, expr->arg1->variable->id,
                    expr->arg2->variable->id);
            return;
        } else if(expr->arg1->op == EXPR_OP_VARIABLE &&
                  expr->arg2->nbVars == 0) {
            if(!expression_evaluate(expr->arg2, &v)) {
                solver_fail(solver);
                return;
            }
            i = v.id >= 0 ? v.id :
                store_value_get_id(store, v.type, v.typeUri, v.lexical,
                                   v.languageTag);
            if(i < 0) {
                solver_fail(solver);
                return;
            }
            solver_label(solver, expr->arg1->variable->id, i);
            return;
        } else if(expr->arg2->op == EXPR_OP_VARIABLE &&
                  expr->arg1->nbVars == 0) {
            if(!expression_evaluate(expr->arg1, &v)) {
                solver_fail(solver);
                return;
            }
            i = v.id >= 0 ? v.id :
                store_value_get_id(store, v.type, v.typeUri, v.lexical,
                                   v.languageTag);
            if(i < 0) {
                solver_fail(solver);
                return;
            }
            solver_label(solver, expr->arg2->variable->id, i);
            return;
        }
    } else if(expr->op == EXPR_OP_NEQ) {
        if(expr->arg1->op == EXPR_OP_VARIABLE &&
           expr->arg2->op == EXPR_OP_VARIABLE) {
            post_diff(solver, store, expr->arg1->variable->id,
                      expr->arg2->variable->id);
            return;
        } else if(expr->arg1->op == EXPR_OP_VARIABLE &&
                  expr->arg2->nbVars == 0) {
            if(!expression_evaluate(expr->arg2, &v)) {
                solver_fail(solver);
                return;
            }
            i = v.id >= 0 ? v.id :
                store_value_get_id(store, v.type, v.typeUri, v.lexical,
                                   v.languageTag);
            if(i < 0) {
                solver_fail(solver);
                return;
            }
            solver_diff(solver, expr->arg1->variable->id, i);
            return;
        } else if(expr->arg2->op == EXPR_OP_VARIABLE &&
                  expr->arg1->nbVars == 0) {
            if(!expression_evaluate(expr->arg1, &v)) {
                solver_fail(solver);
                return;
            }
            i = v.id >= 0 ? v.id :
                store_value_get_id(store, v.type, v.typeUri, v.lexical,
                                   v.languageTag);
            if(i < 0) {
                solver_fail(solver);
                return;
            }
            solver_diff(solver, expr->arg2->variable->id, i);
            return;
        }
    }
    // fallback generic implementation
    post_filter(solver, store, expr);
}

bool sol(Castor* self, PatternNode* node) {
    Pattern *pat;
    int i, x;

    pat = node->pat;

    // Simple query
    if(pat->type == PATTERN_TYPE_BASIC ||
       (pat->type == PATTERN_TYPE_FILTER &&
        pat->left->type == PATTERN_TYPE_BASIC)) {
        if(node->flag == 0) {
            node->flag = solver_add_search(self->solver, pat->vars, pat->nbVars);
            if(node->flag == 0) { // we were already inconsistent, should not happen
                return false;
            }
            for(i = 0; i < pat->nbVars; i++)
                solver_diff(self->solver, pat->vars[i], 0);
            if(pat->type == PATTERN_TYPE_FILTER) {
                visit_filter(self->solver, self->store, pat->expr);
                pat = pat->left;
            }
            for(i = 0; i < pat->nbTriples; i++) {
                post_statement(self->solver, self->store, &pat->triples[i]);
            }
        } else if(node->flag != solver_search_depth(self->solver)) {
            return true; // another BGP is posted further down
        }
        if(solver_search(self->solver))
            return true;
        node->flag = 0;
        return false;
    }

    // Compound query
    switch(pat->type) {
    case PATTERN_TYPE_FALSE:
        return false;
    case PATTERN_TYPE_FILTER:
        while(sol(self, node->left)) {
            for(i = 0; i < pat->expr->nbVars; i++) {
                x = pat->expr->vars[i];
                if(solver_var_contains(self->solver, x, 0))
                    self->query->vars[x].value = NULL;
                else
                    self->query->vars[x].value = store_value_get(self->store,
                                        solver_var_value(self->solver, x));
            }
            if(expression_is_true(pat->expr))
                return true;
        }
        return false;
    case PATTERN_TYPE_JOIN:
        while(sol(self, node->left)) {
            if(sol(self, node->right))
                return true;
        }
        return false;
    case PATTERN_TYPE_LEFTJOIN:
        while(sol(self, node->left)) {
            if(sol(self, node->right)) {
                node->flag = 1;
                return true;
            } else if(node->flag == 0) {
                return true;
            } else {
                node->flag = 0;
            }
        }
        return false;
    case PATTERN_TYPE_UNION:
        if(node->flag == 0 && sol(self, node->left))
            return true;
        node->flag = 1;
        if(sol(self, node->right))
            return true;
        node->flag = 0;
        return false;
    default:
        // should not happen
        return false;
    }
    return false;
}

bool castor_next(Castor* self) {
    int i, n;

    if(!sol(self, self->root))
        return false;

    n = self->query->nbVars;
    for(i = 0; i < n; i++) {
        if(solver_var_contains(self->solver, i, 0))
            self->query->vars[i].value = NULL;
        else
            self->query->vars[i].value =
                    store_value_get(self->store,
                                    solver_var_value(self->solver, i));
    }

    return true;
}
