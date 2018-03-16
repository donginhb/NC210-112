/*******************************************************************************
 *   Filename:       app_task_detect.c
 *   Revised:        All copyrights reserved to fourth.Peng.
 *   Revision:       v1.0
 *   Writer:	     fourth.Peng.
 *
 *   Description:
 *
 *   Notes:
 *
 *******************************************************************************/

/*******************************************************************************
 * INCLUDES
 */
#include <includes.h>
#include "bsp_adc.h"
#include "bsp_dac.h"
#include "app_ctrl.h"
#include "task.h"
#include "app_task_mater.h"
#include "app_voltage_detect.h"
#include "app.h"

#include "task_comm.h"


/*******************************************************************************
 * CONSTANTS
 */

/*******************************************************************************
 * MACROS
 */
//ͨ��ʱ�������������С
#define CH_TIMEPARA_BUF_SIZE    50

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
* Description  : ͨ���ṹ�塣���У�ͨ��ʱ������������㣻
                             ͨ����������ͨ���ľ���ָ�ꡣ
* Author       : 2018/3/13 ���ڶ�, by redmorningcn
*******************************************************************************/
typedef struct
{
    /*******************************************************************************
    * Description  : ��������
    * Author       : 2018/3/14 ������, by redmorningcn
    *******************************************************************************/
    struct  {
        struct  {
            uint32  low_up_time;                            //10%λ�ã������أ��ж�ʱ�� 
            uint32  low_down_time;                          //10%λ�ã��½��أ��ж�ʱ��
            uint32  hig_up_time;                            //90%λ�ã������أ��ж�ʱ��
            uint32  hig_down_time;                          //90%λ�ã��½��أ��ж�ʱ��
            uint16  low_up_cnt;
            uint16  low_down_cnt;
            uint16  hig_up_cnt;
            uint16  hig_down_cnt;
        }time[CH_TIMEPARA_BUF_SIZE];
        
        uint32  pulse_cnt;                                  //����������ж��ź�����
        uint16  p_write;                                    //��������д����
        uint16  p_read;
    }test[2];                                               //ͨ���������
    
    /*******************************************************************************
    * Description  : ͨ������ָ��
    * Author       : 2018/3/14 ������, by redmorningcn
    *******************************************************************************/
    struct _strsignalchannelpara_ {
        uint32              period;                 //���ڣ�  0.00-2000000.00us ��0.5Hz��
        uint32              freq;                   //Ƶ�ʣ�  0-100000hz              
        uint16              raise;                  //�����أ�0.00-50.00us
        uint16              fail;                   //�½��أ�0.00-50.00us
        uint16              ratio;                  //ռ�ձȣ�0.00-100.00%
        uint16              Vol;                    //�͵�ƽ��0.00-30.00V
        uint16              Voh;                    //�ߵ�ƽ��0.00-30.00V
        uint16              status;                 //ͨ��״̬
    }para[2];
    
    uint32  ch1_2phase;                             //��λ�0.00-360.00��
}strCoupleChannel;



/*******************************************************************************
* Description  : ͨ������
* Author       : 2018/3/14 ������, by redmorningcn
*******************************************************************************/
strCoupleChannel    ch;





ST_COLLECTOR_INFO detect_info = {0};
//#define TEST
#ifdef TEST
const ST_COLLECTOR_INFO test_info =
{
.status =0,				 /*����״̬;0:����1:��챻��*/
.reserved=0,
.phase_diff = 900,			/*��λ��*/
.step_counts = 650,			/*����ת������*/
.channel[0].vcc_vol = 15000,		/*������ѹ*/
.channel[0].high_level_vol = 14900,		/*�ߵ�ƽ��ѹ*/
.channel[0].low_level_vol = 100,		/*�͵�ƽ��ѹ*/
.channel[0].rise_time = 100,			/* �����ر���ʱ��*/
.channel[0].fall_time = 90,			/*�½��ر���ʱ��*/
.channel[0].duty_ratio = 500,		/*ռ�ձ�*/
.channel[1].vcc_vol = 15100,		/*������ѹ*/
.channel[1].high_level_vol = 15000,		/*�ߵ�ƽ��ѹ*/
.channel[1].low_level_vol = 200,		/*�͵�ƽ��ѹ*/
.channel[1].rise_time = 110,			/* �����ر���ʱ��*/
.channel[1].fall_time = 99,			/*�½��ر���ʱ��*/
.channel[1].duty_ratio = 800,		/*ռ�ձ�*/
};
#endif
/***********************************************
* ������ ������ƿ飨TCB��
*/
static  OS_TCB   AppTaskDetectTCB;
/***********************************************
* ������ ������ն���
*/
extern OS_Q                DET_RxQ;


/***********************************************
* ������ �����ջ��STACKS��
*/
static  CPU_STK  AppTaskDetectStk[ APP_TASK_KEY_STK_SIZE ];

/*******************************************************************************
 * LOCAL VARIABLES
 */
OS_TMR	app_detect_tmr = {0};
OS_TMR	app_sample_tmr = {0};

