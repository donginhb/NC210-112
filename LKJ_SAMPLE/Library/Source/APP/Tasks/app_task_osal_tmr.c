/*******************************************************************************
 *   Filename:       app_task_tmr.c
 *   Revised:        All copyrights reserved to wumingshen.
 *   Revision:       v1.0
 *   Writer:	     wumingshen.
 *
 *   Description:    ˫��ѡ�� tmr �� Ctrl + H, ��ѡ Match the case, Replace with
 *                   ������Ҫ�����֣���� Replace All
 *                   ˫��ѡ�� Tmr �� Ctrl + H, ��ѡ Match the case, Replace with
 *                   ������Ҫ�����֣���� Replace All
 *                   ˫��ѡ�� TMR �� Ctrl + H, ��ѡ Match the case, Replace with
 *                   ������Ҫ�����֣���� Replace All
 *                   �� app_cfg.h ��ָ��������� ���ȼ�  �� APP_TASK_TMR_PRIO ��
 *                                            �� �����ջ�� APP_TASK_TMR_STK_SIZE ����С
 *                   �� app.h �������������     ���������� void  App_TaskTmrCreate(void) ��
 *                                            �� ���Ź���־λ �� WDTFLAG_Tmr ��
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
#include "DS3231.h"
#ifdef PHOTOELECTRIC_VELOCITY_MEASUREMENT
#include <Speed_sensor.h>
#include "app_task_mater.h"
#else
#include <power_macro.h>
#endif

#ifdef VSC_INCLUDE_SOURCE_FILE_NAMES
const  CPU_CHAR  *app_task_tmr__c = "$Id: $";
#endif

#define APP_TASK_TMR_EN     DEF_ENABLED
#if APP_TASK_TMR_EN == DEF_ENABLED
/*******************************************************************************
 * CONSTANTS
 */
#define CYCLE_TIME_TICKS            (OS_CFG_TICK_RATE_HZ * 1u)
#define CYCLE_SAMPLE_MSEC_TICKS     (OS_CFG_TICK_RATE_HZ / 5)
#define CYCLE_SAMPLE_SEC_TICKS      (OS_CFG_TICK_RATE_HZ * 5u)
#define CYCLE_SAMPLE_MIN_TICKS      (OS_CFG_TICK_RATE_HZ * 60u)
#define CYCLE_SAMPLE_TICKS          (OS_CFG_TICK_RATE_HZ * 2u)
     
/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */

#if ( OSAL_EN == DEF_ENABLED )
#else
/***********************************************
* ������ ������ƿ飨TCB��
*/
static  OS_TCB   AppTaskTmrTCB;

/***********************************************
* ������ �����ջ��STACKS��
*/
static  CPU_STK  AppTaskTmrStk[ APP_TASK_TMR_STK_SIZE ];

#endif
/*******************************************************************************
 * GLOBAL VARIABLES
 */

/*******************************************************************************
 * LOCAL FUNCTIONS
 */

/*******************************************************************************
 * GLOBAL FUNCTIONS
 */

/*******************************************************************************
 * EXTERN VARIABLES
 */

/*******************************************************************************/

