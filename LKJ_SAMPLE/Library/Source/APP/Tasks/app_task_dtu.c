/*******************************************************************************
 *   Filename:       app_task_dtu.c
 *   Revised:        All copyrights reserved to wumingshen.
 *   Revision:       v1.0
 *   Writer:	     wumingshen.
 *
 *   Description:    ˫��ѡ�� dtu �� Ctrl + H, ��ѡ Match the case, Replace with
 *                   ������Ҫ�����֣���� Replace All
 *                   ˫��ѡ�� Dtu �� Ctrl + H, ��ѡ Match the case, Replace with
 *                   ������Ҫ�����֣���� Replace All
 *                   ˫��ѡ�� DTU �� Ctrl + H, ��ѡ Match the case, Replace with
 *                   ������Ҫ�����֣���� Replace All
 *                   �� app_cfg.h ��ָ��������� ���ȼ�  �� APP_TASK_DTU_PRIO     ��
 *                                            �� �����ջ�� APP_TASK_DTU_STK_SIZE ����С
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
#ifdef PHOTOELECTRIC_VELOCITY_MEASUREMENT
#include <Speed_sensor.h>
#include "app_task_mater.h"
#else
#include <power_macro.h>
#endif
#include <mx25.h>
#include <FM24CL64.h>
#include <crccheck.h>

#ifdef VSC_INCLUDE_SOURCE_FILE_NAMES
const  CPU_CHAR  *app_task_dtu__c = "$Id: $";
#endif
#ifdef SAMPLE_BOARD
#define APP_TASK_DTU_EN     DEF_DISABLED
#else
#define APP_TASK_DTU_EN     DEF_ENABLED
#endif
OS_Q                DTU_RxQ;

#if APP_TASK_DTU_EN == DEF_ENABLED
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

/***********************************************
* ������ ������ƿ飨TCB��
*/
static  OS_TCB      AppTaskDtuTCB;

/***********************************************
* ������ �����ջ��STACKS��
*/
static  CPU_STK  AppTaskDtuStk[ APP_TASK_DTU_STK_SIZE ];

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */

/*******************************************************************************
 * LOCAL FUNCTIONS
 */
static void    AppTaskDtu                 (void *p_arg);
static void    APP_DtuInit                (void);
       void     App_McuStatusInit          (void);
       void     ReportDevStatusHandle      (void);
       void     InformDtuConfigMode        (u8 mode);
       
       void     App_SendDataFromHistory     (void);

INT08U          APP_DtuRxDataDealCB        (MODBUS_CH  *pch);
INT08U          IAP_DtuRxDataDealCB        (MODBUS_CH  *pch);
/*******************************************************************************
 * GLOBAL FUNCTIONS
 */

/*******************************************************************************
 * EXTERN VARIABLES
 */

 /*******************************************************************************
 * EXTERN FUNCTIONS
 */
extern void     uartprintf              (MODBUS_CH  *pch,const char *fmt, ...);

/*******************************************************************************/

/*******************************************************************************
 * ��    �ƣ� App_TaskDtuCreate
 * ��    �ܣ� **���񴴽�
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ� wumingshen.
 * �������ڣ� 2015-02-05
 * ��    �ģ�
 * �޸����ڣ�
 * ��    ע�� ���񴴽�������Ҫ��app.h�ļ�������
 *******************************************************************************/
void  App_TaskDtuCreate(void)
{
    OS_ERR  err;

    /***********************************************
    * ������ ���񴴽�
    */
    OSTaskCreate((OS_TCB     *)&AppTaskDtuTCB,                      // ������ƿ�  ����ǰ�ļ��ж��壩
                 (CPU_CHAR   *)"App Task Dtu",                      // ��������
                 (OS_TASK_PTR ) AppTaskDtu,                         // ������ָ�루��ǰ�ļ��ж��壩
                 (void       *) 0,                                  // ����������
                 (OS_PRIO     ) APP_TASK_DTU_PRIO,                 // �������ȼ�����ͬ�������ȼ�������ͬ��0 < ���ȼ� < OS_CFG_PRIO_MAX - 2��app_cfg.h�ж��壩
                 (CPU_STK    *)&AppTaskDtuStk[0],                   // ����ջ��
                 (CPU_STK_SIZE) APP_TASK_DTU_STK_SIZE / 10,         // ����ջ�������ֵ
                 (CPU_STK_SIZE) APP_TASK_DTU_STK_SIZE,              // ����ջ��С��CPU���ݿ�� * 8 * size = 4 * 8 * size(�ֽ�)����app_cfg.h�ж��壩
                 (OS_MSG_QTY  ) 0u,                                 // ���Է��͸�����������Ϣ��������
                 (OS_TICK     ) 0u,                                 // ��ͬ���ȼ��������ѭʱ�䣨ms����0ΪĬ��
                 (void       *) 0,                                  // ��һ��ָ����������һ��TCB��չ�û��ṩ�Ĵ洢��λ��
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK |               // �����ջ��������
                                OS_OPT_TASK_STK_CLR),               // ��������ʱ��ջ����
                 (OS_ERR     *)&err);                               // ָ���������ָ�룬���ڴ����������

}