//ST_DET_VOL detect_vol = {0};

ST_CONDITION_DETECT condition_info[MAX_ADC_CHANNEL-2] = {0};


//TIM8 ����״̬
static uint8 capture_state[4]={0x40,0x40,0x40,0x40};
uint32 m1_cycle_count,m2_cycle_count;

/*******************************************************************************
 * GLOBAL VARIABLES
 */
/*******************************************************************************
 * LOCAL FUNCTIONS
 */
void APP_Detect_Init(void);
void app_detect_time_tick(void *data, uint16 len);
static void app_detect_adc_data_update(void *data, uint16 len);

void  AppTaskDetect (void *p_arg);
void app_detect_tmr_callback(OS_TMR *ptmr,void *p_arg);
static void detect_timer_init(void);

void app_detect_task_event(ST_QUEUE *queue);
void app_calc_cha_fall_time(void *data, uint16 len);
void app_calc_cha_rise_time(void *data, uint16 len);
void app_calc_chb_fall_time(void *data, uint16 len);
void app_calc_chb_rise_time(void *data, uint16 len);
void app_sample_cycle_tick(void *data, uint16 len);
void app_check_m1_cycle(void *data, uint16 len);
 void app_check_m2_cycle(void *data, uint16 len);
void app_calc_m1_data(void *data, uint16 len);
void app_calc_m2_data(void *data, uint16 len);

void app_detect_sample_cycle_callback(void *data, uint16 len);

void Detect_exti_init(void);
void  cha_level_detect_tmrInit (void);
void cha_level_detect_tmr_set(uint32 dly_time);
void  chb_level_detect_tmrInit (void);
void chb_level_detect_tmr_set(uint32 dly_time);
int32 get_pulse_count(void);


/* comm task �����¼���Ӧ�ص��������*/
const ST_TASK_EVENT_TBL_TYPE app_detect_task_event_table[]=
{
{DETECT_TASK_TIME_TICK,				app_detect_time_tick			},
{DETECT_TASK_VOL_ADC_UPDATE,		app_detect_adc_data_update	    },
/*
{DETECT_TASK_CALC_CHA_FALL_TIME,	app_calc_cha_fall_time		},
{DETECT_TASK_CALC_CHA_RISE_TIME,	app_calc_cha_rise_time		},
{DETECT_TASK_CALC_CHB_FALL_TIME,	app_calc_chb_fall_time		},
{DETECT_TASK_CALC_CHB_RISE_TIME,	app_calc_chb_rise_time		},
*/
{DETECT_TASK_CHECK_M1_CYCLE,		app_check_m1_cycle		},
{DETECT_TASK_CHECK_M2_CYCLE,		app_check_m2_cycle		},
{DETECT_TASK_M1_DATA_UPDATE,		app_calc_m1_data		},
{DETECT_TASK_M2_DATA_UPDATE,		app_calc_m2_data		},
{DETECT_CALC_DETECT_INFO,			app_detect_sample_cycle_callback	},

};

/*******************************************************************************
 * GLOBAL FUNCTIONS
 */

/*******************************************************************************
 * EXTERN VARIABLES
 */
extern ST_para_storage system_para;
/*******************************************************************************
 * EXTERN FUNCTIONS
 */
 uint32 diff(uint32 a, uint32 b)
{
	uint32 d_value = a-b;
	if (d_value > 0x80000000)
		d_value = b - a;
	return d_value;
}
/*******************************************************************************/

/*******************************************************************************
 * ��    �ƣ� AppTaskKey
 * ��    �ܣ� ��������
 * ��ڲ����� p_arg - �����񴴽���������
 * ���ڲ����� ��
 * �������ߣ� Roger-WY.
 * �������ڣ� 2015-02-05
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void  AppTaskDetect (void *p_arg)
{
	ST_QUEUE *p_mailbox = 0;
	/***********************************************
	* ������ �����ʼ��
	*/
//	APP_Detect_Init();

	/***********************************************
	* ������Task body, always written as an infinite loop.
	*/

	p_mailbox = (ST_QUEUE *)system_get_msg(DETECT_TASK_ID);
	if (NULL != p_mailbox)
	{
		app_detect_task_event(p_mailbox);
	}

}
/*******************************************************************************
 * ��    �ƣ� APP_GPIO_Init
 * ��    �ܣ� �����ʼ��
 * ��ڲ����� ��
 * ���ڲ����� ��
 * �������ߣ� fourth. peng
 * �������ڣ� 2015-03-28
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
 static void APP_GPIO_Init()
{
    GPIO_InitTypeDef gpio_init;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA| RCC_APB2Periph_AFIO, ENABLE);

    gpio_init.GPIO_Pin  = GPIO_Pin_11|GPIO_Pin_12;
    gpio_init.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &gpio_init);
}
/*******************************************************************************
 * ��    �ƣ� Get_board_id
 * ��    �ܣ� �����ʼ��
 * ��ڲ����� ��
 * ���ڲ����� ��
 * �������ߣ� fourth. peng
 * �������ڣ� 2017-05-11
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
 uint8 Get_board_id(void)
{
	uint8 id = 0;
	Delay_Nus(10);
	if(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_11))
	{
		id |= 0x2;
	}
	if(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_12))
	{
		id |= 0x1;
	}
	return id;
}
/*******************************************************************************
 * ��    �ƣ� APP_Detect_Init
 * ��    �ܣ� �����ʼ��
 * ��ڲ����� ��
 * ���ڲ����� ��
 * �������ߣ� fourth. peng
 * �������ڣ� 2015-05-6
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void APP_Detect_Init(void)
{
	OS_ERR err;
	/***********************************************
	* ������ ��ʼ���������õ������Ӳ��
	*/
	Bsp_ADC_Init();
