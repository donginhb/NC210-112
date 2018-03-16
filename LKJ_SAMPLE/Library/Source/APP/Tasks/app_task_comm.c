/*******************************************************************************
 *   Filename:       app_task_comm.c
 *   Revised:        All copyrights reserved to wumingshen.
 *   Revision:       v1.0
 *   Writer:	     wumingshen.
 *
 *   Description:    ˫��ѡ�� comm �� Ctrl + H, ��ѡ Match the case, Replace with
 *                   ������Ҫ�����֣���� Replace All
 *                   ˫��ѡ�� Comm �� Ctrl + H, ��ѡ Match the case, Replace with
 *                   ������Ҫ�����֣���� Replace All
 *                   ˫��ѡ�� COMM �� Ctrl + H, ��ѡ Match the case, Replace with
 *                   ������Ҫ�����֣���� Replace All
 *                   �� app_cfg.h ��ָ��������� ���ȼ�  �� APP_TASK_COMM_PRIO     ��
 *                                            �� �����ջ�� APP_TASK_COMM_STK_SIZE ����С
 *
 *   Notes:
 *     				E-mail: shenchangwei945@163.com
 *
 *******************************************************************************/

/*******************************************************************************
 * INCLUDES
 */
#define  SNL_APP_SOURCE
#include <includes.h>
#include <app_comm_protocol.h>
#include <bsp_flash.h>
#include <iap.h>
#include <xprintf.h>
#include "app_task_mater.h"
#include "app_collector.h"
#include "dtc.h"
#include <math.h>
#include "task.h"
#include "task_comm.h"


#ifdef VSC_INCLUDE_SOURCE_FILE_NAMES
const  CPU_CHAR  *app_task_comm__c = "$Id: $";
#endif

#define APP_TASK_COMM_EN     DEF_ENABLED
#if APP_TASK_COMM_EN == DEF_ENABLED
/*******************************************************************************
 * CONSTANTS
 */

/*******************************************************************************
 * MACROS
 */
#define CYCLE_TIME_TICKS            (OS_TICKS_PER_SEC * 1)

/*******************************************************************************
 * TYPEDEFS
 */
__packed
typedef struct
{
uint8 ch;
uint8 dest;
uint8 service;
uint8 cmd;
uint8 len;
uint8 chksum;
}ST_SAMPLE_CMD_STR;
/***********************************************
* ������ ������ƿ飨TCB��
*/
static  OS_TCB      AppTaskCommTCB;
extern OS_Q                COM_RxQ;

ST_MONITOR speed_collector_monitor = {0};


OS_TMR	app_comm_tmr = {0};
#ifdef SAMPLE_BOARD
OS_TMR app_collection_info_tmr = {0};
#else
OS_TMR	app_sample_tmr = {0};
OS_TMR	collector_timerout_tmr = {0};
#endif
uint16	collector_update_count_down_tmr = 0;
//OS_Q app_comm_queue = {0};
/***********************************************
* ������ �����ջ��STACKS��
*/
static  CPU_STK     AppTaskCommStk[ APP_TASK_COMM_STK_SIZE ];


const uint8 get_collector_board_info[] = {0x00,0x2,0x81,0x01,0x0};

COLLECTION_STEP collect_step =COLLECTION_IDLE;


ST_DTC_MANGER dtc ={0};

/*******************************************************************************
* Description  : �ɼ���Ӧ����Ϣȫ�ֱ�����ʼ��
* Author       : 2018/1/23 ���ڶ�, by redmorningcn
*******************************************************************************/
ST_COLLECTOR_RESPONCE collector_responce =
{
	0x01,
	0x01,
	0x80,
	0x01,
	0x00,
	{
		0,				 /*����״̬;0:����1:��챻��*/
		0,				/*reserved*/
		90,				/*��λ��*/
		5000,			/*����ת������*/
		{
			{
				49,			/*ռ�ձ�*/
				15000,		/*������ѹ��λΪmv*/
				15000,		/*�ߵ�ƽ��ѹ��λΪmv*/
				1500,		/*�͵�ƽ��ѹ��λΪmv*/
				20,			/* �����ر���ʱ�䵥λ0.1us*/
				20,			/*�½��ر���ʱ�䵥λ0.1us*/
			},
			{
				51,			/*ռ�ձ�*/
				15000,		/*������ѹ��λΪmv*/
				15000,		/*�ߵ�ƽ��ѹ��λΪmv*/
				1500,		/*�͵�ƽ��ѹ��λΪmv*/
				20,			/* �����ر���ʱ�䵥λ0.1us*/
				20,			/*�½��ر���ʱ�䵥λ0.1us*/
			},
		},
	},
	0x00,
};




/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */

/*******************************************************************************
 * LOCAL FUNCTIONS
 */
void         AppTaskComm                 (void *p_arg);
static void         APP_CommInit                (void);
void         App_McuStatusInit           (void);
void         ReportDevStatusHandle       (void);
void         InformCommConfigMode        (u8 mode);
void         App_ModbusInit              (void);

static void dtc_init(void);
INT08U              APP_CommRxDataDealCB        (MODBUS_CH  *pch);
INT08U              IAP_CommRxDataDealCB        (MODBUS_CH  *pch);
void app_abnormal_info_update(ST_COLLECTOR_INFO *p_collector_info, BOARD_ABNORMAL_STATUS *p_abnormal_info);
static void app_dtc_update(BOARD_ABNORMAL_STATUS *p_abnormal_info);


/*******************************************************************************
 * GLOBAL FUNCTIONS
 */

/*******************************************************************************
 * EXTERN VARIABLES
 */
ST_para_storage system_para;
StrMater            Mater;


 /*******************************************************************************
 * EXTERN FUNCTIONS
 */
extern void         uartprintf                  (MODBUS_CH  *pch,const char *fmt, ...);

/*******************************************************************************/


/*******************************************************************************
 * ��    �ƣ� App_TaskCommCreate
 * ��    �ܣ� **���񴴽�
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ� wumingshen.
 * �������ڣ� 2015-02-05
 * ��    �ģ�
 * �޸����ڣ�
 * ��    ע�� ���񴴽�������Ҫ��app.h�ļ�������
 *******************************************************************************/
void  App_TaskCommCreate(void)
{
    OS_ERR  err;

    /***********************************************
    * ������ ���񴴽�
    */
    OSTaskCreate((OS_TCB     *)&AppTaskCommTCB,                     // ������ƿ�  ����ǰ�ļ��ж��壩
                 (CPU_CHAR   *)"App Task Comm",                     // ��������
                 (OS_TASK_PTR ) AppTaskComm,                        // ������ָ�루��ǰ�ļ��ж��壩
                 (void       *) 0,                                  // ����������
                 (OS_PRIO     ) APP_TASK_COMM_PRIO,                 // �������ȼ�����ͬ�������ȼ�������ͬ��0 < ���ȼ� < OS_CFG_PRIO_MAX - 2��app_cfg.h�ж��壩
                 (CPU_STK    *)&AppTaskCommStk[0],                  // ����ջ��
                 (CPU_STK_SIZE) APP_TASK_COMM_STK_SIZE / 10,        // ����ջ�������ֵ
                 (CPU_STK_SIZE) APP_TASK_COMM_STK_SIZE,             // ����ջ��С��CPU���ݿ�� * 8 * size = 4 * 8 * size(�ֽ�)����app_cfg.h�ж��壩
                 (OS_MSG_QTY  ) 0u,                                 // ���Է��͸�����������Ϣ��������
                 (OS_TICK     ) 0u,                                 // ��ͬ���ȼ��������ѭʱ�䣨ms����0ΪĬ��
                 (void       *) 0,                                  // ��һ��ָ����������һ��TCB��չ�û��ṩ�Ĵ洢��λ��
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK |               // �����ջ��������
                                OS_OPT_TASK_STK_CLR),               // ��������ʱ��ջ����
                 (OS_ERR     *)&err);                               // ָ���������ָ�룬���ڴ����������

}




/*
void app_get_collector_info()
{
	uint8 send_buf[250] = {0};
	uint8 len;
	memcpy(send_buf, get_collector_board_info, sizeof(get_collector_board_info));
//	send_buf[4] = sizeof(get_collector_board_info);
//	memcpy(&send_buf[5], &test_collector_info, sizeof(test_collector_info));
	len = sizeof(get_collector_board_info);
	nmb_send_data(Ctrl.Com.pch, (uint8 *)&send_buf, len);
}
*/

