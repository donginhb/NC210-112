/*******************************************************************************
 *   Filename:       app_task_mater.c
 *   Revised:        All copyrights reserved to wumingshen.
 *   Revision:       v1.0
 *   Writer:	     wumingshen.
 *
 *   Description:    ˫��ѡ�� mater �� Ctrl + H, ��ѡ Match the case, Replace with
 *                   ������Ҫ�����֣���� Replace All
 *                   ˫��ѡ�� Mater �� Ctrl + H, ��ѡ Match the case, Replace with
 *                   ������Ҫ�����֣���� Replace All
 *                   ˫��ѡ�� MATER �� Ctrl + H, ��ѡ Match the case, Replace with
 *                   ������Ҫ�����֣���� Replace All
 *                   �� app_cfg.h ��ָ��������� ���ȼ�  �� APP_TASK_MATER_PRIO     ��
 *                                            �� �����ջ�� APP_TASK_MATER_STK_SIZE ����С
 *
 *   Notes:
 *     				 E-mail: shenchangwei945@163.com
 *
 *******************************************************************************/

/*******************************************************************************
 * INCLUDES
 */
#define  SNL_APP_SOURCE
#include <includes.h>
//#include <app_mater_protocol.h>
#include <bsp_flash.h>
#include <iap.h>
     
#include "stm32f10x.h"
#include <stdio.h>
#include "SZ_STM32F107VC_LIB.h"
#include "DS3231.h"
#include "I2C_CLK.h"
#include "Display.h"
#include "DATA_STORAGE.h"
#include "SPI_CS5463_AC.h"
#include "SPI_CS5463_DC.h"
#include "DELAY.h"
#include "RS485.h"
#ifdef PHOTOELECTRIC_VELOCITY_MEASUREMENT
#include <Speed_sensor.h>
#include "app_task_mater.h"
#else
#include <power_macro.h>
#endif
#include "MX25.h"
#include "FM24CL64.h"
#include "string.h"
#include "xprintf.h"
#include "WatchDog.h"
#include <app_comm_protocol.h>
#include <crccheck.h>

#ifdef VSC_INCLUDE_SOURCE_FILE_NAMES
const  CPU_CHAR  *app_task_mater__c = "$Id: $";
#endif
#ifdef SAMPLE_BOARD
#define APP_TASK_MATER_EN     DEF_ENABLED
#else
#define APP_TASK_MATER_EN     DEF_ENABLED
#endif
OS_Q                MTR_RxQ;

#if APP_TASK_MATER_EN == DEF_ENABLED
/*******************************************************************************
 * CONSTANTS
 */
const ST_para_storage default_storage_para =
{
//ST_DISPLAY_PARA
{
	{DEF_ON,	1000},	//time
	{DEF_ON,	1000},	//bat_vol
	{DEF_OFF,	1000},	//speed
	{DEF_ON,	1000},	//phase diff
	{DEF_ON,	1000},	//duty ratio
	{DEF_ON,	1000},	//DTC
	{DEF_ON,	1000},	//History number
},
{
60,		//phase_threshold_low;
120,		//phase_threshold_hig;
30,		//duty_threshold_low;
70,		//duty_threshold_hig;
42,		//4.2us	edge_time_threshold;
1200,	//12V,	sensor_vcc_threshold_low;
3000,	//30v	sensor_vcc_threshold_hig;
90,		//0.9 x VCC	hig_level_threshold_factor;
20,		//0.2 x VCC	low_level_threshold_factor;
},
{
30,		//110 x 0.27 = 30V
40,		//110 X 0.36 = 40V
75,		//110 X 0.68 = 75V
85,		//110 X 0.77 = 85V
125,		//110 X 1.13 = 125V
135,		//110 X 1.22 = 135V
},
	0.5,					//RTC ģ���ص�ѹת������
	91.96,				// 1004.2/4.2/2.6 = 91.96
	0.0212,				// 3.14.159(PI) x 1.5(ֱ��) x 3600(60 x 60) /200(200�����)/4(4����) /1000(KM) = 0.0212
	500,					// 500ms�ɼ�һ��
	0x00,
};
/***********************************************
* ������ ���ݼ�¼��ַ
*
��ַ	����	����	����
AC.PPpower_NUM
0
4
8
AC.NPpower_NUM
12
16
20
��������
32	4		ֱ����ѹƫ��
36	4		ֱ������ƫ��
40	4		������ѹƫ��
44	4		��������ƫ��
48	4	u32	������ѹ����
52	4	u32	������������
56	4	u32	��������
//60	4	u32	���ݵ�ַ
64	4	u32	�豸ID
68	4	U8	���ͳ���
72	4	u32	��¼��ʼ��ַ
76	4	u32	��¼������ַ
80	4	u32	��¼��ˮ��
84	4	u32	��¼ʱ����
88	4	f32	��ѹ����
92	4	f32	��������
96  4   u32 ��ѹ����ֵ
AC.PQpower_NUM
100
104
108
AC.NQpower_NUM
112	4		���й���������
116	4
120	4

//
160 4   f32 ��ѹ����ϵ��
164 4   f32 ��������ϵ��

168	���Ʋ����洢λ��

300	36 ��¼������ˮ����ͷβ����3�Σ�
*/
/*******************************************************************************
 * MACROS
 */
#define CYCLE_TIME_TICKS            (OS_TICKS_PER_SEC * 1)

/*******************************************************************************
 * TYPEDEFS
 */
typedef struct
{
uint32 Head;
uint32 Tail;
uint32 RecordNbr;
}ST_record_number_type;

/***********************************************
* ������ ������ƿ飨TCB��
*/
static  OS_TCB      AppTaskMaterTCB;

/***********************************************
* ������ �����ջ��STACKS��
*/
static  CPU_STK     AppTaskMaterStk[ APP_TASK_MATER_STK_SIZE ];

/*******************************************************************************
 * LOCAL VARIABLES
 */
//ENERGY              AC;
SDAT 	            recordsfr;	
StrMater            Mater;
StrMater            History;

static volatile uint32_t            SysTime = 0;

uint8_t             g_DipDisVal[40];
uint32_t            g_Flash_Adr;

ST_para_storage system_para = {0};
/*******************************************************************************
 * GLOBAL VARIABLES
 */

/*******************************************************************************
 * LOCAL FUNCTIONS
 */
static void     AppTaskMater                (void *p_arg);
static void     APP_MaterInit               (void);

void            DC_COUNTInit                (void);
void            AC_COUNTInit                (void);
INT08U          APP_SevrRxDataDealCB        (MODBUS_CH  *pch);
void            APP_MaterSendMsgToServer    (void);
void            EXTI0_ISRHandler            (void);
void            EXTI9_5_ISRHandler          (void);

CPU_BOOLEAN     APP_CSNC_CommHandler        (MODBUS_CH  *pch);
CPU_BOOLEAN     APP_MaterCommHandler        (MODBUS_CH  *pch);

void            SavePowerData               (void);

/*******************************************************************************
 * GLOBAL FUNCTIONS
 */
void EEP_OS_Init(void);

/*******************************************************************************
 * EXTERN VARIABLES
 */
extern OS_Q                COM_RxQ;

/*******************************************************************************
 * EXTERN FUNCTIONS
 */
extern void     CS5463_AC_Adjust        (MODBUS_CH  *pch,uint8_t mode)	;
extern void     uartprintf              (MODBUS_CH  *pch,const char *fmt, ...);
extern void app_display_step_sequence();
extern void PWR_PVD_Init(void);
extern void app_save_record_number_and_index(void);
/*******************************************************************************/

/*******************************************************************************
 * ��    �ƣ� App_TaskMaterCreate
 * ��    �ܣ� ���񴴽�
 * ��ڲ����� ��
 * ���ڲ����� ��
 * �������ߣ� wumingshen.
 * �������ڣ� 2015-02-05
 * ��    �ģ�
 * �޸����ڣ�
 * ��    ע�� ���񴴽�������Ҫ��app.h�ļ�������
 *******************************************************************************/
