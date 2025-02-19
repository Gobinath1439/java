/*
 * Copyright (C) Tildeslash Ltd. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef PREPAREDSTATEMENTDELEGATE_INCLUDED
#define PREPAREDSTATEMENTDELEGATE_INCLUDED


/**
 * This interface defines the <b>contract</b> for the concrete database 
 * implementation used for delegation in the PreparedStatement class.
 *
 * @file
 */ 

#define T PreparedStatementDelegate_T
#ifdef __cplusplus
typedef struct tag_T T;
typedef struct tag_Pop_T {
	const char *name;
        void (*free)(T *P);
        void (*setString)(T P, int parameterIndex, const char *x);
        void (*setInt)(T P, int parameterIndex, int x);
        void (*setLLong)(T P, int parameterIndex, long long int x);
        void (*setDouble)(T P, int parameterIndex, double x);
        void (*setBlob)(T P, int parameterIndex, const void *x, int size);
        void (*execute)(T P);
        ResultSet_T (*executeQuery)(T P);
}Pop_T;
#else
typedef struct T *T;
typedef struct Pop_T {
	const char *name;
	void(*free)(T *P);
	void(*setString)(T P, int parameterIndex, const char *x);
	void(*setInt)(T P, int parameterIndex, int x);
	void(*setLLong)(T P, int parameterIndex, long long int x);
	void(*setDouble)(T P, int parameterIndex, double x);
	void(*setBlob)(T P, int parameterIndex, const void *x, int size);
	void(*execute)(T P);
	ResultSet_T(*executeQuery)(T P);
}*Pop_T;
#endif

#undef T
#endif