/*******************************************************************************
* ��    �ƣ� 
* ��    �ܣ� 
* ��ڲ����\
* ���ڲ����� ��
* ��  ���ߣ� fourth.peng.
* �������ڣ� 2017-4-28
* ��    �ģ�
* �޸����ڣ�
* ��    ע��
*******************************************************************************/

/*******************************************************************************
* ��    �ƣ� verify_checksum_of_nmb_frame
* ��    �ܣ� 
* ��ڲ����\
* ���ڲ����� ��
* ��  ���ߣ� fourth.peng.
* �������ڣ� 2017-4-28
* ��    �ģ�
* �޸����ڣ�
* ��    ע��
*******************************************************************************/
uint8 verify_checksum_of_nmb_frame(uint8 *data, uint16 len)
{
	uint8 i;
	uint8 checksum = 0;
	for(i = 0; i< (len-1); i++)
	{
		checksum += data[i];
	}

	if(checksum == data[len-1])
	{
		return TRUE;
	}

	return FALSE;
}

 typedef struct
 {
     uint8 board_a_lost_cnt;
     uint8 board_b_lost_cnt;
     uint8 board_c_lost_cnt;
 }collector_lost_cnt_type;

collector_lost_cnt_type collector_lost = {0};

/*******************************************************************************
* ��    �ƣ� collector_responce_analyze
* ��    �ܣ� 
* ��ڲ����\
* ���ڲ����� ��
* ��  ���ߣ� fourth.peng.
* �������ڣ� 2017-4-28
* ��    �ģ�
* �޸����ڣ�
* ��    ע��
*******************************************************************************/
void collector_responce_analyze( const ST_COLLECTOR_RESPONCE *p_collector_info)
{
	switch(p_collector_info->addr)
	{
		case COLLECTOR_BOARD_A_ADDR:
//			if(BOARD_A_COLLECTING == collect_step)
			{
				memcpy(&Mater.monitor.collector_board[0], (const void *)&p_collector_info->collector_info, sizeof(ST_COLLECTOR_INFO));
				collector_lost.board_a_lost_cnt = 0;
				Mater.monitor.abnormal_info[0].connected_fail = FALSE;
				system_send_msg(COMM_TASK_ID, COMM_TASK_COLLECT_BOARD_B, NULL, 0);
			}
			
			break;
			
		case COLLECTOR_BOARD_B_ADDR:
//			if(BOARD_B_COLLECTING == collect_step)
			{
				memcpy(&Mater.monitor.collector_board[1], (const void *)&p_collector_info->collector_info, sizeof(ST_COLLECTOR_INFO));
				collector_lost.board_b_lost_cnt = 0;
				Mater.monitor.abnormal_info[1].connected_fail = FALSE;
				system_send_msg(COMM_TASK_ID, COMM_TASK_COLLECT_BOARD_C, NULL, 0);
			}			
			break;
			
		case COLLECTOR_BOARD_C_ADDR:
//			if(BOARD_C_COLLECTING == collect_step)
			{
				memcpy(&Mater.monitor.collector_board[2], (const void *)&p_collector_info->collector_info, sizeof(ST_COLLECTOR_INFO));
				collector_lost.board_c_lost_cnt = 0;
				Mater.monitor.abnormal_info[2].connected_fail = FALSE;
				system_send_msg(COMM_TASK_ID, COMM_TASK_COLLECT_COMPLETE, NULL, 0);
			}
			break;
	}
}

/*******************************************************************************
* ��    �ƣ� app_collection_info_tmr_callback
* ��    �ܣ� 
* ��ڲ����\
* ���ڲ����� ��
* ��  ���ߣ� fourth.peng.
* �������ڣ� 2017-5-11
* ��    �ģ�
*******************************************************************************/
void app_collection_info_tmr_callback(OS_TMR *ptmr,void *p_arg)
{
	system_send_msg(COMM_TASK_ID, COMM_TASK_COLLECTION_INFO_UPDATE, NULL, 0);
}
/*******************************************************************************
* ��    �ƣ� comm_cmd_str_analyze
* ��    �ܣ� 
* ��ڲ����\
* ���ڲ����� ��
* ��  ���ߣ� fourth.peng.
* �������ڣ� 2017-4-28
* ��    �ģ�
*******************************************************************************/
void comm_cmd_str_analyze(const ST_SAMPLE_CMD_STR *str)
{
#ifdef SAMPLE_BOARD
	uint16 send_dly_time = 0;
	uint16 event = (uint16)(str->service<<8) + str->cmd;
	static ST_QUEUETCB post_queue = {0};
	OS_ERR err;

	if (EVENT_REQUEST_COLLECTION_INFO == event)
	{
		if( 0 ==  str->dest)
		{
		//broad cast mode
			if ( 1 < Ctrl.sample_id)
			{
				send_dly_time = (Ctrl.sample_id-1)*INTERNAL_TIME;
#if 0
				OSTmrCreate(&app_collection_info_tmr,
								(CPU_CHAR *)"app_collection_info_tmr",
								send_dly_time, 
								send_dly_time, 
								OS_OPT_TMR_ONE_SHOT, 
								(OS_TMR_CALLBACK_PTR)app_collection_info_tmr_callback, 
								NULL, 
								&err);
				OSTmrStart(&app_collection_info_tmr, &err);
#else
				collector_update_count_down_tmr = send_dly_time;
#endif
			
			}
			else
			{
				system_send_msg(COMM_TASK_ID, COMM_TASK_COLLECTION_INFO_UPDATE, NULL, 0);
				
			}
		}
		else if(Ctrl.sample_id == str->dest)
		{
		//��ѯģʽ���������Ͳ�����Ϣ
			system_send_msg(COMM_TASK_ID, COMM_TASK_COLLECTION_INFO_UPDATE, NULL, 0);
		}
	}
#endif
}
/*******************************************************************************
* ��    �ƣ� nmb_rx_data_parser
* ��    �ܣ� 
* ��ڲ����\
* ���ڲ����� ��
* ��  ���ߣ� fourth.peng.
* �������ڣ� 2017-4-28
* ��    �ģ�
* �޸����ڣ�
* ��    ע��
*******************************************************************************/
void nmb_rx_data_parser(uint8 *rx_data, uint16 len)
{
	uint8   parse_len =0;
	uint8   index = 0;
	uint8   i;
	uint8   *p_frame_start = rx_data;
	uint8   parse_data[50] = {0};

#ifdef SAMPLE_BOARD
	ST_SAMPLE_CMD_STR cmd_str = {0};
#else
	ST_COLLECTOR_RESPONCE collector_res = {0};
#endif
	for (i = 0; i< len; i++)
	{
		//���յ�֡�����ַ�
		if (0x7e == rx_data[i])
		{
			parse_len = nmb_frame_translate(p_frame_start, parse_data,index);
			//checksum У��
			if(verify_checksum_of_nmb_frame(parse_data, parse_len))
			{
#ifdef SAMPLE_BOARD
				memcpy(&cmd_str, (const void *)parse_data, sizeof(cmd_str));
				comm_cmd_str_analyze((const ST_SAMPLE_CMD_STR*)&cmd_str);
#else
				memcpy(&collector_res, (const void *)parse_data, sizeof(collector_res));
				collector_responce_analyze((const ST_COLLECTOR_RESPONCE *)&collector_res);
#endif
			}
			//���ݰ�δ�����꣬����������һ֡
			if (index < len)
				{
				index =0;
				p_frame_start = rx_data + i + 1;
				}
		}
		else
		{
			index++;
		}
	}
}