void  App_TaskMaterCreate(void)
{
    OS_ERR  err;

    /***********************************************
    * ������ ���񴴽�
    */
    OSTaskCreate((OS_TCB     *)&AppTaskMaterTCB,                    // ������ƿ�  ����ǰ�ļ��ж��壩
                 (CPU_CHAR   *)"App Task Mater",                    // ��������
                 (OS_TASK_PTR ) AppTaskMater,                       // ������ָ�루��ǰ�ļ��ж��壩
                 (void       *) 0,                                  // ����������
                 (OS_PRIO     ) APP_TASK_MATER_PRIO,                // �������ȼ�����ͬ�������ȼ�������ͬ��0 < ���ȼ� < OS_CFG_PRIO_MAX - 2��app_cfg.h�ж��壩
                 (CPU_STK    *)&AppTaskMaterStk[0],                 // ����ջ��
                 (CPU_STK_SIZE) APP_TASK_MATER_STK_SIZE / 10,       // ����ջ�������ֵ
                 (CPU_STK_SIZE) APP_TASK_MATER_STK_SIZE,            // ����ջ��С��CPU���ݿ�� * 8 * size = 4 * 8 * size(�ֽ�)����app_cfg.h�ж��壩
                 (OS_MSG_QTY  ) 0u,                                 // ���Է��͸�����������Ϣ��������
                 (OS_TICK     ) 0u,                                 // ��ͬ���ȼ��������ѭʱ�䣨ms����0ΪĬ��
                 (void       *) 0,                                  // ��һ��ָ����������һ��TCB��չ�û��ṩ�Ĵ洢��λ��
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK |               // �����ջ��������
                                OS_OPT_TASK_STK_CLR),               // ��������ʱ��ջ����
                 (OS_ERR     *)&err);                               // ָ���������ָ�룬���ڴ����������
}
/*******************************************************************************
 * ��    �ƣ� app_read_system_para
 * ��    �ܣ� �������ж�ȡ���ò���
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ� fourth peng
 * �������ڣ� 2017-5-3
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
 #define	SYSTEM_STORAGE_CHECKSUM		0x55
 void app_save_system_para()
{
	uint8 *p_wr = (uint8 *)&system_para;
	uint8 checksum =0;
	uint8 i;
	uint8 len = sizeof(system_para);
	
	for(i = 0; i < (len -1); i++)
	{
		checksum += p_wr[i];
	}
	system_para.checksum = SYSTEM_STORAGE_CHECKSUM - checksum;
	WriteFM24CL64(168, p_wr, len);
}
/*******************************************************************************
 * ��    �ƣ� app_read_recode_number_and_index
 * ��    �ܣ� �������ж�ȡ��¼ͷβ�Լ���ˮ��
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ� fourth peng
 * �������ڣ� 2017-5-5
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void app_read_recode_number_and_index()
{
	ST_record_number_type read_record_buf[3] = {0};
	ReadFM24CL64(300, (uint8_t *)&read_record_buf, 36); 
	//����������ͬ���򿽱���ͬ����Mater�ṹ����
	if ((0 == memcmp((const void *)&read_record_buf[0], (const void *)&read_record_buf[1], 12))
		||(0 == memcmp((const void *)&read_record_buf[0], (const void *)&read_record_buf[2], 12)))
	{
		memcpy((void *)&Mater.Head, (void *)&read_record_buf[0], 12);
	}else if (0 == memcmp((const void *)&read_record_buf[1], (const void *)&read_record_buf[2], 12))
	{
		memcpy((void *)&Mater.Head, (void *)&read_record_buf[1], 12);
	}
	//������¼������ͬ��˵����д�ڶ����ڴ�ʱ�����쳣��������һ���ڴ���������Mater
	else
	{
		memcpy((void *)&Mater.Head, (void *)&read_record_buf[0], 12);
	}
	if ( Mater.Head > MAX_ADDR ) 
	{
		Mater.Head  = 0;
	} 
	else 
	{
		Mater.Head  = Mater.Head / sizeof(Mater);
		Mater.Head  = Mater.Head * sizeof(Mater);
	}
	
	if ( Mater.Tail > MAX_ADDR ) 
	{
		Mater.Tail  = 0;
	}
	else 
	{
		Mater.Tail  = Mater.Tail / sizeof(Mater);
		Mater.Tail  = Mater.Tail * sizeof(Mater);
	}
	
	if ( Mater.RecordNbr == 0xffffffff )
		Mater.RecordNbr  = 0;
#ifndef SAMPLE_BOARD
	app_save_record_number_and_index();
#endif
}
 
/*******************************************************************************
 * ��    �ƣ� app_read_system_para
 * ��    �ܣ� �������ж�ȡ���ò���
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ� fourth peng
 * �������ڣ� 2017-5-3
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void app_read_system_para()
{
	ST_para_storage read_para = {0};
	uint8 i, chksum =0;
	uint8 *p_data = (uint8 *)&read_para;
	static ST_QUEUETCB queue = {0};
	OS_ERR err = 0;

	ReadFM24CL64(168 , p_data, sizeof(read_para));
	for (i = 0; i< (sizeof(ST_para_storage)); i++)
	{
		chksum += p_data[i];
	}

	if(SYSTEM_STORAGE_CHECKSUM == chksum )
	{
		
		memcpy((void *)&system_para, p_data, sizeof(system_para));
	}
	else
	{
		memcpy((void *)&system_para, (void *)&default_storage_para, sizeof(system_para));
		app_save_system_para();
	}
	
	app_display_step_sequence();	// get the display sequence.

	queue.event = COMM_TASK_CREATE_SAMPLE_TMR;
	queue.queue_data.pdata = &system_para.sample_cycle;
	queue.queue_data.len = sizeof(system_para.sample_cycle);
//	OSQPost(&COM_RxQ, &queue, sizeof(queue), OS_OPT_POST_FIFO, &err);	
	
	/***********************************************
	* ������ ��ȡ�����ͺţ�������
	*/     
	ReadFM24CL64(68 , (uint8_t *)&Mater.LocoTyp, 2);
	ReadFM24CL64(70 , (uint8_t *)&Mater.LocoNbr, 2);
	ReadFM24CL64(64 , (uint8_t *)&Ctrl.ID, 4);
	/***********************************************
	* ������ ��¼ʱ����
	*/  
	ReadFM24CL64(84 , (uint8_t *)&Mater.RecordTime, 4);
	if ( Mater.RecordTime < OS_TICKS_PER_SEC * 10 )
		Mater.RecordTime    = OS_TICKS_PER_SEC * 60;
	else if ( Mater.RecordTime > OS_TICKS_PER_SEC * 300 )
		Mater.RecordTime    = OS_TICKS_PER_SEC * 60;

/*
	ReadFM24CL64(72, (uint8_t *)&Mater.Head, 4); 
	ReadFM24CL64(76, (uint8_t *)&Mater.Tail, 4);
	ReadFM24CL64(80, (uint8_t *)&Mater.RecordNbr, 4);
*/
	app_read_recode_number_and_index();
	

}