#ifdef SAMPLE_BOARD
	APP_GPIO_Init();
	Ctrl.sample_id = Get_board_id();
	BSP_dac_init();
	
    //detect_timer_init();
	//Detect_exti_init();
	//cha_level_detect_tmrInit();
	//chb_level_detect_tmrInit();
#endif

	/***********************************************
	* ������ �ڿ��Ź���־��ע�᱾����Ŀ��Ź���־
	*/
//	WdtFlags |= WDT_FLAG_DETECT;
#if	0
	/*����һ������Ϊ10ms�Ķ�ʱ��*/
	OSTmrCreate( &app_detect_tmr,
					(CPU_CHAR *)"app_detect_tmr",
					10,
					10,
					OS_OPT_TMR_PERIODIC,
					(OS_TMR_CALLBACK_PTR)app_detect_tmr_callback,
					NULL,
					&err);
	OSTmrStart(&app_detect_tmr, &err);

	OSTmrCreate( &app_sample_tmr,
					(CPU_CHAR *)"app_detect_tmr",
					100,
					100,
					OS_OPT_TMR_PERIODIC,
					(OS_TMR_CALLBACK_PTR)app_detect_sample_cycle_callback,
					NULL,
					&err);
	OSTmrStart(&app_sample_tmr, &err);

	/*����һ�����У�������Ϣ����*/
	OSQCreate(&DET_RxQ, "DET RxQ", 100, &err);
	WdtFlags |= WDT_FLAG_DETECT;
#endif
}

/*******************************************************************************
 * ��    �ƣ� app_detect_tmr_callback
 * ��    �ܣ�
 * ��ڲ����� ��
 * ���ڲ����� ��
 * �������ߣ� fourth.
 * �������ڣ� 2017-5-3
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void app_detect_tmr_callback(OS_TMR *ptmr,void *p_arg)
{
	OS_ERR err;
	static ST_QUEUETCB mailbox = {0};
	mailbox.event = DETECT_TASK_TIME_TICK;
	mailbox.queue_data.pdata = NULL;
//	OSQPost(&DET_RxQ, &mailbox, sizeof(mailbox), OS_OPT_POST_FIFO, &err);

}
#define CHANNEL_NUM		2
/*******************************************************************************
 * ��    �ƣ� app_dac_output_update
 * ��    �ܣ�
 * ��ڲ����� ��
 * ���ڲ����� ��
 * �������ߣ� fourth.
 * �������ڣ� 2017-5-3
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
	uint16 ref_voltage_h = 0;
	uint16 ref_voltage_l = 0;

void app_dac_output_update()
{
	#define dac_value_factor			(0.1241)


	uint32 sig_h = 0;
	uint32 sig_l = 0;

	if((detect_info.channel[0].high_level_vol < (detect_info.channel[0].vcc_vol>>1))
		&&(detect_info.channel[0].high_level_vol < (detect_info.channel[1].vcc_vol>>1)))
	{
		return;
	}

	if (detect_info.channel[0].high_level_vol > detect_info.channel[1].high_level_vol)
	{
		sig_h = detect_info.channel[1].high_level_vol;
	}
	else
	{
		sig_h = detect_info.channel[0].high_level_vol;
	}

	if (detect_info.channel[0].low_level_vol > detect_info.channel[1].low_level_vol)
	{
		sig_l = detect_info.channel[0].low_level_vol;
	}
	else
	{
		sig_l = detect_info.channel[1].low_level_vol;
	}

	if(sig_h > 30000)
	{
		sig_h = 30000;
	}
	else if(sig_h < 5000)
	{
		sig_h = 5000;
	}
	if(sig_l > 2000)
	{
		sig_l = 2000;
	}

	ref_voltage_h = (uint16)((sig_l + (sig_h - sig_l)*0.9)*dac_value_factor);
	ref_voltage_l = (uint16)((sig_l + (sig_h - sig_l)*0.1)*dac_value_factor);

    /*******************************************************************************
    * Description  : ����DAC�ο�ֵ�����ݲ��䣬���ܵ�����
    * Author       : 2018/3/9 ������, by redmorningcn
    *******************************************************************************/

	//DAC_SetDualChannelData(DAC_Align_12b_R, ref_voltage_h, ref_voltage_l);
    DAC_SetDualChannelData(DAC_Align_12b_R, 4096/5, 4096/5);
}
/*******************************************************************************
 * ��    �ƣ� app_detect_time_tick
 * ��    �ܣ� 	10ms�ж�һ��
 * ��ڲ����� ��
 * ���ڲ����� ��
 * �������ߣ� fourth.
 * �������ڣ� 2017-5-3
 * ��    �ģ�
 *******************************************************************************/