/*******************************************************************************
 * ��    �ƣ� app_send_collector_update_cmd
 * ��    �ܣ� ���͸��¼����Ϣ������ɼ���
 * ��ڲ����� num - �ɼ�����
 * ���ڲ����� ��
 * ��    �ߣ� fourth.peng
 * �������ڣ� 2017-04-27
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void app_send_collector_info_to_main_unit(void *data, uint16 len)
{
	uint8 send_buf[100] ={0};
	uint8 length = sizeof(collector_responce) -1;
	memcpy(send_buf, (const void *)&collector_responce, length);
	send_buf[1] = Ctrl.sample_id;
	nmb_send_data(Ctrl.Com.pch, (uint8 *)&send_buf, length);
}
/*******************************************************************************
 * ��    �ƣ� app_send_collector_update_cmd
 * ��    �ܣ� ���͸��¼����Ϣ������ɼ���
 * ��ڲ����� num - �ɼ�����
 * ���ڲ����� ��
 * ��    �ߣ� fourth.peng
 * �������ڣ� 2017-05-15
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void app_update_detect_info(void *data, uint16 len)
{
	memcpy(&collector_responce.collector_info, data, sizeof(ST_COLLECTOR_INFO));
}


/*******************************************************************************
 * ��    �ƣ� app_send_collector_update_cmd
 * ��    �ܣ� ���͸��¼����Ϣ������ɼ���
 * ��ڲ����� num - �ɼ�����
 * ���ڲ����� ��
 * ��    �ߣ� fourth.peng
 * �������ڣ� 2017-04-27
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void app_send_collector_update_cmd(uint8 num)
{
	uint8 send_buf[10] ={0};
	uint8 len = sizeof(get_collector_board_info) ;
	memcpy(send_buf, (const void *)&get_collector_board_info, len);
	send_buf[1] = 0;
	nmb_send_data(Ctrl.Com.pch, (uint8 *)&send_buf, len);
	
#ifndef SAMPLE_BOARD
	OS_ERR err;
	OSTmrStart(&collector_timerout_tmr, &err);
#endif
}

/*******************************************************************************
 * ��    �ƣ� app_start_collection
 * ��    �ܣ� comm task�¼����մ�����
 * ��ڲ����� 
 * ���ڲ����� ��
 * ��    �ߣ� fourth.peng
 * �������ڣ� 2017-04-27
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void app_start_collection(void *data, uint16 len)
{
	app_send_collector_update_cmd(1);
	collect_step = START_COLLECTION;
//	collect_step = BOARD_A_COLLECTING;
}
/*******************************************************************************
 * ��    �ƣ� app_collection_board_b
 * ��    �ܣ� comm task�¼����մ�����
 * ��ڲ����� 
 * ���ڲ����� ��
 * ��    �ߣ� fourth.peng
 * �������ڣ� 2017-04-27
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void app_collection_board_b(void *data, uint16 len)
{
//	app_send_collector_update_cmd(2);
	collect_step = BOARD_B_COLLECTING;

}
/*******************************************************************************
 * ��    �ƣ� app_collection_board_b
 * ��    �ܣ� comm task�¼����մ�����
 * ��ڲ����� 
 * ���ڲ����� ��
 * ��    �ߣ� fourth.peng
 * �������ڣ� 2017-04-27
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void app_collection_board_c(void *data, uint16 len)
{
//	app_send_collector_update_cmd(3);
	collect_step = BOARD_C_COLLECTING;

}
/*******************************************************************************
 * ��    �ƣ� app_comm_time_tick
 * ��    �ܣ� comm task�¼����մ�����
 * ��ڲ����� 
 * ���ڲ����� ��
 * ��    �ߣ� fourth.peng
 * �������ڣ� 2017-04-27
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void app_comm_time_tick(void *data, uint16 len)
{
	static ST_QUEUETCB queue = {0};
	static uint8 collection_start_cnt =0;
	OS_ERR err;

	collection_start_cnt++;	
	if(collection_start_cnt>=5)
	{
		collection_start_cnt = 0;
	}
	
	
		

}
/*******************************************************************************
 * ��    �ƣ� app_comm_rx_collector_info
 * ��    �ܣ� comm task�¼����մ�����
 * ��ڲ����� 
 * ���ڲ����� ��
 * ��    �ߣ� fourth.peng
 * �������ڣ� 2017-04-27
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void app_sample_tmr_callback(OS_TMR *ptmr,void *p_arg)
{
	system_send_msg(COMM_TASK_ID, COMM_TASK_START_COLLECTION, NULL, 0);
}

/*******************************************************************************
 * ��    �ƣ� app_comm_rx_collector_info
 * ��    �ܣ� comm task�¼����մ�����
 * ��ڲ����� 
 * ���ڲ����� ��
 * ��    �ߣ� fourth.peng
 * �������ڣ� 2017-04-27
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void app_create_sample_tmr(void *data, uint16 len)
{
	uint16 period_time;
	OS_ERR err = 0;
	
	memcpy(&period_time, data, sizeof(period_time));
#ifndef SAMPLE_BOARD
	OSTmrCreate(&app_sample_tmr,
					(CPU_CHAR *)"app_comm_tmr",
					period_time, 
					period_time, 
					OS_OPT_TMR_PERIODIC, 
					(OS_TMR_CALLBACK_PTR)app_sample_tmr_callback, 
					NULL, 
					&err);
	if(OS_ERR_NONE != err)
	{
	}

	OSTmrStart(&app_sample_tmr, &err);
#endif
	if(OS_ERR_NONE != err)
	{
	}
}


/*******************************************************************************
 * ��    �ƣ� app_comm_rx_collector_info
 * ��    �ܣ� comm task�¼����մ�����
 * ��ڲ����� 
 * ���ڲ����� ��
 * ��    �ߣ� fourth.peng
 * �������ڣ� 2017-04-27
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void app_comm_rx_collector_info(void *data, uint16 len)
{
	uint8 *rx_buf = data;
	nmb_rx_data_parser(rx_buf, len);
}

/*******************************************************************************
 * ��    �ƣ� app_comm_collector_data_parser
 * ��    �ܣ��Բɼ������ݽ��м��㲢����
 * ��ڲ����� 
 * ���ڲ����� ��
 * ��    �ߣ� fourth.peng
 * �������ڣ� 2017-04-28
 * ��    �ļ�¼��
 2017-5-2	fourth		���ӹ�����Ϣ�Լ�DTCˢ��
 *******************************************************************************/
void app_comm_collector_data_parser(void *data, uint16 len)
{
	int32 average_steps = 0;
	int32 sum_steps = 0;
	uint8 err = 0;
	uint8 err_ch = 0;
	uint8 valid_cnt = 0;
	float speed = 0;
	ST_MONITOR *p_monitor = &Mater.monitor;
	uint8 i;

	for(i = 0; i <3; i++)
	{
		if ( FALSE == p_monitor->abnormal_info[i].connected_fail )/*�Ƿ���յ��ɼ���Ϣ*/
		{
			if ( 0 == p_monitor->collector_board[i].status ) /*�ɼ��幤������*/
			{
				sum_steps += p_monitor->collector_board[i].step_counts;
				valid_cnt++;
			}
			/*���¹�����Ϣ*/
			app_abnormal_info_update(&p_monitor->collector_board[i], &p_monitor->abnormal_info[i]);
		}
	}
	/*������ˢ��*/
	app_dtc_update(p_monitor->abnormal_info);
	
	average_steps = sum_steps/valid_cnt;

/*
	for (i = 0; i < 3; i++)
	{
		if ((p_monitor->collector_board[i].step_counts > (average_steps * 1.001)) 
			|| (p_monitor->collector_board[i].step_counts < (average_steps * 0.999)))
		{
		//��ƽ��ֵ������1/1000 ����Ϊ�����쳣
			err = 1;
			break;
		}
	}

	if(err)
	{
		int32 diff[3] = {0};
		diff[0] = abs(p_monitor->collector_board[0].step_counts - average_steps);
		diff[1] = abs(p_monitor->collector_board[1].step_counts - average_steps);
		diff[2] = abs(p_monitor->collector_board[2].step_counts - average_steps);
		err_ch = ((diff[0] > diff[1])&&(diff[0]>diff[2]))? 0:(((diff[1] > diff[0])&&(diff[1] > diff[2]))? 1:2);

		average_steps = (sum_steps - p_monitor->collector_board[err_ch].step_counts)>>1;
	}
*/
	
	speed = (float)average_steps*system_para.speed_factor;
	
	Mater.monitor.real_speed = speed;
}