/*******************************************************************************
 * ��    �ƣ� AppTaskMater
 * ��    �ܣ� ��������
 * ��ڲ����� p_arg - �����񴴽���������
 * ���ڲ����� ��
 * ��    �ߣ� wumingshen.
 * �������ڣ� 2015-02-05
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
extern void  App_SaveDataToHistory( void );

static  void  AppTaskMater (void *p_arg)
{
	OS_ERR      err;
	OS_TICK     dly     = CYCLE_TIME_TICKS;
	OS_TICK     ticks;
        
	/***********************************************
	* ������ �����¼���־��
	*/
	OSFlagCreate(( OS_FLAG_GRP  *)&Ctrl.Os.MaterEvtFlagGrp,
	             ( CPU_CHAR     *)"App_CommFlag",
	             ( OS_FLAGS      )0,
	             ( OS_ERR       *)&err);

	Ctrl.Os.MaterEvtFlag= COMM_EVT_FLAG_HEART       // ����������
	                    + COMM_EVT_FLAG_RESET       // COMM��λ
	                    + COMM_EVT_FLAG_CONNECT     // COMM����
	                    + COMM_EVT_FLAG_RECV        // ���ڽ���
	                    + COMM_EVT_FLAG_REPORT      // ���ڷ���
	                    + COMM_EVT_FLAG_CLOSE       // �Ͽ�
	                    + COMM_EVT_FLAG_TIMEOUT     // ��ʱ
	                    + COMM_EVT_SAVE_DATA
	                    + COMM_EVT_FLAG_CONFIG;     // ����

	OSQCreate ( (OS_Q        *)&MTR_RxQ,
	            (CPU_CHAR    *)"RxQ",
	            (OS_MSG_QTY   ) OS_CFG_INT_Q_SIZE,
	            (OS_ERR      *)&err);
        
	/***********************************************
	* ��ʼ��
	*/   
	APP_MaterInit();


    BSP_OS_TimeDly(OS_TICKS_PER_SEC / 2);
    /***********************************************
    * ������ 
    */ 
	uartprintf(Ctrl.Com.pch,"\r\n��ǰͷ��ַ��0x%08X\r\n��ǰβ��ַ��0x%08X\r\n��ǰ��¼�ţ�%d",Mater.Head,Mater.Tail,Mater.RecordNbr);
                
    /***********************************************
    * ������ Task body, always written as an infinite loop.
    */
    while (DEF_TRUE) {
        /***********************************************
        * ������ �������Ź���־��λ
        */
        OS_ERR      terr;
        ticks       = OSTimeGet(&terr);                 // ��ȡ��ǰOSTick
        OS_FlagPost(( OS_FLAG_GRP *)&WdtFlagGRP,
                    ( OS_FLAGS     ) WDT_FLAG_MATER,
                    ( OS_OPT       ) OS_OPT_POST_FLAG_SET,
                    ( CPU_TS       ) 0,
                    ( OS_ERR      *) &err);
        
        /***********************************************
        * ������ �ȴ�DTU���ݽ�����Ϣ����
        */
        OS_MSG_SIZE p_msg_size;
        
        MODBUS_CH *pch = 
       (MODBUS_CH *)OSQPend ((OS_Q*)&MTR_RxQ,
                    (OS_TICK       )dly,
                    (OS_OPT        )OS_OPT_PEND_BLOCKING,//OS_OPT_PEND_NON_BLOCKING,
                    (OS_MSG_SIZE  *)&p_msg_size,
                    (CPU_TS       *)0,
                    (OS_ERR       *)&err);
        // �յ���Ϣ
        if ( OS_ERR_NONE == err ) {
            // ��Ϣ����
            //APP_MtrRxDataDealCB(pch); 
            pch->RxBufByteCnt   = p_msg_size;
            APP_MaterCommHandler(pch);
        }
        
        /***********************************************
        * ������ �ȴ�COMM������־λ
        */
        OS_FLAGS    flags = 
        OSFlagPend( ( OS_FLAG_GRP *)&Ctrl.Os.MaterEvtFlagGrp,
                    ( OS_FLAGS     ) Ctrl.Os.MaterEvtFlag,
                    ( OS_TICK      ) dly,
                    ( OS_OPT       ) OS_OPT_PEND_FLAG_SET_ANY | OS_OPT_PEND_NON_BLOCKING,
                    ( CPU_TS      *) NULL,
                    ( OS_ERR      *)&err);
        /***********************************************
        * ������ û�д���,���¼�����
        */
        if ( err == OS_ERR_NONE ) {
            OS_FLAGS    flagClr = 0;
            /***********************************************
            * ������ ����������
            */
            if       ( flags & COMM_EVT_FLAG_HEART  ) {
                flagClr |= COMM_EVT_FLAG_HEART;
                // �������
//                SavePowerData();
            /***********************************************
            * ������ COMM��λ
            */
            } 
            if ( flags & COMM_EVT_FLAG_RESET ) {
                flagClr |= COMM_EVT_FLAG_RESET;
                
            /***********************************************
            * ������ COMM����
            */
            } 
            if ( flags & COMM_EVT_FLAG_CONNECT ) {
                flagClr |= COMM_EVT_FLAG_CONNECT;
                
            /***********************************************
            * ������ ���ڽ���
            */
            } 
            if ( flags & COMM_EVT_FLAG_RECV ) {
                flagClr |= COMM_EVT_FLAG_RECV;
                
            /***********************************************
            * ������ �ϱ���Ϣ
            */
            } 
            if ( flags & COMM_EVT_FLAG_REPORT ) {
                flagClr |= COMM_EVT_FLAG_REPORT; 
                
            /***********************************************
            * ������ �Ͽ�
            */
            } 
            if ( flags & COMM_EVT_FLAG_CLOSE ) {
                flagClr |= COMM_EVT_FLAG_CLOSE;
                
            /***********************************************
            * ������ ��ʱ
            */
            } 
            if ( flags & COMM_EVT_FLAG_TIMEOUT ) {
                flagClr |= COMM_EVT_FLAG_TIMEOUT;
                
            /***********************************************
            * ������ ����
            */
            } 
		// add by fourth peng
            if ( flags & COMM_EVT_SAVE_DATA ) 
		{
                flagClr |= COMM_EVT_SAVE_DATA;
                /***********************************************
                * ������ ��������
                */                 
                App_SaveDataToHistory();
            	}
            if ( flags & COMM_EVT_FLAG_CONFIG ) {
                flagClr |= COMM_EVT_FLAG_CONFIG;
                /***********************************************
                * ������ ��������
                */                 
                SavePowerData();
                //SaveRecord();
                App_SaveDataToHistory();
                

#if	0//ndef NON_ENERGY_CALC
                uartprintf(Ctrl.Com.pch,"\r\n��ǰʱ�䣺20%02d-%02d-%02d  %02d:%02d:%02d", 
                           Mater.Time.Year, Mater.Time.Mon, Mater.Time.Day,
                           Mater.Time.Hour, Mater.Time.Min, Mater.Time.Sec);                uartprintf(Ctrl.Com.pch,
                           "\r\n�洢���ݳɹ� ID = %d\r\n���й� = %d;\r\n���й� = %d;\r\n���޹� = %d;\r\n���޹� = %d;\r\n��  ѹ = %d;\r\n��  �� = %d;\r\nƵ  �� = %d;\r\n�������� = %d;\r\n�й����� = %d;\r\n�޹����� = %d\r\n",
                            Mater.RecordNbr,
                            Mater.Energy.PPPower,      
                            Mater.Energy.NPPower,      
                            Mater.Energy.PQPower,      
                            Mater.Energy.NQPower,      
                            Mater.Energy.PrimVolt,
                            Mater.Energy.PrimCurr,
                            Mater.Energy.PowerFreq,
                            Mater.Energy.PowerFactor,
                            Mater.Energy.ActivePower,  
                            Mater.Energy.ReactivePower);
#endif
                /***********************************************
                * ������ �������ݴ���
                */
            } 
            //exit:
            /***********************************************
            * ������ �����־
            */
            if ( !flagClr ) {
                flagClr = flags;
            }
            
            /***********************************************
            * ������ �����־λ
            */
            OSFlagPost( ( OS_FLAG_GRP  *)&Ctrl.Os.MaterEvtFlagGrp,
                        ( OS_FLAGS      )flagClr,
                        ( OS_OPT        )OS_OPT_POST_FLAG_CLR,
                        ( OS_ERR       *)&err);
        /***********************************************
        * ������ �����ʱ������һ��������
        */
        } else if ( err == OS_ERR_TIMEOUT ) {
            OSFlagPost( ( OS_FLAG_GRP  *)&Ctrl.Os.MaterEvtFlagGrp,
                        ( OS_FLAGS      )Ctrl.Os.MaterEvtFlag,
                        ( OS_OPT        )OS_OPT_POST_FLAG_CLR,
                        ( OS_ERR       *)&err );
        }
#if	0//ndef NON_ENERGY_CALC
        /***********************************************
        * ������ ���й������й������޹������޹�����
        */
		AC.PPpower_NUM = (uint32_t)(AC.PPpower_temp + AC.PPpower_base);
		AC.NPpower_NUM = (uint32_t)(AC.NPpower_temp + AC.NPpower_base);
		AC.PQpower_NUM = (uint32_t)(AC.PQpower_temp + AC.PQpower_base);
		AC.NQpower_NUM = (uint32_t)(AC.NQpower_temp + AC.NQpower_base);
		
        /***********************************************
        * ������ ѭ����ʾ�����ֵΪ99999999
        */ 
        if(AC.PPpower_NUM >= 100000000)	{
		
			AC.PPpulse = 0;
			AC.PPpower_temp = 0;
			AC.PPpower_base = 1; 
		}
        /***********************************************
        * ������ ѭ����ʾ�����ֵΪ99999999
        */ 
		if(AC.NPpower_NUM >= 100000000)	{
		
			AC.NPpulse = 0;
			AC.NPpower_temp = 0;
			AC.NPpower_base = 1; 
		}
        /***********************************************
        * ������ ѭ����ʾ�����ֵΪ99999999
        */ 
		if(AC.PQpower_NUM >= 100000000)	{
		
			AC.PQpulse = 0;
			AC.PQpower_temp = 0;
			AC.PQpower_base = 1; 
		}
        /***********************************************
        * ������ ѭ����ʾ�����ֵΪ99999999
        */ 
		if(AC.NQpower_NUM >= 100000000)	{
		
			AC.NQpulse = 0;
			AC.NQpower_temp = 0;
			AC.NQpower_base = 1; 
		}
        /***********************************************
        * ������ �й����ʡ��޹����ʡ��������ء�Ƶ��
        */ 
		AC.ACTIVE_POWER     = (int32_t)(SPI_CS5463_AC_Read_Else_FLOAT(PA) * 20000);
		AC.REACTIVE_POWER   = (int32_t)(SPI_CS5463_AC_Read_Else_FLOAT(PQ) * 20000);
		AC.Power_Factor     = SPI_CS5463_AC_Read_Else_FLOAT(PF);
		AC.Power_Freq       = SPI_CS5463_AC_Read_Else_FLOAT(FREQUENCY) * 4000;
        
        /***********************************************
        * ������ ��������
        */
        if ( fabs( AC.Power_Factor ) < 0.0005 )
            AC.Power_Factor     = 0;
        
        /***********************************************
        * ������ ��ѹ������
        */ 
		Vac_RMS_F = SPI_CS5463_AC_Read_VIrms_FLOAT(VRMS);
		Iac_RMS_F = SPI_CS5463_AC_Read_VIrms_FLOAT(IRMS);
		
        /***********************************************
        * ������ ˲ʱ��ѹ��˲ʱ����:17020003
        *
        //AC.U_RMS = Vac_RMS_F * 41666.7;				//����ϵ����25000V/0.6(SS4����)
        //AC.I_RMS = Iac_RMS_F * 500;				    //����ϵ����300A/0.6(SS4����)
        
		AC.U_RMS = Vac_RMS_F * ((float)AC.U_SCL/0.6) * 0.99960016;		    //����ϵ����25000V/0.6(HXD1D����)        
		AC.I_RMS = Iac_RMS_F * ((float)AC.I_SCL/0.6);       //1000;	    //����ϵ����600A/0.6(HXD1D����)
		
		//if(Vac_RMS_F < 0.0012)							    //��ѹ����ϵ���ϴ�ȥ������ѹ
		//	AC.U_RMS = 0;
        
        if ( AC.I_RMS  > 80 ) {
             AC.I_RMS = AC.I_RMS * 0.975609756;
        }
        
        /***********************************************
        * ������ ˲ʱ��ѹ��˲ʱ����:17020001
        */
        //AC.U_RMS = Vac_RMS_F * 41666.7;				//����ϵ����25000V/0.6(SS4����)
        //AC.I_RMS = Iac_RMS_F * 500;				    //����ϵ����300A/0.6(SS4����)
        
		AC.U_RMS = Vac_RMS_F * ((float)AC.U_SCL/0.6) * AC.U_K;  //0.995421063;		    //����ϵ����25000V/0.6(HXD1D����)        
		AC.I_RMS = Iac_RMS_F * ((float)AC.I_SCL/0.6) * AC.I_K;       //1000;	    //����ϵ����600A/0.6(HXD1D����)
		
		//if(Vac_RMS_F < 0.0012)							    //��ѹ����ϵ���ϴ�ȥ������ѹ
		//	AC.U_RMS = 0;
        
        //if ( AC.I_RMS  > 80 ) {
        //     AC.I_RMS = AC.I_RMS * 0.975609756;
        //}
        /***********************************************
        * ������ ˲ʱ��ѹ��˲ʱ����:17020002
        * 
        //AC.U_RMS = Vac_RMS_F * 41666.7;				//����ϵ����25000V/0.6(SS4����)
        //AC.I_RMS = Iac_RMS_F * 500;				    //����ϵ����300A/0.6(SS4����)
        
		AC.U_RMS = Vac_RMS_F * ((float)AC.U_SCL/0.6);		    //����ϵ����25000V/0.6(HXD1D����)        
		AC.I_RMS = Iac_RMS_F * ((float)AC.I_SCL/0.6);       //1000;	    //����ϵ����600A/0.6(HXD1D����)
		
		//if(Vac_RMS_F < 0.0012)							    //��ѹ����ϵ���ϴ�ȥ������ѹ
		//	AC.U_RMS = 0;
        
        //if ( AC.I_RMS  > 80 ) {
        //     AC.I_RMS = AC.I_RMS * 0.975609756;
        //}
        */
        AC.U_RMS = Vac_RMS_F * ((float)AC.U_SCL/0.6) * AC.U_K;		    //����ϵ����25000V/0.6(HXD1D����)        
		AC.I_RMS = Iac_RMS_F * ((float)AC.I_SCL/0.6) * AC.I_K;       //1000;	    //����ϵ����600A/0.6(HXD1D����)
		
        /***********************************************
        * ������ 
        */
        
        if ( AC.U_RMS  < AC.U_O ) {
             AC.U_RMS = 0;
        }
        
        Mater.Energy.PPPower        = (uint32_t)((float)AC.PPpower_NUM / (float)(( 100000.0 * 100.0 * 5.0 / (float)AC.U_SCL / (float)AC.I_SCL) ) );			                //���й�����    1kvarh      99999999 kvarh
        Mater.Energy.NPPower        = (uint32_t)((float)AC.NPpower_NUM / (float)(( 100000.0 * 100.0 * 5.0 / (float)AC.U_SCL / (float)AC.I_SCL) ) );			                //���й�����    1kvarh      99999999 kvarh 
        Mater.Energy.PQPower        = (uint32_t)((float)AC.PQpower_NUM / (float)(( 100000.0 * 100.0 * 5.0 / (float)AC.U_SCL / (float)AC.I_SCL) ) );			                //���޹�����    1kvarh      99999999 kvarh
        Mater.Energy.NQPower        = (uint32_t)((float)AC.NQpower_NUM / (float)(( 100000.0 * 100.0 * 5.0 / (float)AC.U_SCL / (float)AC.I_SCL) ) );			                //���޹�����    1kvarh      99999999 kvarh
        Mater.Energy.PrimVolt       = (uint32_t)(AC.U_RMS * 1000);				//ԭ�ߵ�ѹ      0.001V      0��35000.000V
        Mater.Energy.PrimCurr       = (uint32_t)(AC.I_RMS * 1000);				//ԭ�ߵ���      0.001A      0��600.000A
        Mater.Energy.PowerFreq      = (uint32_t)(AC.Power_Freq * 1000);			//Ƶ��          0.001Hz    
        Mater.Energy.PowerFactor    = (int32_t)(AC.Power_Factor * 1000);		//��������      0.001       -1.000��1.000
        Mater.Energy.ActivePower    = AC.ACTIVE_POWER;			                //�й�����      0.001kW     -12000.000  kW��12000.000  kW
        Mater.Energy.ReactivePower  = AC.REACTIVE_POWER;		                //�޹�����      0.001kvar   -12000.000  kvar��12000.000 
        
        SPI_CS5463_AC_GetDrdy();
        SPI_CS5463_AC_ClearDrdy();
#endif
        /***********************************************
        * ������ ����ʣ��ʱ��
        */
        dly   = CYCLE_TIME_TICKS - ( OSTimeGet(&err) - ticks );
        if ( dly  < 1 ) {
            dly = 1;
        } else if ( dly > CYCLE_TIME_TICKS ) {
            dly = CYCLE_TIME_TICKS;
        }
    }
}