void app_detect_time_tick(void *data, uint16 len)
{
	uint8 i;
	for(i = 0; i < (MAX_ADC_CHANNEL - 2); i++)
	{
		if (TRUE == condition_info[i].transform.active )
		{
			if (condition_info[i].transform.timeout_cnt)
			{
				condition_info[i].transform.timeout_cnt--;
				if (0 == condition_info[i].transform.timeout_cnt )
				{
					//������ɣ��洢������¼
#if 0
					OS_ERR err;
				        OS_FlagPost(( OS_FLAG_GRP *)&Ctrl.Os.MaterEvtFlagGrp,
						            ( OS_FLAGS     ) COMM_EVT_SAVE_DATA,
						            ( OS_OPT       ) OS_OPT_POST_FLAG_SET,
						            ( CPU_TS       ) 0,
						            ( OS_ERR      *) &err);
#endif
				}
			}
		}
	}
}
#ifndef SAMPLE_BOARD
/*******************************************************************************
 * ��    �ƣ� app_condition_state_handler
 * ��    �ܣ� 	����״̬����
 * ��ڲ����� ��
 * ���ڲ����� ��
 * �������ߣ� fourth.
 * �������ڣ� 2017-5-3
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
void app_condition_state_handler(uint8 ch, CONDITION_STATUS next)
{
	CONDITION_STATUS *p_curr_status = &condition_info[ch].status;
	if (*p_curr_status != next)
	{
		//
		if (FALSE == condition_info[ch].transform.active)
		{
			condition_info[ch].transform.active = TRUE;
		}

		//��ǰ������ѹ���ڷ�ֵ��ѹ
		if (Mater.monitor.peak_vol[ch]< Mater.monitor.voltage[ch])
		{
			Mater.monitor.peak_vol[ch] = Mater.monitor.voltage[ch];
		}
		condition_info[ch].transform.timeout_cnt = TRANSFORM_TIMEOUT_CNT;
	}
	else if ( VOLTAGE_OVERFLOW == *p_curr_status)
	{
		//��ǰ������ѹ���ڷ�ֵ��ѹ
		if (Mater.monitor.peak_vol[ch] < Mater.monitor.voltage[ch])
		{
			Mater.monitor.peak_vol[ch] = Mater.monitor.voltage[ch];
		}
		condition_info[ch].transform.timeout_cnt = TRANSFORM_TIMEOUT_CNT;
	}

	*p_curr_status = next;

}

/*******************************************************************************
 * ��    �ƣ� app_detect_adc_data_update
 * ��    �ܣ� 	����״̬ת��
 * ��ڲ����� ��
 * ���ڲ����� ��
 * �������ߣ� fourth.
 * �������ڣ� 2017-5-3
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
 void app_condition_status_update(uint8 ch, uint16 vol)
{
	CONDITION_STATUS state = condition_info[ch].status;

	switch(state)
	{
		case LOW_LEVEL_STATE:
			if ( vol >  system_para.condition_factor.over_level_upper)
			{
				state = VOLTAGE_OVERFLOW;
			}
			else if ( vol >system_para.condition_factor.high_level_upper)
			{
				state = HIG_LEVEL_STATE;
			}
			else if ( vol > system_para.condition_factor.low_level_upper)
			{
				state = RISE_OR_FALL_STATE;
			}
			break;
		case RISE_OR_FALL_STATE:
			if ( vol < system_para.condition_factor.low_level_lower )
			{
				state = LOW_LEVEL_STATE;
			}
			else if ( vol > system_para.condition_factor.over_level_upper)
			{
				state = VOLTAGE_OVERFLOW;
			}
			else if ( vol > system_para.condition_factor.high_level_upper)
			{
				state = HIG_LEVEL_STATE;
			}
			break;
		case HIG_LEVEL_STATE:
			if ( vol < system_para.condition_factor.low_level_lower )
			{
				state = LOW_LEVEL_STATE;
			}
			else if ( vol > system_para.condition_factor.over_level_upper)
			{
				state = VOLTAGE_OVERFLOW;
			}
			else if ( vol <  system_para.condition_factor.high_level_lower)
			{
				state = RISE_OR_FALL_STATE;
			}
			break;
		case VOLTAGE_OVERFLOW:
			if ( vol < system_para.condition_factor.low_level_lower )
			{
				state = LOW_LEVEL_STATE;
			}
			else if ( vol <  system_para.condition_factor.over_level_lower)
			{
				state = HIG_LEVEL_STATE;
			}
			else if ( vol <  system_para.condition_factor.high_level_lower)
			{
				state = RISE_OR_FALL_STATE;
			}
			break;
		}

	app_condition_state_handler(ch, state);
}
 #endif
/*******************************************************************************
 * ��    �ƣ� app_detect_adc_data_update
 * ��    �ܣ� 	ADC���ݴ���
 * ��ڲ����� ��
 * ���ڲ����� ��
 * �������ߣ� fourth.
 * �������ڣ� 2017-5-3
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
static void app_detect_adc_data_update(void *data, uint16 len)
{
#ifndef SAMPLE_BOARD
	uint16_t adc_value[Channel_Times][Channel_Number] = {0};
	int16 adc_average_value[Channel_Number] = {0};
	uint8 i,count;
	uint16 sum = 0;
	uint32 voltage;
	int16 high_ref, low_ref;

	memcpy(adc_value, data->pdata, data->len);
	for (i=0; i < Channel_Number; i++)
	{
		for(count=0 ;count < Channel_Times; count++)
		{
			sum += adc_value[count][i];
		}

		adc_average_value[i] = sum/Channel_Times;
		sum = 0;
	}

	for (i = 0; i < (MAX_ADC_CHANNEL -2); i++)
	{
		voltage = (uint32)(((adc_average_value[i]*3300/4096)*system_para.voltage_factor)/1000);
		app_condition_status_update(i, voltage);
		Mater.monitor.voltage[i] = voltage;
	}
	Mater.monitor.voltage[ADC_CHANNEL_POWER] = (uint16)(((adc_average_value[ADC_CHANNEL_POWER]*3300/4096)*system_para.voltage_factor)/1000);
	Mater.monitor.voltage[ADC_CHANNEL_RTC_BAT] = (uint16)((adc_average_value[ADC_CHANNEL_POWER]*3300/4096)*system_para.bat_vol_factor);
#endif
}



/*******************************************************************************
* Description  : ͨ��ʱ���������
                 ����ͨ��buf�Ķ�дָ�룬ȷ���Ƿ���Ҫ������
* Author       : 2018/3/13 ���ڶ�, by redmorningcn
*******************************************************************************/
void    app_calc_ch_timepara(void)
{
    uint8   p_write;
    uint8   p_read;
    uint64  starttime;
    uint64  endtime;
    uint16  cnt;
    
    uint64  periodtime;
    uint64  ratiotime;
    uint64  phasetime;
    
    uint8   i;
        
    for(i = 0;i< 2;i++)
    {
        p_write = ch.test[i].p_write;
        p_read  = ch.test[i].p_read;
        
        /*******************************************************************************
        * Description  : �ٶ�ͨ��ʱ���������
        * Author       : 2018/3/13 ���ڶ�, by redmorningcn
        *******************************************************************************/
        if(     ( p_write > p_read) &&  (p_write > p_read+10)           
           ||   ( p_write < p_read) &&  (p_write + CH_TIMEPARA_BUF_SIZE > p_read+10)           
               )  
        {

            /*******************************************************************************
            * Description  : ��������(0.01us) �����ź�����һ���ٴγ��֣�ȡlow_up�ж�Ϊ��׼
            * Author       : 2018/3/13 ���ڶ�, by redmorningcn
            *******************************************************************************/
            p_read      =   ch.test[i].p_read;
            starttime   =   ch.test[i].time[p_read].low_up_time  * 65536 
                        +   ch.test[i].time[p_read].low_up_cnt  ;
            
            p_read      =   (ch.test[i].p_read + 1) % CH_TIMEPARA_BUF_SIZE;       //��ֹԽ��
            endtime     =   ch.test[i].time[p_read].low_up_time  * 65536 
                        +   ch.test[i].time[p_read].low_up_cnt  ;
            
            if(starttime > endtime)             //��ֹ��ת
            {
                endtime += 65536;
            }
            periodtime = endtime - starttime;
            
            ch.para[i].period = (periodtime * 1000*1000*100 )/ sys.cpu_freq;
            
            if(periodtime){
                ch.para[i].freq = sys.cpu_freq  / periodtime;   //����Ƶ��
                
                if(((sys.cpu_freq *10) % periodtime)> 4 )       //��������
                    ch.para[i].freq += 1;
            }
            
            /*******************************************************************************
            * Description  : ����ռ�ձ�(xx.xx%)��( hig_down -  low_up ) / period
            * Author       : 2018/3/13 ���ڶ�, by redmorningcn
            *******************************************************************************/
            p_read      =   ch.test[i].p_read;
            starttime   =   ch.test[i].time[p_read].low_up_time  * 65536 
                        +   ch.test[i].time[p_read].low_up_cnt  ;
            endtime     =   ch.test[i].time[p_read].low_down_time  * 65536 
                        +   ch.test[i].time[p_read].low_down_cnt  ;            
            if(starttime > endtime)             //��ֹ��ת
            {
                endtime += 65536;
            }
            ratiotime = endtime - starttime;
            
            if( periodtime )
                ch.para[i].ratio = ( ratiotime * 100* 100 ) / periodtime; 
            
            /*******************************************************************************
            * Description  : ���������أ�0.01us��
            * Author       : 2018/3/13 ���ڶ�, by redmorningcn
            *******************************************************************************/
            p_read      =   ch.test[i].p_read;
            starttime   =   ch.test[i].time[p_read].low_up_time  * 65536 
                        +   ch.test[i].time[p_read].low_up_cnt  ;
            endtime     =   ch.test[i].time[p_read].hig_up_time  * 65536 
                        +   ch.test[i].time[p_read].hig_up_cnt  ;            
            if(starttime > endtime)             //��ֹ��ת
            {
                endtime += 65536;
            }

            ch.para[i].raise = ( endtime - starttime)/sys.cpu_freq;
            
            /*******************************************************************************
            * Description  : �����½���(0.01us)
            * Author       : 2018/3/13 ���ڶ�, by redmorningcn
            *******************************************************************************/
            p_read      =   ch.test[i].p_read;
            starttime   =   ch.test[i].time[p_read].hig_down_time  * 65536 
                        +   ch.test[i].time[p_read].hig_down_cnt  ;
            endtime     =   ch.test[i].time[p_read].low_down_time  * 65536 
                        +   ch.test[i].time[p_read].low_down_cnt  ;            
            if(starttime > endtime)             //��ֹ��ת
            {
                endtime += 65536;
            }
            
            ch.para[i].fail = ( endtime - starttime)/sys.cpu_freq;
            

            /*******************************************************************************
            * Description  : ������λ��(xx.xx��)
            * Author       : 2018/3/13 ���ڶ�, by redmorningcn
            *******************************************************************************/
            if(i == 1)      
            {
                p_read      =   ch.test[i].p_read;
                starttime   =   ch.test[0].time[p_read].low_up_time  * 65536 
                            +   ch.test[0].time[p_read].low_up_cnt  ;
                endtime     =   ch.test[1].time[p_read].low_up_time  * 65536 
                            +   ch.test[1].time[p_read].low_up_cnt  ;            
                if(starttime > endtime)             //��ֹ��ת
                {
                    endtime += periodtime;          //��һ����ʱ��
                }
                
                ch.ch1_2phase = (endtime - starttime) / periodtime; 
            }

            //��ָ��++
            ch.test[i].p_read++ ;
            ch.test[i].p_read %= CH_TIMEPARA_BUF_SIZE; 
        }
    }
}