/*******************************************************************************
 * ��    �ƣ� AppTaskDtu
 * ��    �ܣ� ��������
 * ��ڲ����� p_arg - �����񴴽���������
 * ���ڲ����� ��
 * ��    �ߣ� wumingshen.
 * �������ڣ� 2015-02-05
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
static  void  AppTaskDtu (void *p_arg)
{
    OS_ERR      err;

    OS_TICK     dly     = CYCLE_TIME_TICKS;
    OS_TICK     ticks;
        
    APP_DtuInit();
    
    BSP_OS_TimeDly(OS_TICKS_PER_SEC / 2);
    /***********************************************
    * ������ Task body, always written as an infinite loop.
    */
    while (DEF_TRUE) { 
        /***********************************************
        * ������ �������Ź���־��λ
        */
        OS_FlagPost(( OS_FLAG_GRP *)&WdtFlagGRP,
                    ( OS_FLAGS     ) WDT_FLAG_DTU,
                    ( OS_OPT       ) OS_OPT_POST_FLAG_SET,
                    ( CPU_TS       ) 0,
                    ( OS_ERR      *) &err);
        
        /***********************************************
        * ������ �ȴ�DTU���ݽ�����Ϣ����
        *
        OS_MSG_SIZE p_msg_size;
        
        MODBUS_CH *pch = 
       (MODBUS_CH *)OSQPend ((OS_Q*)&DTU_RxQ,
                    (OS_TICK       )dly,
                    (OS_OPT        )OS_OPT_PEND_BLOCKING,//OS_OPT_PEND_NON_BLOCKING,
                    (OS_MSG_SIZE  *)&p_msg_size,
                    (CPU_TS       *)0,
                    (OS_ERR       *)&err);
        // DTU�յ���Ϣ
        if ( OS_ERR_NONE == err ) {
            // ��Ϣ����
            APP_DtuRxDataDealCB(pch);
        }
        *//***********************************************
        * ������ �ȴ�DTU������־λ
        */
        OS_FLAGS    flags = 
        OSFlagPend( ( OS_FLAG_GRP *)&Ctrl.Os.DtuEvtFlagGrp,
                    ( OS_FLAGS     ) Ctrl.Os.DtuEvtFlag,
                    ( OS_TICK      ) dly,
                    ( OS_OPT       ) OS_OPT_PEND_FLAG_SET_ANY,
                    ( CPU_TS      *) NULL,
                    ( OS_ERR      *)&err);
        
        OS_ERR      terr;
        ticks   = OSTimeGet(&terr);                        // ��ȡ��ǰOSTick
        
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
            
            /***********************************************
            * ������ DTU��λ
            */
            } 
            if ( flags & COMM_EVT_FLAG_RESET ) {
                flagClr |= COMM_EVT_FLAG_RESET;
                
            /***********************************************
            * ������ DTU����
            */
            } 
            if ( flags & COMM_EVT_FLAG_CONNECT ) {
                flagClr |= COMM_EVT_FLAG_CONNECT;
                
            /***********************************************
            * ������ DTU���ڽ�������
            */
            } 
            if ( flags & COMM_EVT_FLAG_RECV ) {
                flagClr |= COMM_EVT_FLAG_RECV;
                APP_DtuRxDataDealCB(Ctrl.Dtu.pch);
            /***********************************************
            * ������ �ϱ���Ϣ
            */
            } 
            if ( flags & COMM_EVT_FLAG_REPORT ) {
                /***********************************************
                * ������ ������ʷ���ݵ�������
                */
                App_SendDataFromHistory();
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
            } else if ( flags & COMM_EVT_FLAG_CONFIG ) {
                flagClr |= COMM_EVT_FLAG_CONFIG;
            
            /***********************************************
            * ������ IAP����
            */
            } 
            if ( flags & COMM_EVT_FLAG_IAP_END ) {
                flagClr |= COMM_EVT_FLAG_IAP_END;
                
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
            OSFlagPost( ( OS_FLAG_GRP  *)&Ctrl.Os.DtuEvtFlagGrp,
                        ( OS_FLAGS      )flagClr,
                        ( OS_OPT        )OS_OPT_POST_FLAG_CLR,
                        ( OS_ERR       *)&err);
            
        /***********************************************
        * ������ �����ʱ������һ��������
        */
        } else if ( err == OS_ERR_TIMEOUT ) {
            
        }