/*******************************************************************************
 * ��    �ƣ� APP_MaterInit
 * ��    �ܣ� �����ʼ��
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ� wumingshen.
 * �������ڣ� 2016-11-11
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
 void APP_MaterInit(void)
{
    /***********************************************
    * ������ �����¼���־��
    */    
	//SZ_USART1_DATA.uart = SZ_STM32_COM1;
#if 0    
	AC.U_RMS = 0;
	AC.I_RMS = 0;
    
	AC.Power_Factor = 0;
	AC.Power_Freq = 0;
    
	AC.ACTIVE_POWER = 0;
	AC.REACTIVE_POWER = 0;
    
	AC.PPpower_NUM = 0;								
	AC.NPpower_NUM = 0;			
	AC.PPpulse = 0;				
	AC.NPpulse = 0;				
	AC.PPpower_base = 0;		
	AC.NPpower_base = 0;		
	AC.PPpower_temp = 0;		
	AC.NPpower_temp = 0;
    
	AC.PQpower_NUM = 0;								
	AC.NQpower_NUM = 0;			
	AC.PQpulse = 0;				
	AC.NQpulse = 0;				
	AC.PQpower_base = 0;		
	AC.NQpower_base = 0;		
	AC.PQpower_temp = 0;		
	AC.NQpower_temp = 0;
#endif
        
    BSP_OS_TimeDly(OS_TICKS_PER_SEC);
    /***********************************************
    * ������ 
    */
	//SZ_STM32_COMInit(COM1, 115200);
	//RS485_SET_RX_Mode();      /* Ĭ������RS485�ķ���Ϊ���գ������������߳�ͻ */
	//xPrintf_Init(19200);           //COM2��Ӧ����RS485
	//LED_DIS_Config();
	I2C_GPIO_Config();
	    EEP_OS_Init();
	InitDS3231();
