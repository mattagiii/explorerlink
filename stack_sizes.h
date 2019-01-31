/*
 * stack_sizes.h
 *
 *  Created on: Oct 15, 2018
 *      Author: Matt
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
