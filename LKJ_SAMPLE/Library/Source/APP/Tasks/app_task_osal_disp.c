/*******************************************************************************
 *   Filename:       app_task_disp.c
 *   Revised:        All copyrights reserved to wumingshen.
 *   Revision:       v1.0
 *   Writer:	     wumingshen.
 *
 *   Description:    ˫��ѡ�� disp �� Ctrl + H, ��ѡ Match the case, Replace with
 *                   ������Ҫ�����֣���� Replace All
 *                   ˫��ѡ�� Disp �� Ctrl + H, ��ѡ Match the case, Replace with
 *                   ������Ҫ�����֣���� Replace All
 *                   ˫��ѡ�� DISP �� Ctrl + H, ��ѡ Match the case, Replace with
 *                   ������Ҫ�����֣���� Replace All
 *                   �� app_cfg.h ��ָ��������� ���ȼ�  �� APP_TASK_DISP_PRIO ��
 *                                            �� �����ջ�� APP_TASK_DISP_STK_SIZE ����С
 *                   �� app.h �������������     ���������� void  App_TaskDispCreate(void) ��
 *                                            �� ���Ź���־λ �� WDTFLAG_Disp ��
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
#include <global.h>
#include <bsp_adc7682.h>
#include <bsp_max7219.h>
#include <iap.h>
#include "DS3231.h"
#ifdef PHOTOELECTRIC_VELOCITY_MEASUREMENT
#include <Speed_sensor.h>
#include "app_task_mater.h"
#else
#include <power_macro.h>
#endif

#include "app_voltage_detect.h"
#include "dtc.h"
#include "display.h"

#ifdef VSC_INCLUDE_SOURCE_FILE_NAMES
const  CPU_CHAR  *app_task_disp__c = "$Id: $";
#endif

#define APP_TASK_DISP_EN     DEF_ENABLED
#if APP_TASK_DISP_EN == DEF_ENABLED
/*******************************************************************************
 * CONSTANTS
 */
#define CYCLE_TIME_TICKS    (OS_CFG_TICK_RATE_HZ * 1u)

/*******************************************************************************
 * MACROS
 */

#define DISP_STARTUP			0
#define DISP_ID					1
#define DISP_HW_VERSION		2
#define DISP_SW_VERSION		3
#define DISP_PRIMARY_VOL		4
#define DISP_PRIMARY_CURRENT	5
#define DISP_DATE				6
#define DISP_TIME				7
#define DISP_ALL_CLEAR			8
#define DISP_NORMAL			9
#define DISP_LOCOTYPE			10
#define DISP_LOCONBR			11

/*******************************************************************************
 * TYPEDEFS
 */

typedef struct st_display_sequence
{
struct st_display_sequence *pNext;
const uint8 display_index;
}ST_DISPLAY_SEQUENCE;



/*******************************************************************************
 * LOCAL VARIABLES
 */
static INT32U  DispCycle           = CYCLE_TIME_TICKS;

static uint8 phase_display_index = 0;
static uint8 duty_ratio_display_index =0;
static uint8 dtc_display_index = 0;

#if ( OSAL_EN == DEF_ENABLED )
#else
/***********************************************
* ������ ������ƿ飨TCB��
*/
static  OS_TCB   AppTaskDispTCB;

/***********************************************
* ������ �����ջ��STACKS��
*/
static  CPU_STK  AppTaskDispStk[ APP_TASK_DISP_STK_SIZE ];

#endif

static ST_DISPLAY_SEQUENCE display_sequence_arry[DISPLAY_STEP_MAX] = 
{
{NULL,	DISPLAY_TIME},
{NULL,	DISPLAY_BAT_VOL},
{NULL,	DISPLAY_SPEED},
{NULL,	DISPLAY_PHASE_DIFF},
{NULL,	DISPLAY_DUTY_RATIO},
{NULL,	DISPLAY_DTC},
{NULL,	DISPLAY_HISTORY_NUMBER}
};

/*******************************************************************************
 * GLOBAL VARIABLES
 */

/*******************************************************************************
 * LOCAL FUNCTIONS
 */
#if ( OSAL_EN == DEF_ENABLED )
#else
static  void    AppTaskDisp           (void *p_arg);
#endif