//	SPI_AC_INIT();
//	CS5463_AC_INIT();
	SPI_FLASH_Init();
    
    /***********************************************
    * ������ 
    */
// fourth marked at 2017-5-11
//    DC_COUNTInit();
//	AC_COUNTInit();

    /***********************************************
    * ������ 
    */
	//NVIC_GroupConfig();
	//NVIC_COMConfiguration();
    
    /***********************************************
    * ������ 
    */
	//Dis_Test();							 //������ϵ����
   	//SZ_STM32_SysTickInit(100);

// fourth marked at 2017-5-11
//	PWR_PVD_Init();
//    WDG_Init();
        
    /***********************************************
    * ������ �ڿ��Ź���־��ע�᱾����Ŀ��Ź���־
    */
    WdtFlags |= WDT_FLAG_MATER;
	app_read_system_para();
}

/*******************************************************************************
 * ��    �ƣ� DC_COUNTInit
 * ��    �ܣ� ��ʼ��PE9���ⲿ�жϹ��ܣ������������
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ� wumingshen.
 * �������ڣ� 2016-11-16
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void DC_COUNTInit(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    EXTI_InitTypeDef EXTI_InitStructure;
    //NVIC_InitTypeDef NVIC_InitStructure;
    
    /* ʹ��GPIOB��Clockʱ�� */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE | RCC_APB2Periph_AFIO, ENABLE);
    
    /* Configure Button pin as input floating */
    /* ��ʼ��GPIOB8�ܽţ�����Ϊ������������ */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_Init(GPIOE, &GPIO_InitStructure);
    
    /* ��ʼ��GPIOA0Ϊ�ж�ģʽ */
    /* ��GPIOA0��Ӧ�Ĺܽ����ӵ��ڲ��ж��� */    
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOE , GPIO_PinSource9);
    
    /* Configure Button EXTI line */
    /* ��GPIOA0����Ϊ�ж�ģʽ���½��ش����ж� */    
    EXTI_InitStructure.EXTI_Line = EXTI_Line9;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;  
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);
    
    //NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;
    //NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    //NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	//NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    //NVIC_Init(&NVIC_InitStructure);
	EXTI_ClearITPendingBit(EXTI_Line9); 
    
    /***********************************************
    * ������ �ⲿ�ж�9 ~ 5����
    */ 
    BSP_IntVectSet(BSP_INT_ID_EXTI9_5, EXTI9_5_ISRHandler);
    BSP_IntEn(BSP_INT_ID_EXTI9_5);
}

/*******************************************************************************
 * ��    �ƣ� AC_COUNTInit
 * ��    �ܣ� ��ʼ��PE0���ⲿ�жϹ��ܣ������������
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ� wumingshen.
 * �������ڣ� 2016-11-16
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void AC_COUNTInit(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    EXTI_InitTypeDef EXTI_InitStructure;
    //NVIC_InitTypeDef NVIC_InitStructure;
    
    /* ʹ��GPIOE��Clockʱ�� */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE | RCC_APB2Periph_AFIO, ENABLE);
    
    /* Configure Button pin as input floating */
    /* ��ʼ��GPIOE0�ܽţ�����Ϊ������������ */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_Init(GPIOE, &GPIO_InitStructure);
    
	/* ��ʼ��GPIOE0Ϊ�ж�ģʽ */
    /* ��GPIOE0��Ӧ�Ĺܽ����ӵ��ڲ��ж��� */    
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOE , GPIO_PinSource0);
    
    /* ��ʼ��GPIOD2ΪGPIO */   
	/* ʹ��GPIOD��Clockʱ�� */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
    /* Configure Button pin as input floating */
    /* ��ʼ��GPIOD2�ܽţ�����Ϊ������������ */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_Init(GPIOD, &GPIO_InitStructure);    
    
    EXTI_InitStructure.EXTI_Line = EXTI_Line0;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;  
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);
    
    //NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;
    //NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    //NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    //NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    //NVIC_Init(&NVIC_InitStructure);
	EXTI_ClearITPendingBit(EXTI_Line0); 
    
    /***********************************************
    * ������ �����ⲿ�ж������������ж�
    */    
    /***********************************************
    * ������ �ⲿ�ж�0 ~ 4����
    */ 
    BSP_IntVectSet(BSP_INT_ID_EXTI0, EXTI0_ISRHandler);
    BSP_IntEn(BSP_INT_ID_EXTI0);
}

/*******************************************************************************
 * ��    �ƣ� EXTI0_IRQHandler
 * ��    �ܣ� �ⲿ�ж�0�������������������й�����
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ� wumingshen.
 * �������ڣ� 2016-11-16
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void EXTI0_ISRHandler(void) 
{   
	if(EXTI_GetITStatus(EXTI_Line0) != RESET) {        
#if 0
        if ( ( Mater.Energy.PrimVolt        == 0 ) ||
             ( Mater.Energy.PrimCurr        == 0 ) ||
             ( Mater.Energy.PowerFreq       == 0 ) ||
             ( Mater.Energy.PowerFactor     == 0 ) ) {
        } else {
            //���й�
            if(GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_2)) {
                AC.PPpulse++;
                //	AC.Ppower_temp = ((float)(AC.Ppulse * 15)) / 100.00 + 0.5;  //���峣��100000��25kV/100V---300A/5A(SS4����)
                AC.PPpower_temp = AC.PPpulse;                    //���峣��100000��25kV/150V---600A/1A(HXD1D����)
            //���й�
            } else {
                AC.NPpulse++;
                //	AC.Npower_temp = ((float)(AC.Npulse * 15)) / 100.00 + 0.5;  //���峣��100000��25kV/100V---300A/5A(SS4����)
                AC.NPpower_temp = AC.NPpulse;                    //���峣��100000��25kV/150V---600A/1A(HXD1D����)
            }
        }
#endif
        /* Clear the EXTI Line 0 */  
        EXTI_ClearITPendingBit(EXTI_Line0);
	}
}

/*******************************************************************************
 * ��    �ƣ� EXTI1_IRQHandler
 * ��    �ܣ� �ⲿ�ж�1�������������������޹�����
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ� wumingshen.
 * �������ڣ� 2016-11-16
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void EXTI9_5_ISRHandler(void) 
{
	if(EXTI_GetITStatus(EXTI_Line9) != RESET) {
#if 0
        if ( ( Mater.Energy.PrimVolt        == 0 ) ||
             ( Mater.Energy.PrimCurr        == 0 ) ||
             ( Mater.Energy.PowerFreq       == 0 ) ||
             ( Mater.Energy.PowerFactor     == 0 ) ) {
        } else {
            //���޹�
            if(GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_2)) {
                AC.PQpulse++;
                //	AC.PQpower_temp = ((float)(AC.Ppulse * 15)) / 100.00 + 0.5;  //���峣��100000��25kV/100V---300A/5A(SS4����)
                AC.PQpower_temp = AC.PQpulse;                    //���峣��100000��25kV/150V---600A/1A(HXD1D����)
            //���޹�
            } else {
                AC.NQpulse++;
                //	AC.NQpower_temp = ((float)(AC.Npulse * 15)) / 100.00 + 0.5;  //���峣��100000��25kV/100V---300A/5A(SS4����)
                AC.NQpower_temp = AC.NQpulse;                    //���峣��100000��25kV/150V---600A/1A(HXD1D����)
            }
            //һ�������Ӧԭ��һ�ȵ�
            
            //uartprintf("\r\n ���޹�������%d��������%dkvarh\n", AC.PQpulse,AC.PQpower_temp);
            //uartprintf("\r\n ���޹�������%d��������%dkvarh\n", AC.NQpulse,AC.NQpower_temp);
        }
#endif
        /* Clear the EXTI Line 9 */  
        EXTI_ClearITPendingBit(EXTI_Line9);
	}
}

/*******************************************************************************
 * ��    �ƣ� APP_MaterDispHandler
 * ��    �ܣ� �����ʾ
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ���� ���ߣ� wumingshen.
 * �������ڣ� 2015-03-28
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void APP_MaterDispHandler(INT08U *step)
{
	//static uint8_t step = 0;
    
    BSP_DispClrAll();
    switch ( *step ) {
    /***********************************************
    * ������ 
    */
