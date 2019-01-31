/*
 * explorer_main.h
 *
 *  Created on: May 10, 2018
 *      Author: Matt
 */

/*
 * PROJECT CONVENTIONS
 *
 * - Variable naming
 *      Variables have their return type indicated at the start of the
 *      identifier, using the following format:
 *      - v = void
 *      - b = bool
 *      - c = int8_t
 *      - s = int16_t
 *      - l = int32_t
 *      - uc = uint8_t
 *      - us = uint16_t
 *      - ul = uint32_t
 *      - e = enum
 *      - x = structs and other complex types
 *      Pointers to the above types are indicated with an additionally prefixed
 *      'p'.
 *
 *      If a variable is a measurable quantity with units, the units are also
 *      abbreviated as a suffix.
 *
 *      For example, usSampleRateHz indicates a uint16_t variable with units of
 *      Hertz.
 *
 * - Function naming
 *      Public functions have their return type indicated at the start of the
 *      identifier, using the same format as variable names above.
 *
 *      This is followed by the name of the file they are contained in. For
 *      example, ulSampleGetMinPeriod returns a uint32_t and is contained in
 *      sample.h.
 *
 *      Private functions do not use the above scheme, but are still generally
 *      prefixed with the file name unless they are nonspecific helper
 *      functions.
 *
 * - Abbreviations
 *      For clarity, all abbreviations used in the project are disambiguated
 *      here (save for standardized abbreviations such as units):
 *      - ch = channel
 *      - cmd = command
 *      - ms = milliseconds
 *      - rcv = receive
 *      - rcvd = received
 *      - rsp = response
 *
 */

#ifndef EXPLORER_MAIN_H_
#define EXPLORER_MAIN_H_



#endif /* EXPLORER_MAIN_H_ */