/* comm task �����¼���Ӧ�ص��������*/
const ST_TASK_EVENT_TBL_TYPE app_comm_task_event_table[]=
{
{COMM_TASK_TIME_TICK,					app_comm_time_tick					},
{COMM_TASK_START_COLLECTION,			app_start_collection					},
{ COMM_TASK_COLLECT_BOARD_B,			app_collection_board_b				},
{ COMM_TASK_COLLECT_BOARD_C,			app_collection_board_c				},
{ COMM_TASK_COLLECT_COMPLETE,			app_comm_collector_data_parser		},
{COMM_TASK_RX_COLLECTOR_INFO,			app_comm_rx_collector_info			},
{COMM_TASK_CREATE_SAMPLE_TMR,			app_create_sample_tmr				},
{COMM_TASK_COLLECTION_INFO_UPDATE,		app_send_collector_info_to_main_unit	},
{COMM_TASK_UPDATE_DETECT_INFO,			app_update_detect_info				},

};


/*******************************************************************************
 * ��    �ƣ� app_comm_task_event
 * ��    �ܣ� comm task�¼�������ں���
 * ��ڲ����� 
 * ���ڲ����� ��
 * ��    �ߣ� fourth.peng
 * �������ڣ� 2017-04-27
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void app_comm_task_event(ST_QUEUE *queue)//uint16 event, ST_QUEUEDATA *data)
{
	uint8 i = 0;
	TASK_EVENT_CALLBACK p_fn = NULL;
	for(i = 0; i < ((sizeof(app_comm_task_event_table))/(sizeof(ST_TASK_EVENT_TBL_TYPE))); i++)
	{
		if(app_comm_task_event_table[i].event == queue->event)
		{
			p_fn = app_comm_task_event_table[i].p_func;
			if (NULL != p_fn)
			{
				p_fn(queue->data, queue->len);
			}
			queue->event = 0;
			queue->data = 0;
			queue->len = 0;
			break;
		}
 	}
}


/*******************************************************************************
 * ��    �ƣ� AppTaskComm
 * ��    �ܣ� ��������
 * ��ڲ����� p_arg - �����񴴽���������
 * ���ڲ����� ��
 * ��    �ߣ� wumingshen.
 * �������ڣ� 2015-02-05
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void  AppTaskComm (void *p_arg)
{
	ST_QUEUE *p_mailbox = 0;
    
/*******************************************************************************
* Description  : ����ͨѶ��������ѯ�¼����������¼������ö�Ӧ�Ļص�����
                 �¼�������MODBUS�ײ�Ķ�ʱ���������С�
* Author       : 2018/1/30 ���ڶ�, by redmorningcn
*******************************************************************************/
	p_mailbox = (ST_QUEUE *)system_get_msg(COMM_TASK_ID);

	if (NULL != p_mailbox)
	{
		app_comm_task_event(p_mailbox);//->event, p_mailbox->data,);
	}
}

/*******************************************************************************
 * ��    �ƣ� APP_ModbusInit
 * ��    �ܣ� MODBUS��ʼ��
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ� ������
 * �������ڣ� 2015-03-28
 * ��    �ģ�
 * �޸����ڣ�
 * ��    ע�� �ó�ʼ���ᴴ��Modbus����
 *******************************************************************************/
void App_ModbusInit(void)
{
    MODBUS_CH   *pch;
    /***********************************************
    * ������ uCModBus��ʼ����RTUʱ��Ƶ��Ϊ1000HZ
    *        ʹ���˶�ʱ��2��TIM2��
    */
    MB_Init(1000);
    // UART1
    /***********************************************
    * ������ ��ModBus����Ϊ�ӻ�����mb_cfg.c��
    *        ������������
    */
#if MODBUS_CFG_SLAVE_EN == DEF_TRUE
    pch         = MB_CfgCh( ModbusNode,             // ... Modbus Node # for this slave channel
                            MODBUS_SLAVE,           // ... This is a SLAVE
                            500,                    // ... 0 when a slave
                            MODBUS_MODE_RTU,        // ... Modbus Mode (_ASCII or _RTU)
                            0,                      // ... Specify UART #1
                            115200,                 // ... Baud Rate
                            USART_WordLength_8b,    // ... Number of data bits 7 or 8
                            USART_Parity_No,        // ... Parity: _NONE, _ODD or _EVEN
                            USART_StopBits_1,       // ... Number of stop bits 1 or 2
                            MODBUS_WR_EN);          // ... Enable (_EN) or disable (_DIS) writes
    pch->AesEn          = DEF_DISABLED;             // ... AES���ܽ�ֹ
    pch->NonModbusEn    = DEF_ENABLED;              // ... ֧�ַ�MODBUSͨ��
    pch->IapModbusEn    = DEF_ENABLED;              // ... ֧��IAP MODBUSͨ��
    
    pch->RxFrameHead    = 0x1028;                   // ... ���ƥ��֡ͷ
    pch->RxFrameTail    = 0x102C;                   // ... ���ƥ��֡β
    
    Ctrl.Com.pch       = pch;
    
    xdev_out(xUSART1_putchar);
    xdev_in(xUSART1_getchar);  
#endif
    
    // UART2
    /***********************************************
    * ������ ��ModBus����Ϊ�ӻ�����mb_cfg.c��
    *        ��չͨѶ���ڣ���˼άͨѶ
    */
#if MODBUS_CFG_SLAVE_EN == DEF_TRUE
    pch         = MB_CfgCh( ModbusNode,             // ... Modbus Node # for this slave channel
                            MODBUS_SLAVE,           // ... This is a SLAVE
                            500,                    // ... 0 when a slave
                            MODBUS_MODE_RTU,        // ... Modbus Mode (_ASCII or _RTU)
                            1,                      // ... Specify UART #2
                            57600,                  // ... Baud Rate
                            USART_WordLength_8b,    // ... Number of data bits 7 or 8
                            USART_Parity_No,        // ... Parity: _NONE, _ODD or _EVEN
                            USART_StopBits_1,       // ... Number of stop bits 1 or 2
                            MODBUS_WR_EN);          // ... Enable (_EN) or disable (_DIS) writes
    pch->AesEn          = DEF_DISABLED;             // ... AES���ܽ�ֹ
    pch->NonModbusEn    = DEF_ENABLED;              // ... ֧�ַ�MODBUSͨ��
    pch->IapModbusEn    = DEF_ENABLED;              // ... ֧��IAP MODBUSͨ��
    
    pch->RxFrameHead    = 0x1028;                   // ... ���ƥ��֡ͷ
    pch->RxFrameTail    = 0x102C;                   // ... ���ƥ��֡β
    
    Ctrl.Dtu.pch        = pch;
#endif


#ifndef DISABLE_UART3_4_FOR_MODBUS
    // UART3
    /***********************************************
    * ������ ��ModBus����Ϊ��������mb_cfg.c��
    *        ����ͨѶ���ڣ��빫˾������
    */
#if MODBUS_CFG_MASTER_EN == DEF_TRUE
    
    pch         = MB_CfgCh( ModbusNode,             // ... Modbus Node # for this slave channel
                            MODBUS_SLAVE,           // ... This is a SLAVE
                            500,                    // ... 0 when a slave
                            MODBUS_MODE_RTU,        // ... Modbus Mode (_ASCII or _RTU)
                            2,                      // ... Specify UART #3
                            57600,                   // ... Baud Rate
                            USART_WordLength_8b,    // ... Number of data bits 7 or 8
                            USART_Parity_No,        // ... Parity: _NONE, _ODD or _EVEN
                            USART_StopBits_1,       // ... Number of stop bits 1 or 2
                            MODBUS_WR_EN);          // ... Enable (_EN) or disable (_DIS) writes
    pch->AesEn          = DEF_DISABLED;             // ... AES���ܽ�ֹ
    pch->NonModbusEn    = DEF_ENABLED;              // ... ֧�ַ�MODBUSͨ��
    pch->IapModbusEn    = DEF_ENABLED;              // ... ֧��IAP MODBUSͨ��
    
    pch->RxFrameHead    = 0x1028;                   // ... ���ƥ��֡ͷ
    pch->RxFrameTail    = 0x102C;                   // ... ���ƥ��֡β
    
    Ctrl.Dtu.pch        = pch;
#endif
    // UART4
    /***********************************************
    * ������ ��ModBus����Ϊ��������mb_cfg.c��
    *        ����ͨѶ���ڣ��빫˾������
    */
#if MODBUS_CFG_SLAVE_EN == DEF_TRUE
    
    pch         = MB_CfgCh( ModbusNode,             // ... Modbus Node # for this slave channel
                            MODBUS_SLAVE,           // ... This is a SLAVE
                            500,                    // ... 0 when a slave
                            MODBUS_MODE_RTU,        // ... Modbus Mode (_ASCII or _RTU)
                            3,                      // ... Specify UART #3
                            19200,                  // ... Baud Rate
                            USART_WordLength_8b,    // ... Number of data bits 7 or 8
                            USART_Parity_No,        // ... Parity: _NONE, _ODD or _EVEN
                            USART_StopBits_1,       // ... Number of stop bits 1 or 2
                            MODBUS_WR_EN);          // ... Enable (_EN) or disable (_DIS) writes
    pch->AesEn          = DEF_DISABLED;             // ... AES���ܽ�ֹ
    pch->NonModbusEn    = DEF_ENABLED;              // ... ֧�ַ�MODBUSͨ��
    pch->IapModbusEn    = DEF_ENABLED;              // ... ֧��IAP MODBUSͨ��
    
    pch->RxFrameHead    = 0x1028;                   // ... ���ƥ��֡ͷ
    pch->RxFrameTail    = 0x102C;                   // ... ���ƥ��֡β
    
    Ctrl.Otr.pch        = pch;
#endif
#endif
    
    Ctrl.Com.SlaveAddr  = 0x84;
}


