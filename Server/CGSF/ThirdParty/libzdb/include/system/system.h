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


#ifndef SYSTEM_INCLUDED
#define SYSTEM_INCLUDED
#include <stdarg.h>


/**
 * Systems routines
 *
 * @file
 */


/**
 * Returns a String describing the last system error
 * @return The last error message
 */
const char *System_getLastError(void);


/**
 * Returns a String describing the error code
 * @param error error code to lookup
 * @return The error string for the given code
 */
const char *System_getError(int error);


/**
 * Prints the given error message to <code>stderr</code> and 
 * <code>abort(3)</code> the application. If an AbortHandler callback 
 * function is defined for the library, this function is called instead.
 * @param e A formated (printf-style) message string
 */
void System_abort(const char *e, ...);// __attribute__((format (printf, 1, 2)));


/**
 * Prints the given error message to <code>stderr</code>. If an ErrorHandler
 * callback function is defined for the library, this function is called instead.
 * @param s A formated (printf-style) message string
 */
void System_debug(const char *s, ...);// __attribute__((format (printf, 1, 2)));


#endif