/*******************************************************************************
 * GLOBAL FUNCTIONS
 */

/*******************************************************************************
 * EXTERN VARIABLES
 */
extern ST_para_storage system_para;
/*******************************************************************************/

/*******************************************************************************
 * ��    �ƣ� App_TaskDispCreate
 * ��    �ܣ� **���񴴽�
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ���� ���ߣ� wumingshen.
 * �������ڣ� 2015-02-05
 * ��    �ģ�
 * �޸����ڣ�
 * ��    ע�� ���񴴽�������Ҫ��app.h�ļ�������
 *******************************************************************************/
void  App_TaskDispCreate(void)
{

#if ( OSAL_EN == DEF_ENABLED )
#else
    OS_ERR  err;

    /***********************************************
    * ������ ���񴴽�
    */
    OSTaskCreate((OS_TCB     *)&AppTaskDispTCB,                 // ������ƿ�  ����ǰ�ļ��ж��壩
                 (CPU_CHAR   *)"App Task Disp",                 // ��������
                 (OS_TASK_PTR ) AppTaskDisp,                    // ������ָ�루��ǰ�ļ��ж��壩
                 (void       *) 0,                              // ����������
                 (OS_PRIO     ) APP_TASK_DISP_PRIO,             // �������ȼ�����ͬ�������ȼ�������ͬ��0 < ���ȼ� < OS_CFG_PRIO_MAX - 2��app_cfg.h�ж��壩
                 (CPU_STK    *)&AppTaskDispStk[0],              // ����ջ��
                 (CPU_STK_SIZE) APP_TASK_DISP_STK_SIZE / 10,    // ����ջ�������ֵ
                 (CPU_STK_SIZE) APP_TASK_DISP_STK_SIZE,         // ����ջ��С��CPU���ݿ�� * 8 * size = 4 * 8 * size(�ֽ�)����app_cfg.h�ж��壩
                 (OS_MSG_QTY  ) 5u,                             // ���Է��͸�����������Ϣ��������
                 (OS_TICK     ) 0u,                             // ��ͬ���ȼ��������ѭʱ�䣨ms����0ΪĬ��
                 (void       *) 0,                              // ��һ��ָ����������һ��TCB��չ�û��ṩ�Ĵ洢��λ��
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK |           // �����ջ��������
                                OS_OPT_TASK_STK_CLR),           // ��������ʱ��ջ����
                 (OS_ERR     *)&err);                           // ָ���������ָ�룬���ڴ����������
#endif
}
extern void APP_MaterDispHandler(INT08U *step);


const uint8 * dtc_display_str_table[MAX_DTC_NUM]=
{
"E-81    ",	//COLLECTOR_I_CONNECT_FAIL ,
"E-82    ",	//COLLECTOR_II_CONNECT_FAIL,
"E-83    ",	//COLLECTOR_III_CONNECT_FAIL,
"E-91    ",	//COLLECTOR_I_PHASE_ABNORMAL,
"E-92    ",	//COLLECTOR_II_PHASE_ABNORMAL,
"E-93    ",	//COLLECTOR_III_PHASE_ABNORMAL,
"E-71    ",	//SENSOR_I_IS_ABNORMAL,
"E-72    ",	//SENSOR_II_IS_ABNORMAL,
"E-73    ",	//SENSOR_III_IS_ABNORMAL,
"E-74    ",	//SENSOR_IV_IS_ABNORMAL,
"E-75    ",	//SENSOR_V_IS_ABNORMAL,
"E-76    ",	//SENSOR_VI_IS_ABNORMAL,
"E-31    ",	//BATTERY_VOLTAGE_LOW_FAULT,
"E-36    ",	//OUTPUT_STORAGE_ERROR,
"E-37    ",	//PARAMETER_STORAGE_ERROR,
};