/*******************************************************************************
 * ��    �ƣ� app_comm_tmr_callback
 * ��    �ܣ� comm task ����ʱ���Ļص�����
 * ��ڲ����� ��
 * ���ڲ����� ��
 * �������ߣ� pengwending
 * �������ڣ� 2017-4-26
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void app_comm_tmr_callback(OS_TMR *ptmr,void *p_arg)
{
	system_send_msg(COMM_TASK_ID, COMM_TASK_TIME_TICK, NULL, 0);
}

/*******************************************************************************
 * ��    �ƣ� app_collector_timeout_tmr_call_back
 * ��    �ܣ� comm task ����ʱ���Ļص�����
 * ��ڲ����� ��
 * ���ڲ����� ��
 * �������ߣ� pengwending
 * �������ڣ� 2017-4-26
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void app_collector_timeout_tmr_call_back(OS_TMR *ptmr,void *p_arg)
{
	BOARD_ABNORMAL_STATUS *p_abnormal_info = Mater.monitor.abnormal_info;

	switch ( collect_step )
	{
		case BOARD_A_COLLECTING:
			collector_lost.board_a_lost_cnt++;
			if(collector_lost.board_a_lost_cnt > 5)
			{
				memset(&Mater.monitor.collector_board[0], 0, sizeof(ST_COLLECTOR_INFO));
				p_abnormal_info[0].connected_fail = TRUE;
			}
			system_send_msg(COMM_TASK_ID, COMM_TASK_COLLECT_BOARD_B, NULL, 0);
			break;
		case BOARD_B_COLLECTING:
			collector_lost.board_b_lost_cnt++;
			if(collector_lost.board_b_lost_cnt > 5)
			{
				memset(&Mater.monitor.collector_board[1], 0, sizeof(ST_COLLECTOR_INFO));
				p_abnormal_info[1].connected_fail = TRUE;
			}
			system_send_msg(COMM_TASK_ID, COMM_TASK_COLLECT_BOARD_C, NULL, 0);
			break;
		case BOARD_C_COLLECTING:
			collector_lost.board_c_lost_cnt++;
			if(collector_lost.board_c_lost_cnt > 5)
			{
				memset(&Mater.monitor.collector_board[2], 0, sizeof(ST_COLLECTOR_INFO));
				p_abnormal_info[2].connected_fail = TRUE;
			}
			
			system_send_msg(COMM_TASK_ID, COMM_TASK_COLLECT_COMPLETE, NULL, 0);
			break;
		case START_COLLECTION:
			collector_lost.board_a_lost_cnt++;
			if(collector_lost.board_a_lost_cnt > 5)
			{
				memset(&Mater.monitor.collector_board[0], 0, sizeof(ST_COLLECTOR_INFO));
				p_abnormal_info[0].connected_fail = TRUE;
			}
			
			collector_lost.board_b_lost_cnt++;
			if(collector_lost.board_b_lost_cnt > 5)
			{
				memset(&Mater.monitor.collector_board[1], 0, sizeof(ST_COLLECTOR_INFO));
				p_abnormal_info[1].connected_fail = TRUE;
			}
			
			collector_lost.board_c_lost_cnt++;
			if(collector_lost.board_c_lost_cnt > 5)
			{
				memset(&Mater.monitor.collector_board[2], 0, sizeof(ST_COLLECTOR_INFO));
				p_abnormal_info[2].connected_fail = TRUE;
			}
			system_send_msg(COMM_TASK_ID, COMM_TASK_COLLECT_COMPLETE, NULL, 0);
			break;
	}

}

/*******************************************************************************
 * ��    �ƣ� APP_CommInit
 * ��    �ܣ� �����ʼ��
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ� wumingshen.
 * �������ڣ� 2015-03-28
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
 void APP_CommInit(void)
{
    OS_ERR err;
    
    /***********************************************
    * ������ �����¼���־��
    */
    OSFlagCreate(( OS_FLAG_GRP  *)&Ctrl.Os.CommEvtFlagGrp,
                 ( CPU_CHAR     *)"App_CommFlag",
                 ( OS_FLAGS      )0,
                 ( OS_ERR       *)&err);
    
    Ctrl.Os.CommEvtFlag = COMM_EVT_FLAG_HEART       // ����������
                        + COMM_EVT_FLAG_RESET       // COMM��λ
                        + COMM_EVT_FLAG_CONNECT     // COMM����
                        + COMM_EVT_FLAG_RECV        // ���ڽ���
                        + COMM_EVT_FLAG_REPORT      // ���ڷ���
                        + COMM_EVT_FLAG_CLOSE       // �Ͽ�
                        + COMM_EVT_FLAG_TIMEOUT     // ��ʱ
                        + COMM_EVT_FLAG_CONFIG      // ����
                        + COMM_EVT_FLAG_IAP_START   // IAP��ʼ
                        + COMM_EVT_FLAG_IAP_END    // IAP����
                        +COMM_EVT_FLAG_TIME_TICK;
        
    /***********************************************
    * ������ ��ʼ��MODBUSͨ��
    */        
    App_ModbusInit();
    
    /***********************************************
    * ������ �ڿ��Ź���־��ע�᱾����Ŀ��Ź���־
    */

//    WdtFlags |= WDT_FLAG_COMM;

	/*����һ������Ϊ20ms�Ķ�ʱ��*/
	OSTmrCreate(&app_comm_tmr,
					(CPU_CHAR *)"app_comm_tmr",
					20, 
					20, 
					OS_OPT_TMR_PERIODIC, 
					(OS_TMR_CALLBACK_PTR)app_comm_tmr_callback, 
					NULL, 
					&err);
	#ifndef SAMPLE_BOARD
	OSTmrCreate(&collector_timerout_tmr,
				(CPU_CHAR *)"collector time out tmr",
				50, 
				50, 
				OS_OPT_TMR_ONE_SHOT, 
				(OS_TMR_CALLBACK_PTR)app_collector_timeout_tmr_call_back, 
				NULL, 
				&err);
	OSTmrStart(&app_comm_tmr, &err);
	#endif
	#if	0	//def DISABLE_MATER_TASK
	ST_QUEUEDATA data;
	uint16 internal_time = 100;
	data.pdata = &internal_time;
	data.len = sizeof(internal_time);
	app_create_sample_tmr(&data);
	#endif

	/*����һ�����У�������Ϣ����*/
	OSQCreate(&COM_RxQ, "comm RxQ", 100, &err);
	
	
}

/*******************************************************************************
 * ��    �ƣ� APP_CommRxDataDealCB
 * ��    �ܣ� �������ݴ���ص���������MB_DATA.C����
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ� ������
 * �������ڣ� 2016-01-04
 * ��    �ģ�
 * �޸����ڣ�
 * ��    ע�� 
 *******************************************************************************/