/*******************************************************************************
* Description  : ȫ��ʱ���ۻ�����ʵʱ�� time = strSys.time * 65536 + TIM_GetCounter  
                              �ٳ��Ե�����ʱ�䡣
* Author       : 2018/3/13 ���ڶ�, by redmorningcn
*******************************************************************************/
void TIM8_OVER_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM8,TIM_IT_Update)!=RESET)  //����������ж�
	{
		TIM_ClearITPendingBit(TIM8,TIM_IT_Update);  //����жϱ�־
        sys.time++;                                 //ϵͳ��ʱ���ۼ�
	}
}

/*******************************************************************************
* Description  : ȫ��ʱ�ӣ�Ϊ�����ź��ṩͳһʱ���׼
* Author       : 2018/3/13 ���ڶ�, by redmorningcn
*******************************************************************************/
void Timer8_Iint(void)
{
	TIM_TimeBaseInitTypeDef	TIM_BaseInitStructure;
//	TIM_ICInitTypeDef TIM_ICInitStruct;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8,ENABLE);//ʹ�ܶ�ʱ��ʱ��
	//��ʼ����ʱ��8
	TIM_BaseInitStructure.TIM_Period = 65535;                   //�������Զ���װֵ
	TIM_BaseInitStructure.TIM_Prescaler = 0;                    //����Ƶ
	TIM_BaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;     //ʱ�Ӳ��ָ�
	TIM_BaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up; //���ϼ���
	TIM_BaseInitStructure.TIM_RepetitionCounter = 0;            //�ظ���������
    
	TIM_TimeBaseInit(TIM8,&TIM_BaseInitStructure);              //��ʼ��ʱ��
    
	TIM_ClearFlag(TIM8, TIM_FLAG_Update);                       //����жϱ�־λ
	TIM_ITConfig(TIM8,TIM_IT_Update,ENABLE);                    //����������ж�
	TIM_Cmd(TIM8,ENABLE);
    
  
	BSP_IntVectSet(TIM8_UP_IRQn, TIM8_OVER_IRQHandler);
	BSP_IntEn(TIM8_UP_IRQn);
    
    sys.time = 0;                                               //ϵͳʱ����0
}