/*******************************************************************************
 * ��    �ƣ� AppTaskTmr
 * ��    �ܣ� ��������
 * ��ڲ����� p_arg - �����񴴽���������
 * ���ڲ����� ��
 * ���� ���ߣ� wumingshen.
 * �������ڣ� 2015-02-05
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/

osalEvt  TaskTmrEvtProcess(INT8U task_id, osalEvt task_event)
{
    OS_ERR      err;
    static BOOL     chang_flag = FALSE;
    
    /***********************************************
    * ������ �������Ź���־��λ
    */
    OS_FlagPost ((OS_FLAG_GRP *)&WdtFlagGRP,
                 (OS_FLAGS     ) WDT_FLAG_TMR,
                 (OS_OPT       ) OS_OPT_POST_FLAG_SET,
                 (CPU_TS       ) 0,
                 (OS_ERR      *) &err);    
  
    /***************************************************************************
    * ������ 1����һ��
    */
    if( task_event & OS_EVT_TMR_TICKS ) {     
        /***************************************************
        * ������ ��ȡʱ��
        */
        uint8_t time[6];   
        GetTime((TIME *)&time[0]);
        
        CPU_SR  cpu_sr;
        OS_CRITICAL_ENTER();
        Mater.Time.Year  = time[0];
        Mater.Time.Mon   = time[1];
        Mater.Time.Day   = time[2];
        Mater.Time.Hour  = time[3];
        Mater.Time.Min   = time[4];
        Mater.Time.Sec   = time[5];
        OS_CRITICAL_EXIT();
        /***************************************************
        * ������ ��λ�����������ݱ�־λ�������������ݱ���
        */
        OS_FlagPost(( OS_FLAG_GRP *)&Ctrl.Os.MaterEvtFlagGrp,
                    ( OS_FLAGS     ) COMM_EVT_FLAG_HEART,
                    ( OS_OPT       ) OS_OPT_POST_FLAG_SET,
                    ( CPU_TS       ) 0,
                    ( OS_ERR      *) &err);
        return ( task_event ^ OS_EVT_TMR_TICKS );
    }
    
    /***************************************************************************
    * ������ 1����һ��
    */
    if( task_event & OS_EVT_TMR_MIN ) { 
        /***************************************************
        * ������ ��λ�������ݱ�־λ���������ݱ���
        */
        OS_FlagPost(( OS_FLAG_GRP *)&Ctrl.Os.DtuEvtFlagGrp,
                    ( OS_FLAGS     ) COMM_EVT_FLAG_REPORT,
                    ( OS_OPT       ) OS_OPT_POST_FLAG_SET,
                    ( CPU_TS       ) 0,
                    ( OS_ERR      *) &err);
        
        return ( task_event ^ OS_EVT_TMR_MIN );
    }
    
    /***************************************************************************
    * ������ 1����һ��
    */
    if( task_event & OS_EVT_TMR_DEAL ) {        
#if 0
        Ctrl.PFreq          = (AC.PPpulse  - Ctrl.PPpulseLast)/60.0;
        Ctrl.PPpulseLast    = AC.PPpulse;
#endif        
        osal_start_timerRl( OS_TASK_ID_TMR, OS_EVT_TMR_MIN ,(osalTime)Mater.RecordTime);
                
        OS_FlagPost(( OS_FLAG_GRP *)&Ctrl.Os.MaterEvtFlagGrp,
                    ( OS_FLAGS     ) COMM_EVT_FLAG_CONFIG,
                    ( OS_OPT       ) OS_OPT_POST_FLAG_SET,
                    ( CPU_TS       ) 0,
                    ( OS_ERR      *) &err);
        return ( task_event ^ OS_EVT_TMR_DEAL );
    }
    return 0;
}

/*******************************************************************************
 * ��    �ƣ� APP_TmrInit
 * ��    �ܣ� �����ʼ��
 * ��ڲ����� ��
 * ���ڲ����� ��
 * �������ߣ� wumingshen.
 * �������ڣ� 2015-03-28
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void TaskInitTmr(void)
{
    /***********************************************
    * ������ ��ʼ��
    */    
       
    /***********************************************
    * ������ �ڿ��Ź���־��ע�᱾����Ŀ��Ź���־
    */
    WdtFlags |= WDT_FLAG_TMR;
    /*************************************************
    * �����������¼���ѯ
    */
    osal_start_timerRl( OS_TASK_ID_TMR, OS_EVT_TMR_TICKS, CYCLE_TIME_TICKS);
    osal_start_timerRl( OS_TASK_ID_TMR, OS_EVT_TMR_MIN ,  OS_TICKS_PER_SEC * 10);
    osal_start_timerRl( OS_TASK_ID_TMR, OS_EVT_TMR_DEAL , OS_TICKS_PER_SEC * 60);
}

/*******************************************************************************
 * 				                    end of file                                *
 *******************************************************************************/
#endif