extern CPU_BOOLEAN APP_CSNC_CommHandler(MODBUS_CH  *pch);
                extern void UpdateRecordPoint(uint8_t storeflag);

INT08U APP_CommRxDataDealCB(MODBUS_CH  *pch)
{
    /***********************************************
    * ������ 2016/01/08���ӣ����ڷ�MODBBUS IAP����ͨ��
    */
#if MB_IAPMODBUS_EN == DEF_ENABLED
    /***********************************************
    * ������ �������������ģʽ
    */
    if ( ( Iap.Status != IAP_STS_DEF ) && 
         ( Iap.Status != IAP_STS_SUCCEED ) &&
         ( Iap.Status != IAP_STS_FAILED ) ) {
        return IAP_CommRxDataDealCB(pch);
    }
#endif
    /***********************************************
    * ������ ��ȡ֡ͷ
    */
//    CPU_SR_ALLOC();
//    CPU_CRITICAL_ENTER();
    uint16_t Len     = pch->RxBufByteCtr;
    memcpy( (INT08U *)&pch->RxFrameData, (INT08U *)pch->RxBuf, Len );
//    CPU_CRITICAL_EXIT();
    
    INT08U  *DataBuf    = pch->RxFrameData;
    /***********************************************
    * ������ �������ݴ���
    */
    u8  DstAddr = 
    CSNC_GetData(	(unsigned char *)pch->RxFrameData,          //RecBuf,
                    (unsigned short )Len,                       //RecLen, 
                    (unsigned char *)DataBuf,                   //DataBuf,
                    (unsigned short *)&Len);                    //InfoLen)

    /***********************************************
    * ������ �����ַΪ0���򲻴���
    */
    if ( DstAddr == 0 )
        return FALSE;
    
    /***********************************************
    * ������ ��ȡ֡ͷ
    */
//    OS_CRITICAL_ENTER();
    memcpy( (INT08U *)&Ctrl.Com.Rd.Head, (INT08U *)pch->RxFrameData, 8 );
//    OS_CRITICAL_EXIT();

    /***********************************************
    * ������ ��ַ�ȶԣ����Ǳ�����ַ��ֱ�ӷ���
    */
    if ( Ctrl.Com.Rd.Head.DstAddr != Ctrl.Com.SlaveAddr ) {
        /***********************************************
        * ������ ת������
        */
        //NMB_Tx(Ctrl.Otr.pch,
        //       (CPU_INT08U  *)pch->RxBuf,
        //       (CPU_INT16U   )pch->RxBufByteCtr); 
        return FALSE;
    } 
    
    /***********************************************
    * ������ �������ָ��
    */
    if ( ( Len == 18) && 
         ( 0 >= memcmp((const char *)"IAP_pragram start!",(const char *)&pch->RxFrameData[8], 18) ) ) {
#if defined     (IMAGE_A) || defined   (IMAGE_B)
             
#else
        /***********************************************
        * ������ �������ݴ���
        */
        CSNC_SendData(	(MODBUS_CH      *)Ctrl.Com.pch,
                        (unsigned char  ) Ctrl.Com.SlaveAddr,         // SourceAddr,
                        (unsigned char  ) Ctrl.Com.Rd.Head.SrcAddr,   // DistAddr,
                        (unsigned char *)&pch->RxFrameData[8],         // DataBuf,
                        (unsigned int	 ) Len);                        // DataLen 
#endif
        IAP_Reset();
        Iap.FrameIdx    = 0;
        return TRUE;
    }    

    /***********************************************
    * ������ ������ݳ��Ȼ��ߵ�ַΪ0���򲻴���
    */
    if ( Len == 0 ) {
        if ( pch->PortNbr == Ctrl.Dtu.pch->PortNbr ) {
            if ( GetRecvFrameNbr() == Ctrl.Com.Rd.Head.PacketSn ) {
                /***********************************************
                * ������ �����ʱ�ϴ���־
                */
                SetSendFrameNbr();
#ifndef SAMPLE_BOARD
                UpdateRecordPoint(1);
#endif
                pch->StatNoRespCtr  = 0;
                
#ifndef SAMPLE_BOARD
                osal_set_event( OS_TASK_ID_TMR, OS_EVT_TMR_MIN);
#endif
                
                /***********************************************
                * ������ ���ڽ���COMMģ�����Ϣ������
                */
                Ctrl.Com.ConnectTimeOut    = 0;                // ��ʱ����������
                Ctrl.Com.ConnectFlag       = TRUE;             // ת���ӳɹ���־
                return TRUE;
            }
        }
        return FALSE;
    }
    /***********************************************
    * ������ ��ȡ����
    */
    if( Len > 256 )
        return FALSE;
    
//    OS_CRITICAL_ENTER();
//    memcpy( (INT08U *)&Ctrl.Com.Rd.Data, (INT08U *)&pch->RxFrameData[8], Len );
//    OS_CRITICAL_EXIT();
    
    /***********************************************
    * ������ ��������A
    */
    if ( ( Ctrl.Com.Rd.Head.PacketCtrl & 0x0f ) == 0x04 ) {
        
    /***********************************************
    * ������ ��������B
    */
    } else if ( ( Ctrl.Com.Rd.Head.PacketCtrl & 0x0f ) == 0x03 ) {
        
    /***********************************************
    * ������ ������ȡ
    */
    } else if ( ( Ctrl.Com.Rd.Head.PacketCtrl & 0x0f ) == 0x02 ) {
        
    /***********************************************
    * ������ ��������
    */
    } else if ( ( Ctrl.Com.Rd.Head.PacketCtrl & 0x0f ) == 0x01 ) {
        
    /***********************************************
    * ������ ������
    */
    } else if ( ( Ctrl.Com.Rd.Head.PacketCtrl & 0x0f ) == 0x00  ) {        
        return TRUE;//APP_CSNC_CommHandler(pch);
    }
exit:
    /***********************************************
    * ������ ���ڽ���COMMģ�����Ϣ������
    */
    Ctrl.Com.ConnectTimeOut    = 0;                // ��ʱ����������
    Ctrl.Com.ConnectFlag       = TRUE;             // ת���ӳɹ���־
    
    return TRUE;
}

void app_comm_speed_collection_rx_data(MODBUS_CH *pch)
{
	uint8 rx_buf[100]={0};
	uint8 i;
	uint8 checksum = 0;
	ST_COLLECTOR_INFO collector_info = {0};

	uint8 len = pch->RxBufByteCtr-1;

	memcpy (rx_buf, pch->RxBuf, len);
	len -= 1;
	for (i =0; i < len; i++)
	{
		checksum += rx_buf[i];
	}

	if(checksum == rx_buf[len])
	{
		memcpy((void *)&collector_info, &rx_buf[5], sizeof(collector_info));
	}
	
}

/***********************************************
* ������ 2016/01/08���ӣ����ڷ�MODBBUS IAP����ͨ��
*/
#if MB_IAPMODBUS_EN == DEF_ENABLED
/*******************************************************************************
 * ��    �ƣ� APP_CommRxDataDealCB
 * ��    �ܣ� �������ݴ���ص���������MB_DATA.C����
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ� ������
 * �������ڣ� 2016-01-04
 * ��    �ģ�
 * �޸����ڣ�
 * ��    ע�� 
 *******************************************************************************/