/*******************************************************************************
* Description  : ��Լ�ж�ʱ�䣬������ֻ��ֵ��
                 1��ȡϵͳ��ʱ��ʱ��sys.time
                 2��ȡ�������������ʱ��cnt�����������ʵ��ʱ��Ϊsys.time * 65536 + cnt;
                 3�������źż�¼������ʱ�䣬���β���ʱ��ȷ�� 
                    ��������10%λ��ʱ�䣻
                    ��������90%λ��ʱ�䣻
                    �½�����10%λ��ʱ�䣻
                    �½�����90%λ��ʱ�䡣
* Author       : 2018/3/16 ������, by redmorningcn
*******************************************************************************/
void TIM8_CC_IRQHandler(void)
{

    uint16      cnt;
    u32         time;                                //ʱ����� sys.time * 65536+TIM_CNT     

    //cnt  = TIM_CNT;
    time = sys.time;                                //ʱ����� sys.time * 65536+TIM_CNT     
	
	if(TIM8->SR&0x04)								//CH2�����ж� 
	{
        cnt = TIM8->CCR2;

        if(GPIO_ReadInputDataBit(GPIOC,GPIO_Pin_7) == SET)      //��λ��90%������������
        {     
            TIM_OC2PolarityConfig(TIM8,TIM_ICPolarity_Falling); //����Ϊ�½��ز���			

            ch.test[0].time[ch.test[0].p_write].hig_up_time  = time;     
            ch.test[0].time[ch.test[0].p_write].hig_up_cnt   = cnt;   
        }
        else                                                    //��λ��90%���½��ش���
        {    
            TIM_OC2PolarityConfig(TIM8,TIM_ICPolarity_Rising);  //����Ϊ�����ز���

            ch.test[0].time[ch.test[0].p_write].hig_down_time   = time;     
            ch.test[0].time[ch.test[0].p_write].hig_down_cnt    = cnt;   
        }
        
        TIM_ClearITPendingBit(TIM8,TIM_IT_CC2);                 //����жϱ�־
	}
    
	if(TIM8->SR&0x02)								            //CC1�����ж� 
	{

        cnt = TIM8->CCR1;
        if(GPIO_ReadInputDataBit(GPIOC,GPIO_Pin_6) == SET)      //��λ��10%������������
        {  
            TIM_OC1PolarityConfig(TIM8,TIM_ICPolarity_Falling);//����Ϊ�½��ز���			

            ch.test[0].time[ch.test[0].p_write].low_up_time    =  time;     
            ch.test[0].time[ch.test[0].p_write].low_up_cnt     =  cnt;    
        }else                                                   //��λ��10%�����½�����
        {           
            TIM_OC1PolarityConfig(TIM8,TIM_ICPolarity_Rising);//����Ϊ�����ز���

            ch.test[0].time[ch.test[0].p_write].low_down_time    =  time;     
            ch.test[0].time[ch.test[0].p_write].low_down_cnt     =  cnt; 
            
            ch.test[0].pulse_cnt++;                             //���ڽ��������ں���
            ch.test[0].p_write           =      ch.test[0].pulse_cnt 
                % CH_TIMEPARA_BUF_SIZE;
        }
        
        TIM_ClearITPendingBit(TIM8,TIM_IT_CC1);//����жϱ�־
	}    
  
	

	if(TIM8->SR&0x10)//CH4�����ж� ��CH4�������ж��м�¼��ֵ��Ϊ�����ε�������ʱ���
	{
        
        cnt =  TIM8->CCR4;
        if(GPIO_ReadInputDataBit(GPIOC,GPIO_Pin_9) == SET)      //��λ��90%������������
        {     
            TIM_OC4PolarityConfig(TIM8,TIM_ICPolarity_Falling);//����Ϊ�½��ز���			

            ch.test[1].time[ch.test[1].p_write].hig_up_time  = time;     
            ch.test[1].time[ch.test[1].p_write].hig_up_cnt   = cnt;   
        }
        else                                                    //��λ��90%���½��ش���
        {           
            TIM_OC4PolarityConfig(TIM8,TIM_ICPolarity_Rising);//����Ϊ�����ز���		

            ch.test[1].time[ch.test[1].p_write].hig_down_time   = time;     
            ch.test[1].time[ch.test[1].p_write].hig_down_cnt    = cnt;   
        }
        
        TIM_ClearITPendingBit(TIM8,TIM_IT_CC4);//����жϱ�־

	}
    
	if(TIM8->SR&0x08)//CH3�����ж�  
	{
        cnt = TIM8->CCR3;
        if(GPIO_ReadInputDataBit(GPIOC,GPIO_Pin_8) == SET)      //��λ��10%������������
        {  
            TIM_OC3PolarityConfig(TIM8,TIM_ICPolarity_Falling);//����Ϊ�½��ز���			

            ch.test[1].time[ch.test[1].p_write].low_up_time    =  time;     
            ch.test[1].time[ch.test[1].p_write].low_up_cnt     =  cnt;    
        }
        else                                                    //��λ��10%�����½�����
        {     
            TIM_OC3PolarityConfig(TIM8,TIM_ICPolarity_Rising);//����Ϊ�����ز���		

            ch.test[1].time[ch.test[1].p_write].low_down_time    =  time;     
            ch.test[1].time[ch.test[1].p_write].low_down_cnt     =  cnt;  
            
            
            ch.test[1].pulse_cnt++;                             //���ڽ��������ں���
            ch.test[1].p_write             =        ch.test[1].pulse_cnt 
                % CH_TIMEPARA_BUF_SIZE;
        }
            
        TIM_ClearITPendingBit(TIM8,TIM_IT_CC3);//����жϱ�־
	}
    
}