//next:
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
 * ��    �ƣ� APP_DtuInit
 * ��    �ܣ� �����ʼ��
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ� wumingshen.
 * �������ڣ� 2015-03-28
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
 void APP_DtuInit(void)
{
    OS_ERR err;
    
    /***********************************************
    * ������ �����¼���־��
    */
    OSFlagCreate(( OS_FLAG_GRP  *)&Ctrl.Os.DtuEvtFlagGrp,
                 ( CPU_CHAR     *)"App_DtuFlag",
                 ( OS_FLAGS      )0,
                 ( OS_ERR       *)&err);
    
    Ctrl.Os.DtuEvtFlag = COMM_EVT_FLAG_HEART       // ����������
                        + COMM_EVT_FLAG_RESET       // DTU��λ
                        + COMM_EVT_FLAG_CONNECT     // DTU����
                        + COMM_EVT_FLAG_RECV        // ���ڽ���
                        + COMM_EVT_FLAG_REPORT      // ���ڷ���
                        + COMM_EVT_FLAG_CLOSE       // �Ͽ�
                        + COMM_EVT_FLAG_TIMEOUT     // ��ʱ
                        + COMM_EVT_FLAG_CONFIG      // ����
                        + COMM_EVT_FLAG_IAP_START   // IAP��ʼ
                        + COMM_EVT_FLAG_IAP_END;    // IAP����
            
    OSQCreate ( (OS_Q        *)&DTU_RxQ,
                (CPU_CHAR    *)"RxQ",
                (OS_MSG_QTY   ) OS_CFG_INT_Q_SIZE,
                (OS_ERR      *)&err);
    /***********************************************
    * ������ �ڿ��Ź���־��ע�᱾����Ŀ��Ź���־
    */
    WdtFlags |= WDT_FLAG_DTU;
}

/*******************************************************************************
 * ��    �ƣ� APP_DtuRxDataDealCB
 * ��    �ܣ� �������ݴ���ص���������MB_DATA.C����
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ� ������
 * �������ڣ� 2016-01-04
 * ��    �ģ�
 * �޸����ڣ�
 * ��    ע�� 
 *******************************************************************************/
INT08U APP_DtuRxDataDealCB(MODBUS_CH  *pch)
{
    /***********************************************
    * ������ ��ȡ֡ͷ
    */
    CPU_SR_ALLOC();
    CPU_CRITICAL_ENTER();
    u8  Len     = pch->RxBufByteCtr;
    memcpy( (INT08U *)&pch->RxFrameData, (INT08U *)pch->RxBuf, Len );
    CPU_CRITICAL_EXIT();
    
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
    * ������ ������ݳ��Ȼ��ߵ�ַΪ0���򲻴���
    */
    if ( ( Len == 0 ) || ( DstAddr == 0 ) )
        return FALSE;
    
    /***********************************************
    * ������ ��ȡ֡ͷ
    */
    OS_CRITICAL_ENTER();
    memcpy( (INT08U *)&Ctrl.Dtu.Rd.Head, (INT08U *)pch->RxFrameData, 8 );
    OS_CRITICAL_EXIT();

    /***********************************************
    * ������ ��ַ�ȶԣ����Ǳ�����ַ��ֱ�ӷ���
    */
    if ( Ctrl.Dtu.Rd.Head.DstAddr != Ctrl.Dtu.SlaveAddr ) {
        return FALSE;
    }
    
    /***********************************************
    * ������ ��ȡ����
    */
    if( Len > sizeof(StrCommRecvData))
        return FALSE;
    
    OS_CRITICAL_ENTER();
    memcpy( (INT08U *)&Ctrl.Dtu.Rd.Data, (INT08U *)&pch->RxFrameData[8], Len );
    OS_CRITICAL_EXIT();
    
    /***********************************************
    * ������ ��������A
    */
    if ( ( Ctrl.Dtu.Rd.Head.PacketCtrl & 0x0f ) == 0x04 ) {
        
    /***********************************************
    * ������ ��������B
    */
    } else if ( ( Ctrl.Dtu.Rd.Head.PacketCtrl & 0x0f ) == 0x03 ) {
        
    /***********************************************
    * ������ ������ȡ
    */
    } else if ( ( Ctrl.Dtu.Rd.Head.PacketCtrl & 0x0f ) == 0x02 ) {
        
    /***********************************************
    * ������ ��������
    */
    } else if ( ( Ctrl.Dtu.Rd.Head.PacketCtrl & 0x0f ) == 0x01 ) {
        
    /***********************************************
    * ������ ������
    */
    } else if ( ( Ctrl.Dtu.Rd.Head.PacketCtrl & 0x0f ) == 0x00  ) {
        /***********************************************
        * ������ �������ʾģ�飬��Ӧ��
        */
        if ( Ctrl.Para.dat.Sel.udat.DevSel == 2 ) {        
            Ctrl.Dtu.Rd.Data.Oil           = SW_INT16U((Ctrl.Dtu.Rd.Data.Oil>>16));
            goto exit;
        }
    }
exit:
    /***********************************************
    * ������ ���ڽ���DTUģ�����Ϣ������
    */
    Ctrl.Dtu.ConnectTimeOut    = 0;                // ��ʱ����������
    Ctrl.Dtu.ConnectFlag       = TRUE;             // ת���ӳɹ���־
    
    return TRUE;
}

