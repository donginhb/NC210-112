

#include <app.h>
#include <iap.h>
#include "task_comm.h"


/*******************************************************************************
* Description  : ϵͳ����
* Author       : 2018/3/14 ������, by redmorningcn
*******************************************************************************/
strSysPara  sys;


extern uint16	collector_update_count_down_tmr;
OS_RATE_HZ     const  OSCfg_TickRate_Hz          = (OS_RATE_HZ  )OS_CFG_TICK_RATE_HZ;

extern int32 get_pulse_count();

extern  uint32 get_sys_tick();

 /*******************************************************************************
 * ��    �ƣ�     polling
 * ��    �ܣ�     ϵͳʱ�Ӳ�ѯ����
 * ��ڲ�����    ��
 * ���ڲ�����    ��
 * ���� ���ߣ�   ������
 * �������ڣ�    2014-08-08
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void polling()
{
//	static uint32 tick;
//	static uint16 polling_tick_cnt;
//	static uint16 led_blink_cnt = 0;
//
//	if (tick != get_sys_tick())
//	{
//		tick = get_sys_tick();
//		polling_tick_cnt++;
//		if (0 == polling_tick_cnt%100)
//		{
//			//100ms
//			system_send_msg(DETECT_TASK_ID, DETECT_CALC_DETECT_INFO, NULL, 0);
//			if ( 0 == led_blink_cnt )
//			{
//				BSP_LED_On(1);
//			}
//			else
//			{
//				BSP_LED_Off(1);
//			}
//			led_blink_cnt++;
//			if(0 == get_pulse_count())
//			{
//				led_blink_cnt %= 20;
//			}
//			else
//			{
//				led_blink_cnt %= 2;
//			}
////			system_send_msg(COMM_TASK_ID, COMM_TASK_COLLECTION_INFO_UPDATE, NULL, 0);
//		}
//		if (collector_update_count_down_tmr)
//		{
//			collector_update_count_down_tmr--;
//			if (0 == collector_update_count_down_tmr)
//			{
//                /*******************************************************************************
//                * Description  : ���ڲ�����ʱ���������д��ڴ���
//                * Author       : 2018/1/30 ���ڶ�, by redmorningcn
//                *******************************************************************************/
//
//				system_send_msg(COMM_TASK_ID, COMM_TASK_COLLECTION_INFO_UPDATE, NULL, 0);
//			}
//		}
//	}
}

void  OS_CPU_SysTickInit (CPU_INT32U  cnts)
{
    CPU_INT32U  prio;


    CPU_REG_NVIC_ST_RELOAD = cnts - 1u;

                                                            /* Set SysTick handler prio.                              */
    prio  = CPU_REG_NVIC_SHPRI3;
    prio &= DEF_BIT_FIELD(24, 0);
    prio |= DEF_BIT_MASK(OS_CPU_CFG_SYSTICK_PRIO, 24);

    CPU_REG_NVIC_SHPRI3 = prio;

                                                            /* Enable timer.                                          */
    CPU_REG_NVIC_ST_CTRL |= CPU_REG_NVIC_ST_CTRL_CLKSOURCE |
                            CPU_REG_NVIC_ST_CTRL_ENABLE;
                                                            /* Enable timer interrupt.                                */
    CPU_REG_NVIC_ST_CTRL |= CPU_REG_NVIC_ST_CTRL_TICKINT;
}

 /*******************************************************************************
 * Description  : ������˵��
    1��ʵʱ��Ҫ��ߣ�δ�����в���ϵͳ
    2���������в����¼�������ͨ���¼��ص��������У�system_get_msg����
       system_send_msg,���������¼�����ڲ���������ID���¼�ID��������ָ�룬���ݳ��ȣ���
    3��system_get_msg����ڲ���������ID�������пɶ�������¼�����
       �����¼���ָ�루�������¼��š����ݳ��ȡ�������ָ�룩��
    4��app_XXXX_task_event,ѭ��ִ�У��ж�Ӧ�¼�������
       ��������¼�app_XXX_task_event_table���У��ҵ���Ӧ�ص����������¼���

 * Author       : 2018/1/30 ���ڶ�, by redmorningcn
 *******************************************************************************/

void main (void)
{
	CPU_INT32U  cpu_clk_freq;
	CPU_INT32U  cnts;
	BSP_Init();                                                 /* Initialize BSP functions                             */
	CPU_TS_TmrInit();

	/***********************************************
	* ������ ��ʼ���δ�ʱ��������ʼ��ϵͳ����ʱ�ӡ�
	*/
	cpu_clk_freq = BSP_CPU_ClkFreq();                           /* Determine SysTick reference freq.                    */
    
    sys.cpu_freq = cpu_clk_freq;                                //ʱ��Ƶ��
    
	cnts = cpu_clk_freq / (CPU_INT32U)OSCfg_TickRate_Hz;        /* Determine nbr SysTick increments                     */
	//OS_CPU_SysTickInit(cnts);
	
	App_ModbusInit();
	APP_Detect_Init();
    init_ch_timepara_detect();

	while(1)
	{
        /*******************************************************************************
        * Description  : ����ͨѶ����������MOUDB�ײ������¼��Ĵ�����
        * Author       : 2018/3/9 ������, by redmorningcn
        *******************************************************************************/
		AppTaskComm();          //����ͨѶʱ�䴦��
        
        //��AppTaskCommһ����ɴ���ͨѶ
		mod_bus_rx_task();
        
        /*******************************************************************************
          * Description  : �źż��
          * Author       : 2018/3/9 ������, by redmorningcn
          *******************************************************************************/
		//AppTaskDetect();
        
		polling();
        
        app_calc_ch_timepara();
	}
}