/*******************************************************************************
 * ��    �ƣ� app_speed_display_handler
 * ��    �ܣ� ˢ����ʾ
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ� fourth.peng
 * �������ڣ� 2017-05-2
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void app_dtc_display_handler(uint8 *step)
{
	uint8 i;

	if (MAX_DTC_NUM != dtc.fault_list[dtc_display_index])
	{
		uprintf("%s",dtc_display_str_table[dtc.fault_list[dtc_display_index]]);
	}
}

/*******************************************************************************
 * ��    �ƣ� app_display_step_sequence
 * ��    �ܣ� ˢ�µ�ǰ��Ҫ��ʾ������
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ� fourth.peng
 * �������ڣ� 2017-05-3
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
 void app_display_step_sequence()
{
	uint8 i;
	ST_DISPLAY_SEQUENCE *p_primary_step = NULL;
	ST_DISPLAY_SEQUENCE *p_curr_step = &display_sequence_arry[0];
	DISPLAY_SETTING *p_display = system_para.display_setting;
	for ( i = 0; i < DISPLAY_STEP_MAX; i++)
	{
		if(DEF_ON == p_display[i].enable)
		{
			if (NULL == p_primary_step)
			{
				p_primary_step = &display_sequence_arry[i];
			}
			p_curr_step->pNext = &display_sequence_arry[i];
			p_curr_step = p_curr_step->pNext;
		}
	}
	p_curr_step->pNext = p_primary_step;
}
/*******************************************************************************
 * ��    �ƣ� app_speed_display_handler
 * ��    �ܣ� ˢ����ʾ
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ� fourth.peng
 * �������ڣ� 2017-05-3
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
 uint8 app_display_step_handler(uint8 step)
{
	uint8 next_step = step;
	DISPLAY_SETTING *p_next_display_setting = NULL;
	uint8 i = 0;

	next_step = display_sequence_arry[step].pNext->display_index;
	
	switch(step)
	{
		case DISPLAY_PHASE_DIFF:
			phase_display_index++;
			
/*			if (TRUE == Mater.monitor.abnormal_info[phase_display_index].connected_fail)
			{
				phase_display_index++;
			}
*/			
			if ( 3 <= phase_display_index )
			{
				phase_display_index = 0;
			}
			else
			{
				next_step = step;
			}
			break;
			
		case DISPLAY_DUTY_RATIO:
			duty_ratio_display_index++;

//		if (TRUE == Mater.monitor.abnormal_info[duty_ratio_display_index>>1].connected_fail)

			if ( 6 <= duty_ratio_display_index )
			{
				duty_ratio_display_index = 0;
			}
			else
			{
				next_step = step;
			}
			break;
			
		case DISPLAY_DTC:
			dtc_display_index++;
			dtc_display_index %= MAX_DTC_NUM;
			if (MAX_DTC_NUM == dtc.fault_list[dtc_display_index])
			{
				dtc_display_index = 0;	
			}
			else
			{
				next_step = step;
			}
			break;
			
		default:
		case DISPLAY_TIME:
		case DISPLAY_BAT_VOL:
		case DISPLAY_SPEED:
			break;
	}
	return next_step;
}


/*******************************************************************************
 * ��    �ƣ� app_speed_display_handler
 * ��    �ܣ� ˢ����ʾ
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ� fourth.peng
 * �������ڣ� 2017-05-2
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/

void app_display_handler()
{
        static uint8_t dispStep = DISPLAY_TIME; 
#ifndef SAMPLE_BOARD
	BSP_DispClrAll();
	switch(dispStep)
	{
		case DISPLAY_TIME:
		{
			uprintf("%02d-%02d-%02d",Mater.Time.Hour, Mater.Time.Min, Mater.Time.Sec);
			break;
		}

		case DISPLAY_BAT_VOL:
		{
			uprintf("V  %5d",Mater.monitor.voltage[ADC_CHANNEL_POWER]);
			break;
		}
		
		case DISPLAY_SPEED:
		{
			uprintf("S %5d.%1d", (uint32)Mater.monitor.real_speed, (uint32)(Mater.monitor.real_speed*10)%10);
			break;
		}

		case DISPLAY_PHASE_DIFF:
		{
			uprintf("p%d %4d", (phase_display_index+1), Mater.monitor.collector_board[phase_display_index].phase_diff);
			break;
		}

		case DISPLAY_DUTY_RATIO:
		{
			uprintf("ch%d %4d", (duty_ratio_display_index+1), Mater.monitor.collector_board[duty_ratio_display_index/2].channel[duty_ratio_display_index%2].duty_ratio);
			break;
		}

		case DISPLAY_DTC:
		{
			app_dtc_display_handler(&dispStep);
			break;
		}

		case DISPLAY_HISTORY_NUMBER:
		{
			uprintf("H %6d",Mater.RecordNbr);
			break;
		}
		default:
		{
			dispStep = DISPLAY_TIME;
			break;
		}
	}
	DispCycle = system_para.display_setting[dispStep].times;
	dispStep = app_display_step_handler(dispStep);
#endif
}

/*******************************************************************************
 * ��    �ƣ� AppTaskDisp
 * ��    �ܣ� ��������
 * ��ڲ����� p_arg - �����񴴽���������
 * ���ڲ����� ��
 * ���� ���ߣ� wumingshen.
 * �������ڣ� 2015-02-05
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/

#if ( OSAL_EN == DEF_ENABLED )
osalEvt  TaskDispEvtProcess(INT8U task_id, osalEvt task_event)
#else
static  void  AppTaskDisp (void *p_arg)
#endif
{
    OS_ERR      err;
    INT32U      ticks;
    INT32S      dly;
    //CPU_SR_ALLOC();
    
    /***********************************************
    * ������Task body, always written as an infinite loop.
    */
