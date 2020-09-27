/*
 * ranging.c
 *
 *  Created on: 2020-6-26
 *      Author: FS00000
 */
#include "ranging.h"
#include <c6x.h>
#include <math.h>

#define RANGING_UARTNUM    (4)
//#define RANGING_UARTNUM    	(0)
#define RANGING_MBOX_DEPTH 	(16)
#define smooth_size 		(10)

/****************************************************************************/
/*                                                                          */
/*         变量                                                        								*/
/*                                                                          */
/****************************************************************************/
static Mailbox_Handle recvMbox;
static uint16_t fir_ranging;
static uartDataObj_t rangingUartDataObj;
static uartDataObj_t recvUartDataObj;
static const int UPDATE_INTERVAL = 50;
static int smooth[smooth_size] = {0};
static int wt_six[6] = {3,14,33,33,14,3};
static int wt_seven[7] = {3,9,23,30,23,9,3};
static int wt_eight[8] = {2,6,17,25,25,17,6,2};
static int wt_nine[9] = {2,5,12,19,24,19,12,5,2};

/****************************************************************************/
/*                                                                          */
/*              函数定义                                                   						    */
/*                                                                          */
/****************************************************************************/
static int get_max(int *data)
{
	int i;
	int index;
	int max = data[0];
	for(i = 0; i < smooth_size; i++)
	{
		if(data[i] > max)
		{
			max = data[i];
			index = i;
		}
	}

	return index;
}

static int get_min(int *data)
{
	int i;
	int index;
	int min = data[0];
	for(i = 0; i < smooth_size; i++)
	{
		if(data[i] < min)
		{
			min = data[i];
			index = i;
		}
	}

	return index;
}

static int get_max_next(int *data, int max_index)
{
	int i;
	int index = 0;
	int max = data[0];
	for(i = 0; i < smooth_size; i++)
	{
		if((data[i] > max) && (max_index != i))
		{
			max = data[i];
			index = i;
		}
	}

	return index;
}

static int get_min_next(int *data, int min_index)
{
	int i;
	int index = 0;
	int min = data[0];
	for(i = 0; i < smooth_size; i++)
	{
		if((data[i] < min) && (min_index != i))
		{
			min = data[i];
			index = i;
		}
	}

	return index;
}

static int get_max_next_next(int *data, int max_index, int max_index_next)
{
	int i;
	int index = 0;
	int max = data[0];
	for(i = 0; i < smooth_size; i++)
	{
		if((data[i] > max) && (max_index != i) && (max_index_next != i))
		{
			max = data[i];
			index = i;
		}
	}

	return index;
}

static int get_min_next_next(int *data, int min_index, int min_index_next)
{
	int i;
	int index = 0;
	int min = data[0];
	for(i = 0; i < smooth_size; i++)
	{
		if((data[i] < min) && (min_index != i) && (min_index_next != i))
		{
			min = data[i];
			index = i;
		}
	}

	return index;
}

static int fir_smooth(int data_in)
{
	int i = 0;
	int sum = 0;
	int smooth_fir = 0;
	int index_max = 0, index_min = 0, index_max_next = 0, index_min_next = 0;
	int index_max_next_next = 0, index_min_next_next = 0;
	static int count_index = 0;
	static int flag = 1;
	static int wt_fir[smooth_size] = {0};

	/*添加新数据*/
	if(flag)
	{
		flag = 0;
		for(i = 0; i < smooth_size; i++)
		{
			smooth[i] = data_in;
			wt_fir[i] = data_in;
		}
	}
	for(i = 0; i < smooth_size-1; i++)
	{
		smooth[i] = smooth[i+1];
	}
	smooth[smooth_size-1] = data_in;

	/*去除3个最大，1个最小*/
	index_max = get_max(smooth);
	index_min = get_min(smooth);
	index_max_next = get_max_next(smooth, index_max);
	index_min_next = get_min_next(smooth, index_min);
	index_max_next_next = get_max_next_next(smooth, index_max, index_max_next);
	index_min_next_next = get_min_next_next(smooth, index_min, index_min_next);
	for(i = 0; i < smooth_size; i++)
	{
		if((i != index_max) && (i != index_max_next) && (i != index_max_next_next) && (i != index_min))
		{
				wt_fir[count_index] = smooth[i];
				count_index++;
				//sum += smooth[i];
		}
	}

	if(count_index == 6)
	{
		for(i = 0; i < 6; i++)
		{
			smooth_fir += wt_fir[i]*wt_six[i];
		}
	}
	else if(count_index == 7)
	{
		for(i = 0; i < 7; i++)
		{
			smooth_fir += wt_fir[i]*wt_seven[i];
		}
	}
	else if(count_index == 8)
	{
		for(i = 0; i < 8; i++)
		{
			smooth_fir += wt_fir[i]*wt_eight[i];
		}
	}
	else if(count_index == 9)
	{
		for(i = 0; i < 9; i++)
		{
			smooth_fir += wt_fir[i]*wt_nine[i];
		}
	}
	else
	{
		;
	}

	//smooth_fir = (sum)/count_index;
	smooth_fir = smooth_fir/100;
	count_index = 0;

	return smooth_fir;
}