INT08U IAP_CommRxDataDealCB(MODBUS_CH  *pch)
{
//    CPU_SR_ALLOC();
//    CPU_CRITICAL_ENTER();
    u8  Len     = pch->RxBufByteCtr;
    memcpy( (INT08U *)&pch->RxFrameData, (INT08U *)pch->RxBuf, Len );
//    CPU_CRITICAL_EXIT();
    
    /***********************************************
    * ������ ��ȡ֡ͷ
    */    
    INT08U  *DataBuf    = pch->RxFrameData;
    
    /***********************************************
    * ������ �������ݴ���
    */
    u8  DstAddr = 
    CSNC_GetData(	(unsigned char *)pch->RxFrameData,          //RecBuf,
                    (unsigned char	 )Len,                       //RecLen, 
                    (unsigned char *)DataBuf,                   //DataBuf,
                    (unsigned short *)&Len);                    //InfoLen)
    /***********************************************
    * ������ �����ս���
    */
    if ( ( Len == 16) && 
         ( 0 >= memcmp((const char *)"IAP_pragram end!",(const char *)&pch->RxFrameData[8], 16) ) ) {
        /***********************************************
        * ������ �������ݴ���
        */
        CSNC_SendData(	(MODBUS_CH      *)Ctrl.Com.pch,
                        (unsigned char  ) Ctrl.Com.SlaveAddr,          // SourceAddr,
                        (unsigned char  ) Ctrl.Com.Rd.Head.SrcAddr,    // DistAddr,
                        (unsigned char *)&pch->RxFrameData[8],          // DataBuf,
                        (unsigned short ) Len); 
        /***********************************************
        * ������ ��λIAP������־λ
        */
/*
        OS_ERR err;
        OS_FlagPost(( OS_FLAG_GRP *)&Ctrl.Os.CommEvtFlagGrp,
                    ( OS_FLAGS     ) COMM_EVT_FLAG_IAP_END,
                    ( OS_OPT       ) OS_OPT_POST_FLAG_SET,
                    ( CPU_TS       ) 0,
                    ( OS_ERR      *) &err);
*/        
        return TRUE;
    }
    
    if ( ( Len == 18) && 
         ( 0 >= memcmp((const char *)"IAP_pragram start!",(const char *)&pch->RxFrameData[8], 18) ) ) {
        
#if defined     (IMAGE_A) || defined   (IMAGE_B)
#else
        /***********************************************
        * ������ �������ݴ���
        */
        CSNC_SendData(	(MODBUS_CH      *)Ctrl.Com.pch,
                        (unsigned char  ) Ctrl.Com.SlaveAddr,          // SourceAddr,
                        (unsigned char  ) Ctrl.Com.Rd.Head.SrcAddr,    // DistAddr,
                        (unsigned char *)&pch->RxFrameData[8],         // DataBuf,
                        (unsigned int	 ) Len);                        // DataLen 
#endif
        IAP_Restart();
        Iap.FrameIdx    = 0;
        return TRUE;
    }
    
    if ( Iap.Status < IAP_STS_START )
        return TRUE;
    /***********************************************
    * ������ д����
    */
    /***********************************************
    * ������ �����ݴ�������ͽṹ
    */
    Ctrl.Com.Wr.Head.DataLen       = 0;
    /***********************************************
    * ������ �༭Ӧ������
    */
    char str[20];
    usprintf(str,"\n%d",Iap.FrameIdx);
    str[19]  = 0;
    /***********************************************
    * ������ д���ݵ�Flash
    */
    IAP_Program((StrIapState *)&Iap, (INT16U *)&pch->RxFrameData[8], Len, (INT16U )GetRecvFrameNbr() );
    /***********************************************
    * ������ �������ݴ���
    */
    CSNC_SendData(	(MODBUS_CH     *) Ctrl.Com.pch,
                    (unsigned char  ) Ctrl.Com.SlaveAddr,                    // SourceAddr,
                    (unsigned char  ) Ctrl.Com.Rd.Head.SrcAddr,              // DistAddr,
                    (unsigned char *) str,                                    // DataBuf,
                    (unsigned short ) strlen(str));                           // DataLen 
    if ( Len < 128 ) {
        /***********************************************
        * ������ ��λIAP������־λ
        */
/*        OS_ERR err;
        OS_FlagPost(( OS_FLAG_GRP *)&Ctrl.Os.CommEvtFlagGrp,
                    ( OS_FLAGS     ) COMM_EVT_FLAG_IAP_END,
                    ( OS_OPT       ) OS_OPT_POST_FLAG_SET,
                    ( CPU_TS       ) 0,
                    ( OS_ERR      *) &err);*/
    } else {
        IAP_Programing();                               // ��λ��ʱ������
    }
        
    /***********************************************
    * ������ ���ڽ���COMMģ�����Ϣ������
    */
    Ctrl.Com.ConnectTimeOut    = 0;                // ��ʱ����������
    Ctrl.Com.ConnectFlag       = TRUE;             // ת���ӳɹ���־
    
    return TRUE;
}
#endif

/*******************************************************************************
 * ��    �ƣ� NMBS_FCxx_Handler
 * ��    �ܣ� ��MODBUS�������ݴ���ص���������mbs_core.d����
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ� ������
 * �������ڣ� 2017-02-03
 * ��    �ģ�
 * �޸����ڣ�
 * ��    ע�� 
 *******************************************************************************/
extern OS_Q                MTR_RxQ;
extern OS_Q                COM_RxQ;
extern OS_Q                DTU_RxQ;
CPU_BOOLEAN  NMBS_FCxx_Handler (MODBUS_CH  *pch)
{
	static ST_QUEUETCB post_queue = {0};
    OS_ERR      err;
    CPU_INT16U  head    = BUILD_INT16U(pch->RxBuf[1], pch->RxBuf[0]);
    CPU_INT16U  tail    = BUILD_INT16U(pch->RxBuf[pch->RxBufByteCtr-1],
                                       pch->RxBuf[pch->RxBufByteCtr-2]);
    /***********************************************
    * ������ ��ɳ�ϳ�DTUЭ�鴦��
    */
    if ( ( pch->RxFrameHead == head ) &&
         ( pch->RxFrameTail == tail ) ) {
        APP_CommRxDataDealCB(pch);
        /***********************************************
        * ������ ������Э��Э��,���Э��Э�鴦����
        */
    } else if ( ( 0xAA55 == head ) || ( 0xAAAA == head ) ) {	//�˷�֧û��У�� ���Ķ�
        OS_ERR  err;
        
//        CPU_SR_ALLOC();
//        OS_CRITICAL_ENTER();
        memcpy( (INT08U *)&pch->RxFrameData, (INT08U *)pch->RxBuf, pch->RxBufByteCnt );
//        OS_CRITICAL_EXIT();
        
#ifndef SAMPLE_BOARD
/*        (void)OSQPost((OS_Q         *)&MTR_RxQ,
                      (void         *) pch,
                      (OS_MSG_SIZE   ) pch->RxBufByteCtr,
                      (OS_OPT        ) OS_OPT_POST_FIFO,
                      (OS_ERR       *)&err);
         BSP_OS_TimeDly(5);		//�˴�Ϊʲôdelay? ���Ķ�*/
	system_send_msg(MATER_TASK_ID, MATER_TASK_RX_NMB_DATA, pch, pch->RxBufByteCtr);
#endif
         /***********************************************
         * ������ ���Э��Э�鴦��
         */  
    }
	else if(0x7e == (0xff&tail))
     	{
//        CPU_SR_ALLOC();
//        OS_CRITICAL_ENTER();
        memcpy( (INT08U *)&pch->RxFrameData, (INT08U *)pch->RxBuf, pch->RxBufByteCnt );
//        OS_CRITICAL_EXIT();

/*
	post_queue.event = COMM_TASK_RX_COLLECTOR_INFO;
	post_queue.queue_data.pdata = pch->RxFrameData;
	post_queue.queue_data.len = pch->RxBufByteCtr;
        (void)OSQPost((OS_Q         *)&COM_RxQ,
                      (void         *) &post_queue,
                      (OS_MSG_SIZE   ) sizeof(post_queue),
                      (OS_OPT        ) OS_OPT_POST_FIFO,
                      (OS_ERR       *)&err);
*/
	system_send_msg(COMM_TASK_ID, COMM_TASK_RX_COLLECTOR_INFO, pch->RxFrameData, pch->RxBufByteCtr);
//		app_comm_speed_collection_rx_data(pch);
     	}
	else {
        return DEF_FALSE;
    }
    return DEF_TRUE;
}