/*******************************************************************************
* ��    �ƣ� UpdateRecordPoint
* ��    �ܣ� 
* ��ڲ����� ��
* ���ڲ����� ��
* ��  ���ߣ� wumingshen.
* �������ڣ� 2017-02-07
* ��    �ģ�
* �޸����ڣ�
* ��    ע��
*******************************************************************************/
void UpdateRecordPoint(uint8_t storeflag)
{
    uint32_t    size    = sizeof(StrMater);             // ��ȡ��¼����
    Mater.Tail  += size;
    
    if ( Mater.Tail >= MAX_ADDR ) {
        Mater.Tail = 0; 
    } else if ( Mater.Tail >= Mater.Head ) {
        Mater.Tail  = Mater.Head; 
    }
    if ( storeflag ) {
        //WriteFM24CL64(72, (uint8_t *)&Mater.Head, 4);    
        WriteFM24CL64(300 + 4, (uint8_t *)&Mater.Tail, 4); 
        WriteFM24CL64(312 + 4, (uint8_t *)&Mater.Tail, 4); 
        WriteFM24CL64(324 + 4, (uint8_t *)&Mater.Tail, 4); 
        //WriteFM24CL64(80, (uint8_t *)&Mater.RecordNbr, 4);
    }
}
/*******************************************************************************
 * ��    �ƣ� app_save_record_number_and_index
 * ��    �ܣ�
 * ��ڲ�����
 * ���ڲ����� ��
 * �� �� �ߣ� fourth peng
 * �������ڣ� 2017-5-5
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void app_save_record_number_and_index()
{
	//����ͷβ�Լ���ˮ�ű���3��
	WriteFM24CL64(300, (uint8_t *)&Mater.Head, 12); 
	WriteFM24CL64(312, (uint8_t *)&Mater.Head, 12); 
	WriteFM24CL64(324, (uint8_t *)&Mater.Head, 12); 
}


/*******************************************************************************
 * ��    �ƣ� App_SaveDataToHistory
 * ��    �ܣ�
 * ��ڲ�����
 * ���ڲ����� ��
 * �� �� �ߣ� ������
 * �������ڣ� 2017-02-7
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void  App_SaveDataToHistory( void )
{
    /**************************************************
    * ������ ���浱ǰ����
    */
    uint32_t    size = sizeof(StrMater);                    // ���ݳ���
        
            CPU_SR  cpu_sr;
    OS_CRITICAL_ENTER();
	Mater.chk = GetCrc16Check((uint8_t *)&Mater,size-2);    // ��ȡ�洢У����
    OS_CRITICAL_EXIT();
	/*д��֮ǰû���ж�д���ַ�Ƿ�������Ķ�*/
    SaveOneREcord(Mater.Head,(uint8_t *)&Mater,size);       // ��������
	Mater.Head += size;                                     // ����ͷָ��
    
    if ( Mater.Head >= MAX_ADDR ) {                         // ͷָ�����
        Mater.Head = 0;                                     // ���ָ�����
        if ( Mater.Tail == 0 )                              // ���βָ���Ƿ�Ϊ��
            Mater.Tail  += size;                            // Ϊ�����βָ��
    } else {                                                // ͷָ��û�����
        if ( Mater.Tail >= Mater.Head ) {                   // βָ����ڵ���ͷָ��
            Mater.Tail  += size;                            // ����βָ��
            if ( Mater.Tail >= MAX_ADDR )                   // βָ�����
                Mater.Tail = 0;                             // βָ�����
        }
    }


	//fourth add for clear the peak voltage	2017-5-6
	memset(&Mater.monitor.peak_vol, 0, sizeof(Mater.monitor.peak_vol));
    
    Mater.RecordNbr++;                                      // ��¼��+1
    
    /**************************************************
    * ������ ���浱ǰ��¼�ź�����ָ��
    */
    app_save_record_number_and_index();
