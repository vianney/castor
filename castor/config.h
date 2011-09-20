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
#ifndef CONFIG_H
#define CONFIG_H


/**
 * Sparsity threshold for choosing the method of finding the bound of the
 * domain.
 * @see castor::VarInt::_searchMin()
 */
#define CASTOR_SOLVER_VARINT_BOUND_SPARSITY 2

/**
 * Priority of castor::StatementConstraint
 */
#define CASTOR_CONSTRAINTS_STATEMENT_PRIORITY  PRIOR_LOW

/**
 * Priority of castor::FilterConstraint
 */
#define CASTOR_CONSTRAINTS_FILTER_PRIORITY  PRIOR_MEDIUM

#endif // CONFIG_H