/*******************************************************************************
 * ��    �ƣ� dtc_init
 * ��    �ܣ� �������ʼ��Ϊ�޹���
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ� fourth.peng
 * �������ڣ� 2017-05-2
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
static void dtc_init(void)
{
	uint8 i;

	memset((void *)&dtc, NO_FAULT, sizeof(dtc));

}
/*******************************************************************************
 * ��    �ƣ� app_abnormal_info_update
 * ��    �ܣ� ˢ�¹�����
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ� fourth.peng
 * �������ڣ� 2017-05-2
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
 void app_abnormal_info_update(ST_COLLECTOR_INFO *p_collector_info, BOARD_ABNORMAL_STATUS *p_abnormal_info)
{
	/*��λ��С��45������135����Ϊ��λ���쳣*/
	const ST_COLLECTOR_THRESHOLD *p_threshold = &system_para.collector_threshold;
	if (( p_threshold->phase_threshold_low > p_collector_info->phase_diff )
		|| (p_threshold->phase_threshold_hig < p_collector_info->phase_diff))
	{
		p_abnormal_info->phase_diff_abnomal = TRUE;
	}
	else
	{
		p_abnormal_info->phase_diff_abnomal = FALSE;
	}
	
	/*ռ�ձ�С��30%���ߴ���%70*/
	if (( p_threshold->duty_threshold_low > p_collector_info->channel[0].duty_ratio ) || ( p_threshold->phase_threshold_hig < p_collector_info->channel[0].duty_ratio))
	{
		p_abnormal_info->channel[0].dtc_flag.duty_ratio = 1;
	}
	else
	{
		p_abnormal_info->channel[0].dtc_flag.duty_ratio = 0;
	}
	if (( p_threshold->duty_threshold_low > p_collector_info->channel[1].duty_ratio ) || ( p_threshold->phase_threshold_hig < p_collector_info->channel[1].duty_ratio))
	{
		p_abnormal_info->channel[1].dtc_flag.duty_ratio = 1;
	}
	else
	{
		p_abnormal_info->channel[1].dtc_flag.duty_ratio = 0;
	}
	
	/*����ʱ�����4.2us*/
	if ( p_threshold->edge_time_threshold < p_collector_info->channel[0].rise_time)
	{
		p_abnormal_info->channel[0].dtc_flag.rise_time = 1;
	}
	else
	{
		p_abnormal_info->channel[0].dtc_flag.rise_time = 0;
	}
	if ( p_threshold->edge_time_threshold < p_collector_info->channel[0].fall_time)
	{
		p_abnormal_info->channel[0].dtc_flag.fall_time = 1;
	}
	else
	{
		p_abnormal_info->channel[0].dtc_flag.fall_time = 0;
	}
	if ( p_threshold->edge_time_threshold < p_collector_info->channel[1].rise_time)
	{
		p_abnormal_info->channel[1].dtc_flag.rise_time = 1;
	}
	else
	{
		p_abnormal_info->channel[1].dtc_flag.rise_time = 0;
	}
	if ( p_threshold->edge_time_threshold < p_collector_info->channel[1].fall_time)
	{
		p_abnormal_info->channel[1].dtc_flag.fall_time = 1;
	}
	else
	{
		p_abnormal_info->channel[1].dtc_flag.fall_time = 0;
	}
	/*������ѹ*/
	if ((p_threshold->sensor_vcc_threshold_low > p_collector_info->channel[0].vcc_vol)
		||(p_threshold->sensor_vcc_threshold_hig < p_collector_info->channel[0].vcc_vol))
	{
		p_abnormal_info->channel[0].dtc_flag.vcc_vol = 1;
	}
	else
	{
		p_abnormal_info->channel[0].dtc_flag.vcc_vol = 0;
	}
	
	if ((p_threshold->sensor_vcc_threshold_low > p_collector_info->channel[1].vcc_vol)
		||(p_threshold->sensor_vcc_threshold_hig < p_collector_info->channel[1].vcc_vol))
	{
		p_abnormal_info->channel[1].dtc_flag.vcc_vol = 1;
	}
	else
	{
		p_abnormal_info->channel[1].dtc_flag.vcc_vol = 0;
	}
	/*�ߵ͵�ƽ*/
	if ((p_threshold->hig_level_threshold_factor *p_collector_info->channel[0].vcc_vol / 100) > p_collector_info->channel[0].high_level_vol)
	{
		p_abnormal_info->channel[0].dtc_flag.high_level = 1;
	}
	else
	{
		p_abnormal_info->channel[0].dtc_flag.high_level = 0;
	}

	if ((p_threshold->low_level_threshold_factor *p_collector_info->channel[0].vcc_vol / 100) < p_collector_info->channel[0].low_level_vol)
	{
		p_abnormal_info->channel[0].dtc_flag.low_level = 1;
	}
	else
	{
		p_abnormal_info->channel[0].dtc_flag.low_level = 0;
	}

	if ((p_threshold->hig_level_threshold_factor *p_collector_info->channel[1].vcc_vol / 100) > p_collector_info->channel[1].high_level_vol)
	{
		p_abnormal_info->channel[1].dtc_flag.high_level = 1;
	}
	else
	{
		p_abnormal_info->channel[1].dtc_flag.high_level = 0;
	}

	if ((p_threshold->low_level_threshold_factor *p_collector_info->channel[1].vcc_vol / 100) < p_collector_info->channel[1].low_level_vol)
	{
		p_abnormal_info->channel[1].dtc_flag.low_level = 1;
	}
	else
	{
		p_abnormal_info->channel[1].dtc_flag.low_level = 0;
	}

	
}
/*******************************************************************************
 * ��    �ƣ� app_dtc_update
 * ��    �ܣ� ˢ�¹�����
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ� fourth.peng
 * �������ڣ� 2017-05-2
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
static void app_dtc_update(BOARD_ABNORMAL_STATUS *p_abnormal_info)
{
	uint8 i;
	uint8 fault_cnt = 0;
	
	for(i = 0; i <3; i++)
	{
		if(0 == p_abnormal_info[i].connected_fail)
		{
			dtc.curr_dtc[COLLECTOR_I_CONNECT_FAIL+i] = NO_FAULT;
			
			if ( TRUE == p_abnormal_info[i].phase_diff_abnomal)
			{
				dtc.curr_dtc[COLLECTOR_I_PHASE_ABNORMAL+i] = FAULT;
			}
			else
			{
				dtc.curr_dtc[COLLECTOR_I_PHASE_ABNORMAL+i] = NO_FAULT;
			}

			if ( p_abnormal_info[i].channel[0].share_byte )
			{
				dtc.curr_dtc[SENSOR_I_IS_ABNORMAL + (2*i)] = FAULT;
			}
			else
			{
				dtc.curr_dtc[SENSOR_I_IS_ABNORMAL + (2*i)] = NO_FAULT;
			}
			
			if ( p_abnormal_info[i].channel[1].share_byte )
			{
				dtc.curr_dtc[SENSOR_I_IS_ABNORMAL +1 + (2*i)] = FAULT;
			}
			else
			{
				dtc.curr_dtc[SENSOR_I_IS_ABNORMAL +1 + (2*i)] = NO_FAULT;
			}		
		}
		else
		{
			dtc.curr_dtc[COLLECTOR_I_CONNECT_FAIL+i] = FAULT;
			dtc.curr_dtc[COLLECTOR_I_PHASE_ABNORMAL+i] = NO_FAULT;
			dtc.curr_dtc[SENSOR_I_IS_ABNORMAL + (2*i)] = NO_FAULT;
			dtc.curr_dtc[SENSOR_I_IS_ABNORMAL +1 + (2*i)] = NO_FAULT;
		}

	}
	
	memset( dtc.fault_list, MAX_DTC_NUM, sizeof(dtc.fault_list) );
	for(i = 0;i < MAX_DTC_NUM; i++)
	{
		if ( FAULT == dtc.curr_dtc[i] )
			dtc.fault_list[fault_cnt++] = i;
	}
	dtc.count = fault_cnt;

	/*���ϴι������Ƿ�һ�£������һ���򱣴浱ǰ����*/
	if(0 != memcmp(dtc.prev_dtc, dtc.curr_dtc, sizeof(dtc.curr_dtc)))
	{
/*		OS_ERR err;
		
	        OS_FlagPost(( OS_FLAG_GRP *)&Ctrl.Os.MaterEvtFlagGrp,
			            ( OS_FLAGS     ) COMM_EVT_SAVE_DATA,
			            ( OS_OPT       ) OS_OPT_POST_FLAG_SET,
			            ( CPU_TS       ) 0,
			            ( OS_ERR      *) &err);
*/
		memcpy(dtc.prev_dtc, dtc.curr_dtc, sizeof(dtc.prev_dtc));
	}
}

/*******************************************************************************
 * 				end of file
 *******************************************************************************/
#endif