void RangingtaskInit(void)
{
	Task_Handle task;
	Task_Params taskParams;

    Task_Params_init(&taskParams);
	taskParams.priority = 5;
	taskParams.stackSize = 4096;

	task = Task_create(taskRanging, &taskParams, NULL);
	if (task == NULL) {
		sb_printf("Task_create() failed!\n");
		BIOS_exit(0);
	}
}

void taskRanging(UArg a0, UArg a1)
{
	uint8_t Ranging_query[1] = {0x55};
	Mailbox_Params mboxParams;
	p_msg_t pmsg;
	Bool result;
	int sendmsg = 0;
	int m_ranging = 0;

	//初始化串口
	UartNs550Init(RANGING_UARTNUM, RangingUartIntrHandler);
	UartNs550Recv(RANGING_UARTNUM, rangingUartDataObj.buffer, UART_REC_BUFFER_SIZE);

	/* 初始化接收邮箱 */
	Mailbox_Params_init(&mboxParams);
	recvMbox = Mailbox_create (sizeof (uartDataObj_t), RANGING_MBOX_DEPTH, &mboxParams, NULL);

	while(1)
	{
		/*
		 * 100ms更新一次距离_itoll(TSCH,TSCL)
		*/
		Task_sleep(UPDATE_INTERVAL);

		UartNs550Send(RANGING_UARTNUM, Ranging_query, 1);
		//UartNs550Send(RANGING_UARTNUM, Ranging_query, 3);

		result = Mailbox_pend(recvMbox,(Ptr*) &recvUartDataObj, 1000);

		if(!result)
		{
			continue;
		}

		if(recvUartDataObj.length != 2)
		{
			//sb_printf("Ranging error: recved %d bytes\n", recvUartDataObj.length);
		}

		m_ranging = recvUartDataObj.buffer[0] * 256 + recvUartDataObj.buffer[1];
		fir_ranging = fir_smooth(m_ranging);

		sb_printf("%ld	%ld\n", m_ranging, fir_ranging);
#if 1
		if(sendmsg >= 5)
		{
			sendmsg = 0;
			pmsg = Message_getEmpty();
			pmsg->type = ranging;
			memcpy( pmsg->data , &m_ranging ,sizeof(m_ranging));
			pmsg->dataLen = sizeof(m_ranging);
			Message_post(pmsg);
		}
		sendmsg++;
#endif
	}
}

void RangingUartIntrHandler(void *callBackRef, u32 event, unsigned int eventData)
{
	u8 Errors;
	u16 DeviceNum = *((u16 *)callBackRef);

	/*
	 * All of the data has been sent.
	 */
	if (event == XUN_EVENT_SENT_DATA) {
	}

	/*
	 * All of the data has been received.
	 */
	if (event == XUN_EVENT_RECV_DATA || event == XUN_EVENT_RECV_TIMEOUT) {
		rangingUartDataObj.length = eventData;
		Mailbox_post(recvMbox, (Ptr *)&rangingUartDataObj, BIOS_NO_WAIT);
        UartNs550Recv(DeviceNum, rangingUartDataObj.buffer, UART_REC_BUFFER_SIZE);
		//Mailbox_post(recvMbox, (Ptr *)&rangingUartDataObj, BIOS_NO_WAIT);
	}

	/*
	 * Data was received with an error, keep the data but determine
	 * what kind of errors occurred.
	 */
	if (event == XUN_EVENT_RECV_ERROR) {
		Errors = UartNs550GetLastErrors(DeviceNum);
	}
}

/*
 * 单位为mm,外部引用的距离数据
 */
int RangingGetDistance()
{
	return fir_ranging;
}