#if ( OSAL_EN == DEF_ENABLED )
#else
    TaskInitDisp();
    
    while (DEF_TRUE) {
#endif
        /***********************************************
        * ������ �������Ź���־��λ
        */
        OS_FlagPost ((OS_FLAG_GRP *)&WdtFlagGRP,
                     (OS_FLAGS     ) WDT_FLAG_DISP,
                     (OS_OPT       ) OS_OPT_POST_FLAG_SET,
                     (CPU_TS       ) 0,
                     (OS_ERR      *) &err);
        /***********************************************
        * ������ �õ�ϵͳ��ǰʱ��
        */
        ticks = OSTimeGet(&err);
        
#if ( OSAL_EN == DEF_ENABLED )
        if( task_event & OS_EVT_DISP_TICKS ) {
#else
#endif
            static INT8U       step    = 0;
           // static INT16S      *pValue;
           //         int         DispValue ;
            
            /***********************************************
            * ������ ����ʾ
            */
            BSP_DispClrAll();
                
            /***********************************************
            * ������ ������ʾģʽ��ʾ
            */
            switch(Ctrl.Disp.Mode) {
            /*******************************************************************
            * ������ ��ʾ��������
            *        ʱ�䣺������ʾһ�Σ���ʾ2s��
            *        ���ݣ���SF-X����
            *        ���壺X=S����������X=A��Ӧ�ó���A��X=B��Ӧ�ó���B
            */
            case DISP_STARTUP: 
                Ctrl.Disp.Mode++;
                BSP_DispClrAll(); 
#if defined     (IMAGE_A)
                /***********************************************
                * ������ ����A
                */
#if defined ( DISPLAY )
                /***********************************************
                * ������ �������ʾģ��
                */
                uprintf("    SD-A");
#endif
                uprintf("    SF-A");
#elif defined   (IMAGE_B)
                /***********************************************
                * ������ ����B
                */
#if defined ( DISPLAY )
                /***********************************************
                * ������ �������ʾģ��
                */
                uprintf("    SD-B");
#endif
                uprintf("    SF-B");
#else
                /***********************************************
                * ������ ��׼����
                */
#if defined ( DISPLAY )
                /***********************************************
                * ������ �������ʾģ��
                */
                uprintf("    SD-S");
#endif
                uprintf("    SF-S");
#endif
                break;
            /*******************************************************************
            * ������ �����ͺ�
            */
	    case DISP_LOCOTYPE:
			Ctrl.Disp.Mode = DISP_LOCONBR;
	                uprintf("%8d", Mater.LocoTyp);
			break;
            /*******************************************************************
            * ������ ������
            */
	    case DISP_LOCONBR:
			Ctrl.Disp.Mode = DISP_ID;
	                uprintf("%8d", Mater.LocoNbr);
			break;

            /*******************************************************************
            * ������ ��ʾ�豸ID
            */
            case DISP_ID: 
                Ctrl.Disp.Mode = DISP_DATE;
                uprintf("%8d", Ctrl.ID);
                break;
#if 0
            /*******************************************************************
            * ������ ��ʾӲ���汾
            *        ʱ�䣺������ʾһ�Σ���ʾ2s��
            *        ���ݣ����ݣ���HV21����
            *        ���壺HVΪӲ��21Ϊ�汾�š�
            */
            case DISP_HW_VERSION: 
                Ctrl.Disp.Mode++;
                uprintf("    HV12");
                
                break;
            /*******************************************************************
            * ������ ��ʾ����汾
            *        ʱ�䣺������ʾһ�Σ���ʾ2s��
            *        ���ݣ����ݣ���SV25�� 
            *        ���壺SVΪ�����25Ϊ�汾�š�
            */
            case DISP_SW_VERSION: 
//                Ctrl.Disp.Mode++;
		  Ctrl.Disp.Mode = DISP_ALL_CLEAR;
                uprintf("    SV20");
                
                break;
            /*******************************************************************
            * ������ 
            */
            case DISP_PRIMARY_VOL: 
                Ctrl.Disp.Mode++;
//                uprintf("%7dU", AC.U_SCL);
                
                break;
            /*******************************************************************
            * ������ 
            */
            case DISP_PRIMARY_CURRENT: 
                Ctrl.Disp.Mode++;
//                uprintf("%7dA", AC.I_SCL);
                
                break;
#endif
            /*******************************************************************
            * ������ ��ʾ�߶Ȳ�
            *        ʱ�䣺������ʾһ�Σ���ʾ2s��
            *        ���ݣ���XX.Xd����
            *        ���壺XX.XΪ�߶�ֵ����λ.1mm��
            */
            case DISP_DATE: 
                Ctrl.Disp.Mode = DISP_ALL_CLEAR;
                osal_start_timerEx( OS_TASK_ID_DISP,
                                    OS_EVT_DISP_TICKS,
                                    CYCLE_TIME_TICKS*2);
                /***********************************************
                * ������ 
                */
                uprintf("%02d-%02d-%02d",Mater.Time.Year, Mater.Time.Mon, Mater.Time.Day);
                //BSP_DispWrite((int)DispValue,"%D",RIGHT,0,(1<<2),1,20,0);
                
                break;
            /*******************************************************************
            * ������ ���ϴ�����ʾ
            *        ʱ�䣺����ʱ��ʾ���й���ʱ����ʾģ��Ϊѭ����ʾʱ4sˢ��һ�Σ�
            *        ���ݣ���E-XX����
            *        ���壺XX���ϴ��롣
            */
            case DISP_TIME:  
                //DispCycle   = CYCLE_TIME_TICKS * 2;
                Ctrl.Disp.Mode++;
                /***********************************************
                * ������ �˶�ʱ��Ӧ����Ч
                */
                osal_start_timerEx( OS_TASK_ID_DISP,
                                    OS_EVT_DISP_TICKS,
                                    CYCLE_TIME_TICKS*2);
                //uprintf("    E-%02X",DispValue);
                uprintf("%02d-%02d-%02d",Mater.Time.Hour, Mater.Time.Min, Mater.Time.Sec);
                break;
            case DISP_ALL_CLEAR:  
                Ctrl.Disp.Mode++;
                /***********************************************
                * ������ 
                */
                osal_start_timerEx( OS_TASK_ID_DISP,
                                    OS_EVT_DISP_TICKS,
                                    CYCLE_TIME_TICKS);
                uprintf("        ");
                break;
            /*******************************************************************
            * ������ �������߶ȡ����ϴ�����ʾ
            */
            case DISP_NORMAL: {
                //INT08U  err = 0;
                //INT08U  dot = 0;
#if 1
		app_display_handler();
//		DispCycle   = CYCLE_TIME_TICKS * 2;
#else
                if ( ( dispStep < 8 ) && ( dispStep % 2 == 0 ) ) {
                    DispCycle   = CYCLE_TIME_TICKS;
                } else {
                    DispCycle   = CYCLE_TIME_TICKS * 2;
                }
                /***********************************************
                * ������ 
                */
                APP_MaterDispHandler(&dispStep);
                //BSP_DispClrAll();
                /***********************************************
                * ������ 
                *
                if ( err ) {
                    uprintf("E-%02X",DispValue);
                } else {
                    CPU_CRITICAL_ENTER();
                    DispValue  = *pValue;
                    CPU_CRITICAL_EXIT();
                    
                    if ( DispValue > 9999 ) {
                        DispValue   /= 10;
                    }
                    BSP_DispWrite((int)DispValue,"%",RIGHT,0,(dot<<1),1,20,0);
                }
                
                *//***********************************************
                * ������ 
                */  
                if (++step >= 4)
                    step    = 0;
#endif
            } break;
            default:
                Ctrl.Disp.Mode  = 0;
                break;
            }
            
            /***********************************************
            * ������ ��ʾ�������ؽ���
            */
            if ( Iap.Status == IAP_STS_PROGRAMING ) { 
                uprintf("    u%03d",Iap.FrameIdx);
                //BSP_DispSetBrightness(Ctrl.Para.dat.DispLevel);
                BSP_DispEvtProcess();	// �Ƿ����
            } else {
            
            }
            
            /*******************************************************************
            * ������ ������ʾ����
            */
            BSP_DispSetBrightness(Ctrl.Para.dat.DispLevel);
            //BSP_DispEvtProcess();
        //exit:
            /***********************************************
            * ������ ȥ���������е�ʱ�䣬�ȵ�һ������������ʣ����Ҫ��ʱ��ʱ��
            */            
            if ( Iap.Status == IAP_STS_PROGRAMING ) { 
                dly   = OS_CFG_TICK_RATE_HZ / 5 - ( OSTimeGet(&err) - ticks );
            } else {
                dly   = DispCycle - ( OSTimeGet(&err) - ticks );            
            }
                
            if ( dly  <= 0 ) {
                dly   = 1;
            }
#if ( OSAL_EN == DEF_ENABLED )
            osal_start_timerEx( OS_TASK_ID_DISP,
                                OS_EVT_DISP_TICKS,
                                dly);
            
            return ( task_event ^ OS_EVT_SEN_SAMPLE );
        }
#endif
        
#if ( OSAL_EN == DEF_ENABLED )
        return 0;
#else
        BSP_OS_TimeDly(dly);
    }