#if 0
    case 0:     uprintf("E1      "); 	            break;  // 
    case 1:     uprintf("%8d",Mater.Energy.PPPower);		break;  //��ʾ���й�����
    case 2:     uprintf("E2      ");		        break;
    case 3:     uprintf("%8d",Mater.Energy.NPPower);*step = 7;		break;	//��ʾ���й�����
    case 4:     uprintf("E3      ");		        break;
    case 5:     uprintf("%8d",Mater.Energy.PQPower);		break;	//��ʾ���޹�����
    case 6:     uprintf("E4      ");		        break;
    case 7:     uprintf("%8d",Mater.Energy.NQPower);		break;  //��ʾ���޹�����
    case 8:     uprintf("U %6d",(uint32_t)AC.U_RMS);break;  //��ʾ��ѹ
    case 9:     uprintf("A %6d",(uint32_t)AC.I_RMS);break;  //��ʾ����
    case 10:    
        if ( AC.Power_Factor == 1.0 )
            uprintf("P   .1000",AC.Power_Factor);	        //��ʾ����
        else if ( AC.Power_Factor == -1.0 )
            uprintf("P  -.1000",AC.Power_Factor);	        //��ʾ����
        else if(AC.Power_Factor < 0 )
            uprintf("P  -.0%03d",abs(AC.Power_Factor * 1000.0));	//��ʾ����
        else
            uprintf("P   .0%03d",(int)(AC.Power_Factor * 1000.0));	//��ʾ����
        break;  //��ʾ����
