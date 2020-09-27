/*
 * ranging.h
 *
 *  Created on: 2020-6-26
 *      Author: FS00000
 */

#ifndef RANGING_H_
#define RANGING_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xdc/std.h>
#include "uartns550.h"
#include "xil_types.h"
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/BIOS.h>
#include "xdc/runtime/System.h"
#include "Message/Message.h"
#include <ti/sysbios/knl/Task.h>

void RangingtaskInit(void);
void taskRanging(UArg a0, UArg a1);
void RangingUartIntrHandler(void *callBackRef, u32 event, unsigned int eventData);
int RangingGetDistance();

#endif /* RANGING_H_ */