#endif
}

/*******************************************************************************
 * ��    �ƣ� APP_DispInit
 * ��    �ܣ� �����ʼ��
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��  ���ߣ� wumingshen.
 * �������ڣ� 2015-12-08
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void TaskInitDisp(void)
{
    /***********************************************
    * ������ ��ʼ���������õ������Ӳ��
    */
    BSP_DispInit();
    Ctrl.Disp.Led   = (StrLedDisp  *)&LedDispCtrl;
    Ctrl.Disp.Mode  = DISP_LOCOTYPE;
    BSP_DispOff();
    BSP_DispSetBrightness(Ctrl.Para.dat.DispLevel);
    BSP_DispClrAll();
    uprintf("        ");
    
    //for(char i = 0; i < 10; i++) {
    //    uprintf("%08d",i*11111111);
    //    //BSP_DispEvtProcess();
    //    BSP_OS_TimeDly(OS_TICKS_PER_SEC/4);
    //}
    
    uprintf("8.8.8.8.8.8.8.8.");
    BSP_OS_TimeDly(OS_TICKS_PER_SEC);
    uprintf("        ");
    BSP_OS_TimeDly(OS_TICKS_PER_SEC/2);
    /***********************************************
    * ������ �ڿ��Ź���־��ע�᱾����Ŀ��Ź���־
    */
    WdtFlags |= WDT_FLAG_DISP;
    /*************************************************
    * �����������¼���ѯ
    */
#if ( OSAL_EN == DEF_ENABLED )
#if defined     (IMAGE_A)
    osal_start_timerEx( OS_TASK_ID_DISP,
                        OS_EVT_DISP_TICKS,
                        1);
#elif defined   (IMAGE_B)
    osal_start_timerEx( OS_TASK_ID_DISP,
                        OS_EVT_DISP_TICKS,
                        1);
#else
    osal_start_timerEx( OS_TASK_ID_DISP,
                        OS_EVT_DISP_TICKS,
                        CYCLE_TIME_TICKS);
#endif
#else
#endif
}

/*******************************************************************************
 * 				                    end of file                                *
 *******************************************************************************/
#endif
