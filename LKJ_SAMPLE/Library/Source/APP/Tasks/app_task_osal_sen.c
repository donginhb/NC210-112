/*******************************************************************************
 *   Filename:       app_task_sen.c
 *   Revised:        All copyrights reserved to wumingshen.
 *   Revision:       v1.0
 *   Writer:	     wumingshen.
 *
 *   Description:    ˫��ѡ�� sen �� Ctrl + H, ��ѡ Match the case, Replace with
 *                   ������Ҫ�����֣���� Replace All
 *                   ˫��ѡ�� Sen �� Ctrl + H, ��ѡ Match the case, Replace with
 *                   ������Ҫ�����֣���� Replace All
 *                   ˫��ѡ�� SEN �� Ctrl + H, ��ѡ Match the case, Replace with
 *                   ������Ҫ�����֣���� Replace All
 *                   �� app_cfg.h ��ָ��������� ���ȼ�  �� APP_TASK_SEN_PRIO ��
 *                                            �� �����ջ�� APP_TASK_SEN_STK_SIZE ����С
 *                   �� app.h �������������     ���������� void  App_TaskSenCreate(void) ��
 *                                            �� ���Ź���־λ �� WDTFLAG_Sen ��
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

#ifdef VSC_INCLUDE_SOURCE_FILE_NAMES
const  CPU_CHAR  *app_task_sen__c = "$Id: $";
#endif

#define APP_TASK_SEN_EN     DEF_ENABLED
#if APP_TASK_SEN_EN == DEF_ENABLED
/*******************************************************************************
 * CONSTANTS
 */
#define CYCLE_TIME_TICKS            (OS_CFG_TICK_RATE_HZ * 1u)
#define CYCLE_SAMPLE_MSEC_TICKS     (OS_CFG_TICK_RATE_HZ / ADC_SAMPLE_CNTS_PER_SEC/2)
#define CYCLE_SAMPLE_SEC_TICKS      (OS_CFG_TICK_RATE_HZ * 5u)
#define CYCLE_SAMPLE_MIN_TICKS      (OS_CFG_TICK_RATE_HZ * 60u)
#define CYCLE_SAMPLE_TICKS          (OS_CFG_TICK_RATE_HZ * 2u)
#define CYCLE_UPDATE_DENSITY        (OS_CFG_TICK_RATE_HZ * 3u*60u)
#define CYCLE_UPDATE_TICKS          (OS_CFG_TICK_RATE_HZ * 30u)                 // �ܶȼ������

#define STOP_UPDATE_TICKS_CNT       (5 * 60 / (CYCLE_UPDATE_TICKS / OS_CFG_TICK_RATE_HZ))   // ͣ�����ʱ����

#define CYCLE_UPDATE_TICKS_CNT      (30 * 60 * CYCLE_UPDATE_TICKS / CYCLE_UPDATE_TICKS)     // ��ֹ���ʱ����

#define CYCLE_SEN_ERR_CHK           (OS_CFG_TICK_RATE_HZ * 2u)                  // ���������ϼ�⴫����
     
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
static  OS_TCB   AppTaskSenTCB;

/***********************************************
* ������ �����ջ��STACKS��
*/
static  CPU_STK  AppTaskSenStk[ APP_TASK_SEN_STK_SIZE ];

#endif
/*******************************************************************************
 * GLOBAL VARIABLES
 */

/*******************************************************************************
 * LOCAL FUNCTIONS
 */
#if ( OSAL_EN == DEF_ENABLED )
#else
static  void    AppTaskSen           (void *p_arg);
#endif

float           App_fParaFilter     (float para, float def, float min, float max);
long            App_lParaFilter     (long para, long def, long min, long max);

void            APP_ParaInit        (void);

/*******************************************************************************
 * GLOBAL FUNCTIONS
 */

/*******************************************************************************
 * EXTERN VARIABLES
 */

/*******************************************************************************/

/*******************************************************************************
 * ��    �ƣ� App_TaskSenCreate
 * ��    �ܣ� **���񴴽�
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ���� ���ߣ� wumingshen.
 * �������ڣ� 2015-02-05
 * ��    �ģ�
 * �޸����ڣ�
 * ��    ע�� ���񴴽�������Ҫ��app.h�ļ�������
 *******************************************************************************/
void  App_TaskSenCreate(void)
{

#if ( OSAL_EN == DEF_ENABLED )
#else
    OS_ERR  err;

    /***********************************************
    * ������ ���񴴽�
    */
    OSTaskCreate((OS_TCB     *)&AppTaskSenTCB,                  // ������ƿ�  ����ǰ�ļ��ж��壩
                 (CPU_CHAR   *)"App Task Sen",                  // ��������
                 (OS_TASK_PTR ) AppTaskSen,                     // ������ָ�루��ǰ�ļ��ж��壩
                 (void       *) 0,                              // ����������
                 (OS_PRIO     ) APP_TASK_SEN_PRIO,              // �������ȼ�����ͬ�������ȼ�������ͬ��0 < ���ȼ� < OS_CFG_PRIO_MAX - 2��app_cfg.h�ж��壩
                 (CPU_STK    *)&AppTaskSenStk[0],               // ����ջ��
                 (CPU_STK_SIZE) APP_TASK_SEN_STK_SIZE / 10,     // ����ջ�������ֵ
                 (CPU_STK_SIZE) APP_TASK_SEN_STK_SIZE,          // ����ջ��С��CPU���ݿ�� * 8 * size = 4 * 8 * size(�ֽ�)����app_cfg.h�ж��壩
                 (OS_MSG_QTY  ) 5u,                             // ���Է��͸�����������Ϣ��������
                 (OS_TICK     ) 0u,                             // ��ͬ���ȼ��������ѭʱ�䣨ms����0ΪĬ��
                 (void       *) 0,                              // ��һ��ָ����������һ��TCB��չ�û��ṩ�Ĵ洢��λ��
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK |           // �����ջ��������
                                OS_OPT_TASK_STK_CLR),           // ��������ʱ��ջ����
                 (OS_ERR     *)&err);                           // ָ���������ָ�룬���ڴ����������
