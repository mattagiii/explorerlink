/*
 * explorerlink_main.h
 *
 * Copyright 2018, 2019 Matt Rounds
 *
 * This file is part of ExplorerLink.
 *
 * ExplorerLink is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * ExplorerLink is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ExplorerLink. If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * PROJECT CONVENTIONS
 *
 * - Variable naming
 *      Variables have their type indicated at the start of the identifier,
 *      using the following format:
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

#ifndef EXPLORERLINK_MAIN_H_
#define EXPLORERLINK_MAIN_H_



#endif /* EXPLORERLINK_MAIN_H_ */