#endif
            
    default:
	    uprintf("S %6d",(uint32_t)Mater.monitor.real_speed);
        *step   = 0;
        break;
    }
    *step  += 1;
    if ( *step > 10 )
        *step   = 0;    
}
/*******************************************************************************
 * ��    �ƣ� APP_MaterCommHandler
 * ��    �ܣ� ��������ݴ�����
 * ��ڲ����� ��
 * ���ڲ����� ��
 * �������ߣ� wumingshen.
 * �������ڣ� 2015-03-28
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
/***********************************************
* ������ 2015/12/07���ӣ����ڷ�MODBBUSͨ��
*        ��MODBUSͨ�ţ���֡ͷ֡β��ͨ�����ݴ���
*/
#if MB_NONMODBUS_EN == DEF_ENABLED
CPU_BOOLEAN APP_MaterCommHandler (MODBUS_CH  *pch)
{    
    uint8_t     fram_clr[4]     = {0};
	uint8_t     IDBuf[4]        = {0};
	uint8_t     clear_buf[24]   = {0};
	uint32_t    ID              = 0;
	uint32_t    utemp;

	static ST_QUEUETCB queue = {0};

	TIME        system; 
    
    /***********************************************
    * ������ �������ݵ�������
    */
    //CPU_SR_ALLOC();
    //CPU_CRITICAL_ENTER();
    //uint32_t  Len     = pch->RxBufByteCnt;
    //memcpy( (INT08U *)&pch->RxFrameData, (INT08U *)pch->RxBuf, Len );
    //CPU_CRITICAL_EXIT();
	uint32_t  Len       = pch->RxBufByteCnt;
	INT08U  *DataBuf    = pch->RxFrameData;

	CPU_INT16U  head    = BUILD_INT16U(pch->RxBuf[1], pch->RxBuf[0]);
	CPU_INT16U  tail    = BUILD_INT16U(pch->RxBuf[pch->RxBufByteCnt-1],
	                                   pch->RxBuf[pch->RxBufByteCnt-2]);
    /***********************************************
    * ������ ��ɳ�ϳ�DTUЭ�鴦��
    */
	if ( ( pch->RxFrameHead == head ) &&
	     ( pch->RxFrameTail == tail ) ) 
	{
		DataBuf = &pch->RxFrameData[8];
		Len     = Ctrl.Com.Rd.Head.DataLen;        
		head    = BUILD_INT16U(DataBuf[1], DataBuf[0]);
	}
    
    /***********************************************
    * ������ 
    ���    ����       �ֽ��� ����˵��
    1       ֡ͷ       2      0xAA 0xAA
    2       ֡����     1      0x08 ֡���ȹ̶�Ϊ8�ֽ�
    3       ��������   2      0x00 0x01
    4       Ԥ��       2      0x00 0x00
    5       �ۼӺ�     1      ǰ7���ֽڵ��ۼӺ�
    */ 
	if ( 0xAAAA == head ) 
	{
		Len            -= 1;
		int8_t chkSum   = GetCheckSum(DataBuf,Len);

		if ( chkSum ==  DataBuf[Len] ) 
		{
			pch->TxFrameData[0]     = 0xAA;
			pch->TxFrameData[1]     = 0xAA;
			pch->TxFrameData[2]     = 0x30;
			pch->TxFrameData[3]     = 0x00;
			pch->TxFrameData[4]     = 0x71;

			CPU_SR  cpu_sr;
			OS_CRITICAL_ENTER();
			memcpy(&pch->TxFrameData[5],(uint8_t *)&Mater.monitor, sizeof(ST_MONITOR));
			OS_CRITICAL_EXIT();

			pch->TxFrameData[45]    = 0x00;
			pch->TxFrameData[46]    = 0x00;

			pch->TxFrameData[47]    = GetCheckSum(pch->TxFrameData,47);

			NMB_Tx((MODBUS_CH   *)pch,
			(CPU_INT08U  *)pch->TxFrameData,
			(CPU_INT16U   )48);
			return TRUE;
		}
		/***********************************************
		* ������ ���Բ���
		*/
	} 
	else if ( ( DEBUG_CMD_HEAD == head ) && ( Len > 10 )) 
	{
		/***********************************************
		* ������ ����Ƿ���֡β��û��֡β�����CRCУ��
		*/
		if ( tail != DEBUG_CMD_TAIL) 
		{
			uint16_t crc16_Cal = crc16((uint8_t *)&DataBuf[0],9);
			uint16_t crc16_Rec = BUILD_INT16U(DataBuf[9],DataBuf[10]);
			if(crc16_Cal != crc16_Rec) 
			{
				return TRUE;
			}
		}
		switch(DataBuf[2])
		{
			case SET_DISPLAY_PARA:
			{
				ST_DISPLAY_PARA_SET *p_disp_para = (ST_DISPLAY_PARA_SET *)&DataBuf[3];
				if ( DISPLAY_STEP_MAX > p_disp_para->index)
				{
					if (DEF_ON < p_disp_para->enbale)
					{
						uartprintf(pch,"display enable error! only accept 1 or 0\r\n");
					}
					else
					{
						system_para.display_setting[p_disp_para->index].enable = p_disp_para->enbale;
						system_para.display_setting[p_disp_para->index].times = p_disp_para->time;
						app_save_system_para();
						app_display_step_sequence();
					}
					
				}
				else
				{
					uartprintf(pch,"display index error!\r\n");
				}
				
				break;
			}
			
			case SET_PHASE_DUTY_THRESHOLD:
			{
				system_para.collector_threshold.phase_threshold_low 	= DataBuf[4];
				system_para.collector_threshold.phase_threshold_hig 	= DataBuf[5];
				system_para.collector_threshold.duty_threshold_low 	= DataBuf[7];
				system_para.collector_threshold.duty_threshold_hig 	= DataBuf[8];
				app_save_system_para();
				break;
			}
			
			case SET_THRESHOLD_OF_SENSOR:
			{
				system_para.collector_threshold.sensor_vcc_threshold_low = BUILD_INT16U(DataBuf[3], DataBuf[4]);
				system_para.collector_threshold.sensor_vcc_threshold_hig = BUILD_INT16U(DataBuf[5], DataBuf[6]);
				system_para.collector_threshold.hig_level_threshold_factor = DataBuf[7];
				system_para.collector_threshold.low_level_threshold_factor = DataBuf[8];
				app_save_system_para();
				break;
			}
			
			case SET_THRESHOLD_OF_EDGE_TIME:
			{
				system_para.collector_threshold.edge_time_threshold = DataBuf[3];
				app_save_system_para();
				break;
			}

			case SET_CONDITION_PARAMETER:
			{
				memcpy(&system_para.condition_factor, &DataBuf[3], sizeof(system_para.condition_factor));
				app_save_system_para();
				break;
			}

			case SET_RTC_BAT_VOL_FACTOR:
			{
				system_para.bat_vol_factor = (((DataBuf[3]&0xf0)>>4)*1000) + ((DataBuf[3]&0xf)*100)
										 +(((DataBuf[4]&0xf0)>>4)*10) + (DataBuf[4]&0xf)
										 +(((DataBuf[5]&0xf0)>>4)*0.1) + (((DataBuf[5]&0xf)>>4)*0.01)
										 +(((DataBuf[6]&0xf0)>>4)*0.001) + (((DataBuf[6]&0xf)>>4)*0.0001)
										 +(((DataBuf[7]&0xf0)>>4)*0.00001) + (((DataBuf[7]&0xf)>>4)*0.000001)
										 +(((DataBuf[8]&0xf0)>>4)*0.0000001) + (((DataBuf[8]&0xf)>>4)*0.00000001);
				app_save_system_para();

				break;
			}

			case SET_BAT_VOL_FACTOR:
			{
				system_para.voltage_factor = (((DataBuf[3]&0xf0)>>4)*1000) + ((DataBuf[3]&0xf)*100)
										  +(((DataBuf[4]&0xf0)>>4)*10) + (DataBuf[4]&0xf)
										  +(((DataBuf[5]&0xf0)>>4)*0.1) + (((DataBuf[5]&0xf)>>4)*0.01)
										  +(((DataBuf[6]&0xf0)>>4)*0.001) + (((DataBuf[6]&0xf)>>4)*0.0001)
										  +(((DataBuf[7]&0xf0)>>4)*0.00001) + (((DataBuf[7]&0xf)>>4)*0.000001)
										  +(((DataBuf[8]&0xf0)>>4)*0.0000001) + (((DataBuf[8]&0xf)>>4)*0.00000001);
				app_save_system_para();
				break;
			}

			case SET_SPEED_FACTOR:
			{
				system_para.speed_factor = (((DataBuf[3]&0xf0)>>4)*1000) + ((DataBuf[3]&0xf)*100)
										  +(((DataBuf[4]&0xf0)>>4)*10) + (DataBuf[4]&0xf)
										  +(((DataBuf[5]&0xf0)>>4)*0.1) + (((DataBuf[5]&0xf)>>4)*0.01)
										  +(((DataBuf[6]&0xf0)>>4)*0.001) + (((DataBuf[6]&0xf)>>4)*0.0001)
										  +(((DataBuf[7]&0xf0)>>4)*0.00001) + (((DataBuf[7]&0xf)>>4)*0.000001)
										  +(((DataBuf[8]&0xf0)>>4)*0.0000001) + (((DataBuf[8]&0xf)>>4)*0.00000001);
				app_save_system_para();
				break;			
			}

			case SET_SAMPLE_CYCLE_TIME:
			{
				OS_ERR err;
				system_para.sample_cycle = BUILD_INT16U(DataBuf[3], DataBuf[4]);
				app_save_system_para();
				queue.event = COMM_TASK_CREATE_SAMPLE_TMR;
				queue.queue_data.pdata = &system_para.sample_cycle;
				queue.queue_data.len = sizeof(system_para.sample_cycle);
//				OSQPost(&COM_RxQ, &queue, sizeof(queue), OS_OPT_POST_FIFO, &err);	
				break;
			}

			/***********************************************
			* ������ �������к�
			*/
			case SET_DEVICE_ID:
				ID = BUILD_INT32U(DataBuf[3], DataBuf[4], DataBuf[5], DataBuf[6]);
				WriteFM24CL64(64 , (uint8_t *)&ID , 4);
				ReadFM24CL64(64 , (uint8_t *)&ID , 4);
				uartprintf(pch,"\r\n ��װ��ID����Ϊ��%d\r\n" , ID);
				Ctrl.ID = ID;
				break;            
			/***********************************************
			* ������ ���û����ͺ�/������
			*/
			case SET_MODEL_AND_NUMBER:	
				Mater.LocoTyp   = BUILD_INT16U(DataBuf[3],DataBuf[4]);
				Mater.LocoNbr   = BUILD_INT16U(DataBuf[5],DataBuf[6]);
				WriteFM24CL64(68 , (uint8_t *)&Mater.LocoTyp, 2);
				WriteFM24CL64(70 , (uint8_t *)&Mater.LocoNbr, 2);
				ReadFM24CL64(68 ,  (uint8_t *)&Mater.LocoTyp, 2);
				ReadFM24CL64(70 ,  (uint8_t *)&Mater.LocoNbr, 2);
				uartprintf(pch,"\r\n ��ǰ���ͣ�%d����ǰ���ţ�%d\r\n", Mater.LocoTyp,Mater.LocoNbr);
				break; 
			/***********************************************
			* ������ ����ʱ��
			*/
			case SET_SYSEM_TIME:
				system.Year     =DataBuf[3];
				system.Month    =DataBuf[4];
				system.Day      =DataBuf[5];
				system.Hour     =DataBuf[6];
				system.Min      =DataBuf[7];
				system.Sec      =DataBuf[8];
				WriteTime(system);
				GetTime((TIME *)&recordsfr.Time[0]);
				uartprintf(pch,"\r\n ��ǰʱ��Ϊ��20%02d-%02d-%02d  %02d:%02d:%02d", 
				recordsfr.Time[0], recordsfr.Time[1], recordsfr.Time[2],
				recordsfr.Time[3], recordsfr.Time[4], recordsfr.Time[5]);
				break;

			/***********************************************
			* ������ ��ȡID
			*/
			case READ_DEVICE_ID:				
				ReadFM24CL64(64 , (uint8_t *)&ID , 4);
				uartprintf(pch,"\r\n ��װ��IDΪ��%d\r\n" , ID);
				break;
			/***********************************************
			* ������ 
			*/
			case READ_MODEL_AND_NUMBER:
				ReadFM24CL64(68 , (uint8_t *)&Mater.LocoTyp, 2);
				ReadFM24CL64(70 , (uint8_t *)&Mater.LocoNbr, 2);

				if( ( Mater.LocoTyp > 0 ) &&
				( Mater.LocoTyp < 10000 ) &&
				( Mater.LocoNbr > 0 ) && 
				( Mater.LocoNbr < 10000 ) ) 
				{
					uartprintf(pch,"\r\n ��ǰ���ͣ�%d�� ��ǰ���ţ�%04d\r\n" , Mater.LocoTyp,Mater.LocoNbr);
				} 
				else 
				{
					uartprintf(pch,"\r\n δ����װ�����ͣ�\r\n");
				}
				break;            
			/***********************************************
			* ������ ��ȡʱ��
			*/
			case GET_SYSTEM_TIME:
				GetTime((TIME *)&recordsfr.Time[0]);
				uartprintf(pch,"\r\n ��ǰʱ��Ϊ��20%02d-%02d-%02d  %02d:%02d:%02d", 
				recordsfr.Time[0], recordsfr.Time[1], recordsfr.Time[2],
				recordsfr.Time[3], recordsfr.Time[4], recordsfr.Time[5]);				
				break;            
			/***********************************************
			* ������ ͨ������ת��FLASH�е�������Ч����
			*/
			case GET_HISTORY_DATA:
				GetTime((TIME *)&recordsfr.Time[0]);
				uartprintf(pch,"\r\n ��ǰʱ��Ϊ��20%02d-%02d-%02d  %02d:%02d:%02d", 
				recordsfr.Time[0], recordsfr.Time[1], recordsfr.Time[2],
				recordsfr.Time[3], recordsfr.Time[4], recordsfr.Time[5]);

				ReadFM24CL64(68 , (uint8_t *)&Mater.LocoTyp, 2);
				ReadFM24CL64(70 , (uint8_t *)&Mater.LocoNbr, 2);
				if( ( Mater.LocoTyp > 0 ) &&
				( Mater.LocoTyp < 10000 ) &&
				( Mater.LocoNbr > 0 ) && 
				( Mater.LocoNbr < 10000 ) ) 
				{
					uartprintf(pch,"\r\n ��ǰ���ͣ�%d�� ��ǰ���ţ�%04d\r\n" , Mater.LocoTyp,Mater.LocoNbr);
				} 
				else 
				{
					uartprintf(pch,"\r\n δ����װ�����ͣ�\r\n");
				}

				DOWNLOAD(g_Flash_Adr/sizeof(Mater));
				if(g_Flash_Adr == 0)
					uartprintf(pch,"\r\n FLASHоƬ������Ч���ݣ�\r\n");
				break;

			case EARSE_FLASH:
				uartprintf(pch,"\r\n FLASHоƬ������............\r\n");
				uartprintf(pch,"\r\n ��Լ��Ҫ30�룬�����ĵȴ���\r\n");
				MX25L3206_Erase(0, 4096);
				uartprintf(pch,"\r\n FLASHоƬ�Ѳ���\r\n");
				break;

			case EARSE_FRAM:
				uartprintf(pch,"\r\n FRAMоƬ������...\r\n");
				WriteFM24CL64(60, fram_clr, 4);
				g_Flash_Adr = 0;
				Mater.Head  = 0;
				Mater.Tail  = 0;
				Mater.RecordNbr  = 0;
#ifndef SAMPLE_BOARD
				app_save_record_number_and_index();
#endif
				/*
				WriteFM24CL64(72, (uint8_t *)&Mater.Head, 4); 
				WriteFM24CL64(76, (uint8_t *)&Mater.Tail, 4);
				WriteFM24CL64(80, (uint8_t *)&Mater.RecordNbr, 4);
				*/    
				uartprintf(pch,"\r\n FRAMоƬ�Ѳ���\r\n");
				break;
			case SET_RECORD_CYCLE: 
				utemp   = BUILD_INT32U(DataBuf[3],DataBuf[4],DataBuf[5],DataBuf[6]); 
				if ( utemp < 10 )
				Mater.RecordTime    = OS_TICKS_PER_SEC * 60;
				else if ( utemp > 300 )
				Mater.RecordTime    = OS_TICKS_PER_SEC * 60;
				else
				Mater.RecordTime    = OS_TICKS_PER_SEC * utemp;

				WriteFM24CL64(84, (uint8_t *)&Mater.RecordTime, 4);
				ReadFM24CL64(84 , (uint8_t *)&utemp, 4);
				osal_start_timerRl( OS_TASK_ID_TMR, OS_EVT_TMR_MIN ,Mater.RecordTime);
				uartprintf(pch,"\r\n �������ݼ�¼/���ͼ��ʱ��Ϊ��%d��\r\n",utemp / OS_TICKS_PER_SEC);
				break;
				
			case READ_RECORD_CYCLE:    
				ReadFM24CL64(84 , (uint8_t *)&utemp, 4);
				uartprintf(pch,"\r\n ��ǰ���ݼ�¼/���ͼ��ʱ��Ϊ��%d��\r\n",utemp / OS_TICKS_PER_SEC);

			default:
			break;
		}
	}
    /***********************************************
    * ������ ���ô������ݴ���ص�����
    */ 
    return TRUE;
}
 extern void UpdateRecordPoint(uint8_t storeflag);

