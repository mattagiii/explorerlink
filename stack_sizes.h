/*
 * stack_sizes.h
 * Defines for all FreeRTOS task stack sizes.
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

#ifndef STACK_SIZES_H_
#define STACK_SIZES_H_

/* Stack sizes are in words. Byte sizes are shown in the comments. Byte sizes
 * must total less than configTOTAL_HEAP_SIZE (20000). */
#define ANALOGTASKSTACKSIZE             96      /* 384 */
#define CANTASKSTACKSIZE                64      /* 256 */
#define DATATASKSTACKSIZE               300     /* 1200 */
#define JSNTASKSTACKSIZE                64      /* 256 */
#define MODEMMGMTTASKSTACKSIZE          64      /* 256 */
#define MODEMUARTTASKSTACKSIZE          256     /* 1024 */
#define REMOTESTARTTASKSTACKSIZE        64      /* 256 */
#define SRFTASKSTACKSIZE                96      /* 384 */
                                                /*  total */

#endif /* STACK_SIZES_H_ */