/*******************************************************************************
* Description  : ���ö�ʱ���ⲿ����
                ��ʱ����˫�߲�����bug�����ܲ��������жϷ�������иı䴥����Ե��ʵ��
                ˫�߲����ܡ�
                
* Author       : 2018/3/16 ������, by redmorningcn
*******************************************************************************/
void Timer8_Configuration(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
//	TIM_TimeBaseInitTypeDef	TIM_BaseInitStructure;
	TIM_ICInitTypeDef TIM_ICInitStruct;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8,ENABLE);//ʹ�ܶ�ʱ��ʱ��
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC,ENABLE);//ʹ��IO��ʱ��
	
	GPIO_InitStructure.GPIO_Pin =        GPIO_Pin_6
                                        |GPIO_Pin_7  
                                        |GPIO_Pin_8
                                        |GPIO_Pin_9;
                           
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;//��������
	GPIO_Init(GPIOC,&GPIO_InitStructure);//��ʼ��
	
	//��ʼ��TIM8���벶�����
	TIM_ICInitStruct.TIM_Channel = TIM_Channel_1;//�����ӳ�䵽TI1
    
	TIM_ICInitStruct.TIM_ICPolarity = TIM_ICPolarity_Rising;//�����ز���
    //TIM_ICInitStruct.TIM_ICPolarity = TIM_ICPolarity_BothEdge;//�����ز���
    
	TIM_ICInitStruct.TIM_ICSelection = TIM_ICSelection_DirectTI;//ӳ�䵽TI1��
	TIM_ICInitStruct.TIM_ICPrescaler = TIM_ICPSC_DIV1;//���벻��Ƶ
	TIM_ICInitStruct.TIM_ICFilter = 0x00;//���벻�˲�
	TIM_ICInit(TIM8,&TIM_ICInitStruct);//��ʼ��TIM8
	
	TIM_ICInitStruct.TIM_Channel = TIM_Channel_2;//�����ӳ�䵽TI2
	TIM_ICInitStruct.TIM_ICPolarity = TIM_ICPolarity_Rising;//�����ز���
	TIM_ICInitStruct.TIM_ICSelection = TIM_ICSelection_DirectTI;//ӳ�䵽TI2��
	TIM_ICInitStruct.TIM_ICPrescaler = TIM_ICPSC_DIV1;//���벻��Ƶ
	TIM_ICInitStruct.TIM_ICFilter = 0x00;//���벻�˲�
	TIM_ICInit(TIM8,&TIM_ICInitStruct);//��ʼ��TIM8
	
	TIM_ICInitStruct.TIM_Channel = TIM_Channel_3;//�����ӳ�䵽TI3
	TIM_ICInitStruct.TIM_ICPolarity = TIM_ICPolarity_Rising;//�����ز���
	TIM_ICInitStruct.TIM_ICSelection = TIM_ICSelection_DirectTI;//ӳ�䵽TI3��
	TIM_ICInitStruct.TIM_ICPrescaler = TIM_ICPSC_DIV1;//���벻��Ƶ
	TIM_ICInitStruct.TIM_ICFilter = 0x00;//���벻�˲�
	TIM_ICInit(TIM8,&TIM_ICInitStruct);//��ʼ��TIM8
	
	TIM_ICInitStruct.TIM_Channel = TIM_Channel_4;//�����ӳ�䵽TI4
	TIM_ICInitStruct.TIM_ICPolarity = TIM_ICPolarity_Rising;//�����ز���
	TIM_ICInitStruct.TIM_ICSelection = TIM_ICSelection_DirectTI;//ӳ�䵽TI4��
	TIM_ICInitStruct.TIM_ICPrescaler = TIM_ICPSC_DIV1;//���벻��Ƶ
	TIM_ICInitStruct.TIM_ICFilter = 0x00;//���벻�˲�
	TIM_ICInit(TIM8,&TIM_ICInitStruct);//��ʼ��TIM8
	
	TIM_ITConfig(TIM8,TIM_IT_Update|TIM_IT_CC1|TIM_IT_CC2|TIM_IT_CC3|TIM_IT_CC4,ENABLE);//����������ж� CC1IE�����ж�
	
	BSP_IntVectSet(TIM8_CC_IRQn, TIM8_CC_IRQHandler);
	BSP_IntEn(TIM8_CC_IRQn);
	
	TIM_Cmd(TIM8,ENABLE);//������ʱ��8
}

/*******************************************************************************
* Description  : ͨ��������ʼ��
                ��ʼ��ȫ�ֶ�ʱ���Ͳ���ͨ�����ⲿ�жϣ�
                �Լ����е�ȫ�ֱ�����
* Author       : 2018/3/13 ���ڶ�, by redmorningcn
*******************************************************************************/
void    init_ch_timepara_detect(void)
{
    //Detect_exti_init();     //�ⲿ���ͨ��
    
    //Detect_exti01_init();
    
    Timer8_Iint();          //����ȫ�ֶ�ʱ��
    
    Timer8_Configuration();
    
    //��ʼ���������
}


/*******************************************************************************
 * 				end of file
 *******************************************************************************/