#endif
}

/*******************************************************************************
 * ��    �ƣ� AppTaskSen
 * ��    �ܣ� ��������
 * ��ڲ����� p_arg - �����񴴽���������
 * ���ڲ����� ��
 * ���� ���ߣ� wumingshen.
 * �������ڣ� 2015-02-05
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/

#if ( OSAL_EN == DEF_ENABLED )
osalEvt  TaskSenEvtProcess(INT8U task_id, osalEvt task_event)
#else
static  void  AppTaskSen (void *p_arg)
#endif
{
    OS_ERR      err;
    static BOOL     chang_flag = FALSE;
    
    /***********************************************
    * ������ �������Ź���־��λ
    */
    OS_FlagPost ((OS_FLAG_GRP *)&WdtFlagGRP,
                 (OS_FLAGS     ) WDT_FLAG_SEN,
                 (OS_OPT       ) OS_OPT_POST_FLAG_SET,
                 (CPU_TS       ) 0,
                 (OS_ERR      *) &err);    
  
    /***************************************************************************
    * ������ 1����һ��
    */
    if( task_event & OS_EVT_SEN_SEC ) {
        
        return ( task_event ^ OS_EVT_SEN_SEC );
    }
    
    /***************************************************************************
    * ������ 1����һ��
    */
    if( task_event & OS_EVT_SEN_MIN ) {        
        OS_FlagPost(( OS_FLAG_GRP *)&Ctrl.Os.MaterEvtFlagGrp,
                    ( OS_FLAGS     ) COMM_EVT_FLAG_REPORT,
                    ( OS_OPT       ) OS_OPT_POST_FLAG_SET,
                    ( CPU_TS       ) 0,
                    ( OS_ERR      *) &err);
        return ( task_event ^ OS_EVT_SEN_MIN );
    }
    
    return 0;
}

/*******************************************************************************
 * ��    �ƣ� App_fParaFilter
 * ��    �ܣ� �����������
 * ��ڲ����� ��
 * ���ڲ����� ��
 * �������ߣ� wumingshen.
 * �������ڣ� 2016-04-20
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
float App_fParaFilter(float para, float def, float min, float max)
{
    if ( para < min ) 
        para   = def;
    else if ( para > max )
        para   = def;
    else if ( (INT16U)para == 0XFFFF ) {
        para   = def;
    }
    
    return para;
}

/*******************************************************************************
 * ��    �ƣ� App_lParaFilter
 * ��    �ܣ� ���Ͳ�������
 * ��ڲ����� ��
 * ���ڲ����� ��
 * �������ߣ� wumingshen.
 * �������ڣ� 2016-04-20
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
long App_lParaFilter(long para, long def, long min, long max)
{
    if ( para < min ) 
        para   = def;
    else if ( para > max )
        para   = def; 
    
    return para;
}

/*******************************************************************************
 * ��    �ƣ� APP_ParaInit
 * ��    �ܣ� Ӧ�ò�����ʼ��
 * ��ڲ����� ��
 * ���ڲ����� ��
 * �������ߣ� wumingshen.
 * �������ڣ� 2016-06-03
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void APP_ParaInit(void)
{
    
}

/*******************************************************************************
 * ��    �ƣ� APP_SenInit
 * ��    �ܣ� �����ʼ��
 * ��ڲ����� ��
 * ���ڲ����� ��
 * �������ߣ� wumingshen.
 * �������ڣ� 2015-03-28
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void TaskInitSen(void)
{
    /***********************************************
    * ������ ��ʼ��ADC
    */
    //BSP_ADCInit();
    
       
    /***********************************************
    * ������ ��ʼ������
    */
    //APP_ParaInit();
    
    /***********************************************
    * ������ �ڿ��Ź���־��ע�᱾����Ŀ��Ź���־
    */
    //WdtFlags |= WDT_FLAG_SEN;
    /*************************************************
    * �����������¼���ѯ
    */
#if ( OSAL_EN == DEF_ENABLED )
    //osal_start_timerRl( OS_TASK_ID_SEN, OS_EVT_SEN_TICKS,           CYCLE_TIME_TICKS);
    //osal_start_timerRl( OS_TASK_ID_SEN, OS_EVT_SEN_MSEC ,           CYCLE_SAMPLE_MSEC_TICKS);
    osal_start_timerRl( OS_TASK_ID_SEN, OS_EVT_SEN_MIN ,           60*OS_TICKS_PER_SEC);
    //osal_start_timerEx( OS_TASK_ID_SEN, OS_EVT_SEN_UPDATE_DENSITY , CYCLE_UPDATE_DENSITY);  
    //osal_start_timerRl( OS_TASK_ID_SEN, OS_EVT_SEN_ERR_CHK ,        CYCLE_SEN_ERR_CHK);  
#else
#endif
}

/*******************************************************************************
 * 				                    end of file                                *
 *******************************************************************************/
#endif