/*    
	WriteFM24CL64(72, (uint8_t *)&Mater.Head, 4); 
	WriteFM24CL64(76, (uint8_t *)&Mater.Tail, 4);
	WriteFM24CL64(80, (uint8_t *)&Mater.RecordNbr, 4);
*/   
    osal_set_event( OS_TASK_ID_TMR, OS_EVT_TMR_MIN);
}

/*******************************************************************************
 * ��    �ƣ� App_SendDataFromHistory
 * ��    �ܣ� ����ת��,���ݵ��ֽ���ǰ�����ֽ��ں� 
 * ��ڲ�����
 * ���ڲ����� ��
 * �� �� �ߣ� ������
 * �������ڣ� 2017-02-7
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void App_SendDataFromHistory(void)
{	    
    /***********************************************
    * ������ �������������ģʽ
    */
    if ( ( Iap.Status != IAP_STS_DEF ) && 
         ( Iap.Status != IAP_STS_SUCCEED ) &&
         ( Iap.Status != IAP_STS_FAILED ) ) {
        return;
    }
    
    /***********************************************
    * ������ ͷָ����ǰ�棨������δ����
    */
    if ( Mater.Head == Mater.Tail ) {
        osal_start_timerRl( OS_TASK_ID_TMR, OS_EVT_TMR_MIN ,  OS_TICKS_PER_SEC * 10);
    /***********************************************
    * ������ ͷָ���ں��棨������������
    */
    } else {
        uint32_t    size    = sizeof(StrMater);             // ��ȡ��¼����
		MX25L3206_RD(Mater.Tail, size,(uint8_t*)&History);  // ��ȡһ����¼
        
        /**************************************************
        * ������ ����У��
        */
		uint16_t    CRC_sum1 = GetCrc16Check((uint8_t*)&History,sizeof(StrMater)-2);		
		uint16_t    CRC_sum2 = History.chk;
        uint32_t    timeout = OS_TICKS_PER_SEC * 5;
        /**************************************************
        * ������ ���ͼ�¼��������
        */
		if(CRC_sum1 == CRC_sum2) {						    //�����ۼӺͼ���
            if (Ctrl.Dtu.pch->RxBufByteCtr == 0) {
                CSNC_SendData( (MODBUS_CH      *) Ctrl.Dtu.pch,
                               (CPU_INT08U      ) Ctrl.Com.SlaveAddr,                     // SourceAddr,
                               (CPU_INT08U      ) 0xCB,                     // DistAddr,
                               (CPU_INT08U     *)&History,                    // DataBuf,
                               (CPU_INT08U      ) size);                    // DataLen 
                Ctrl.Dtu.pch->StatNoRespCtr++;
                if ( Ctrl.Dtu.pch->StatNoRespCtr < 5 ) {
                    timeout     = OS_TICKS_PER_SEC * 10; 
                } else if ( Ctrl.Dtu.pch->StatNoRespCtr < 20 ) {
                    timeout     = OS_TICKS_PER_SEC * 30; 
/*                } else if ( Ctrl.Dtu.pch->StatNoRespCtr < 100 ) {
                    timeout     = OS_TICKS_PER_SEC * 30; */
                } else {                
                    timeout     = OS_TICKS_PER_SEC * 60; 
                }
            }
            osal_start_timerRl( OS_TASK_ID_TMR, OS_EVT_TMR_MIN, timeout );
        } else {
            UpdateRecordPoint(0);
            
            osal_set_event( OS_TASK_ID_TMR, OS_EVT_TMR_MIN);
            //timeout = OS_TICKS_PER_SEC * 1;
        }
        
    }
}

/*******************************************************************************
 * 				end of file
 *******************************************************************************/
#endif