/*******************************************************************************
 * ��    �ƣ� APP_CSNC_CommHandler
 * ��    �ܣ� �������ݴ�����
 * ��ڲ����� ��
 * ���ڲ����� ��
 * �������ߣ� wumingshen.
 * �������ڣ� 2015-03-28
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/

CPU_BOOLEAN APP_CSNC_CommHandler (MODBUS_CH  *pch)
{   
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
            
            osal_set_event( OS_TASK_ID_TMR, OS_EVT_TMR_MIN);
        }
    } else {
        /***********************************************
        * ������ �����ݴ�������ͽṹ
        */ 
        APP_MaterCommHandler(pch);
    }
    
    /***********************************************
    * ������ ���ڽ���COMMģ�����Ϣ������
    */
    Ctrl.Com.ConnectTimeOut    = 0;                // ��ʱ����������
    Ctrl.Com.ConnectFlag       = TRUE;             // ת���ӳɹ���־
    
    return TRUE;
}
#endif

void SavePowerData(void)
{
#if 0
    /***********************************************
    * ������ �������й��������ݵ�FRAM
    *       ���й�����    1kvarh      99999999 kvarh
    */
    if ( recordsfr.PPpower_NUM != AC.PPpower_NUM ) {	
        WriteFM24CL64(0, (uint8_t *)&AC.PPpower_NUM,4);
        WriteFM24CL64(4, (uint8_t *)&AC.PPpower_NUM,4);
        WriteFM24CL64(8, (uint8_t *)&AC.PPpower_NUM,4);
    }
    /***********************************************
    * ������ ���渺�й��������ݵ�FRAM
    *        ���й�����    1kvarh      99999999 kvarh 
    */ 
    if ( recordsfr.NPpower_NUM != AC.NPpower_NUM ) { 
        WriteFM24CL64(12, (uint8_t *)&AC.NPpower_NUM,4);
        WriteFM24CL64(16, (uint8_t *)&AC.NPpower_NUM,4);
        WriteFM24CL64(20, (uint8_t *)&AC.NPpower_NUM,4);
    }
    /***********************************************
    * ������ ���渺�й��������ݵ�FRAM
    *        ���޹�����    1kvarh      99999999 kvarh 
    */
    if ( recordsfr.PQpower_NUM != AC.PQpower_NUM ) {
        WriteFM24CL64(100, (uint8_t *)&AC.PQpower_NUM,4);
        WriteFM24CL64(104, (uint8_t *)&AC.PQpower_NUM,4);
        WriteFM24CL64(108, (uint8_t *)&AC.PQpower_NUM,4);
    }
    /***********************************************
    * ������ ���渺�й��������ݵ�FRAM
    *       ���޹�����    1kvarh      99999999 kvarh 
    */ 
    if ( recordsfr.NQpower_NUM != AC.NQpower_NUM ) {
        WriteFM24CL64(112, (uint8_t *)&AC.NQpower_NUM,4);			  
        WriteFM24CL64(116, (uint8_t *)&AC.NQpower_NUM,4);
        WriteFM24CL64(120, (uint8_t *)&AC.NQpower_NUM,4);
    }    
    
    recordsfr.PPpower_NUM       = AC.PPpower_NUM;
    recordsfr.NPpower_NUM       = AC.NPpower_NUM;
    recordsfr.PQpower_NUM       = AC.PQpower_NUM;
    recordsfr.NQpower_NUM       = AC.NQpower_NUM;
#endif
}

void PVD_IRQHandler(void)
{
    EXTI_ClearITPendingBit(EXTI_Line16);            //���ж�

#if 0    
    //num = BKP_ReadBackupRegister(BKP_DR10);
    //num++;   
    
    //�û���ӽ���������봦    
    //RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);//ʹ��PWR��BKP����ʱ��
    //PWR_BackupAccessCmd(ENABLE);//ʹ�ܺ󱸼Ĵ�������
    
    //BKP_WriteBackupRegister(BKP_DR10, (u8)num);//��������
    
    WriteFM24CL64(0, (uint8_t *)&AC.PPpower_NUM,4);
    WriteFM24CL64(4, (uint8_t *)&AC.PPpower_NUM,4);
    WriteFM24CL64(8, (uint8_t *)&AC.PPpower_NUM,4);
    
    //�����й���������ת�浽������
    WriteFM24CL64(12, (uint8_t *)&AC.NPpower_NUM,4);			  //���ֽ���ǰ�����ֽ��ں�
    WriteFM24CL64(16, (uint8_t *)&AC.NPpower_NUM,4);
    WriteFM24CL64(20, (uint8_t *)&AC.NPpower_NUM,4);                
    
    WriteFM24CL64(100, (uint8_t *)&AC.PQpower_NUM,4);
    WriteFM24CL64(104, (uint8_t *)&AC.PQpower_NUM,4);
    WriteFM24CL64(108, (uint8_t *)&AC.PQpower_NUM,4);
    
    //�����й���������ת�浽������
    WriteFM24CL64(112, (uint8_t *)&AC.NQpower_NUM,4);			  //���ֽ���ǰ�����ֽ��ں�
    WriteFM24CL64(116, (uint8_t *)&AC.NQpower_NUM,4);
    WriteFM24CL64(120, (uint8_t *)&AC.NQpower_NUM,4);
#endif
}

void PWR_PVD_Init(void)
{  
    NVIC_InitTypeDef NVIC_InitStructure;
    EXTI_InitTypeDef EXTI_InitStructure;    
    
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);     //ʹ��PWRʱ��    
    
    //NVIC_InitStructure.NVIC_IRQChannel = PVD_IRQn;           //ʹ��PVD���ڵ��ⲿ�ж�ͨ��
    //NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;//��ռ���ȼ�1
    //NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;       //�����ȼ�0
    //NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;          //ʹ���ⲿ�ж�ͨ��
    //NVIC_Init(&NVIC_InitStructure);
            
    /***********************************************
    * ������ �ⲿ�ж�0 ~ 4����
    */ 
    BSP_IntVectSet(BSP_INT_ID_PVD, PVD_IRQHandler);
    BSP_IntEn(BSP_INT_ID_PVD);    
    
    EXTI_StructInit(&EXTI_InitStructure);
    EXTI_InitStructure.EXTI_Line = EXTI_Line16;             //PVD���ӵ��ж���16��
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;     //ʹ���ж�ģʽ
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;  //��ѹ���ڷ�ֵʱ�����ж�
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;               //ʹ���ж���
    EXTI_Init(&EXTI_InitStructure);                         //��ʼ   
    
    PWR_PVDLevelConfig(PWR_PVDLevel_2V9);                   //�趨��ط�ֵ
    PWR_PVDCmd(ENABLE);                                     //ʹ��PVD    
}

/*******************************************************************************
 * 				end of file
 *******************************************************************************/
#endif
