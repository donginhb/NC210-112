/*******************************************************************************
 *   Filename:       bsp_adc.c
 *   Revised:        All copyrights reserved to Roger.
 *   Date:           2014-08-11
 *   Revision:       v1.0
 *   Writer:	     wumingshen.
 *
 *   Description:    ADCģ��ת��ģ�飨ʹ��DMA�������ݣ�
 *
 *
 *   Notes:
 *
 *   All copyrights reserved to wumingshen
 *******************************************************************************/
#include "bsp_adc.h"
#include "bsp.h"
#include "app_ctrl.h"
#include "..\tasks\task_comm.h"

__IO uint16_t ADC_Value[Channel_Times][Channel_Number];
__IO uint16_t ADC_AverageValue[Channel_Number];


/*****************************************************************************************************/
/* EXTERN VARIABLES*/
extern OS_Q                DET_RxQ;


//============================================================================//
void detect_vol_dma_isr_handler(void);


#ifndef SAMPLE_BOARD

/*******************************************************************************
 * ��    �ƣ� ADCx_GPIO_Config
 * ��    �ܣ� ��ʼ��ADCʹ�õ�����
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ���� ���ߣ� ������.
 * �������ڣ� 2015-06-25
 * ��    �ļ�¼��
 2017-5-3	fourth		ADC�������Ÿ�ΪPB0\PC0~PC5
 *******************************************************************************/
static void ADCx_GPIO_Config(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* Enable DMA clock */
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
#ifdef SAMPLE_BOARD

	/* Enable ADC1 and GPIOC clock */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 |RCC_APB2Periph_GPIOC, ENABLE);
	GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_AIN;
	GPIO_InitStructure.GPIO_Pin     = GPIO_Pin_0
                                    | GPIO_Pin_1
                                    | GPIO_Pin_2
                                    | GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_AIN;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
#else

	/* Enable ADC1 and GPIOB GPIOC clock */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOB|RCC_APB2Periph_GPIOC, ENABLE);
	GPIO_InitStructure.GPIO_Pin     = GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_AIN;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin     = GPIO_Pin_0
								| GPIO_Pin_1
								| GPIO_Pin_2
								| GPIO_Pin_3
								| GPIO_Pin_4
								| GPIO_Pin_5;
	GPIO_InitStructure.GPIO_Mode    = GPIO_Mode_AIN;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
#endif
	
}

/*******************************************************************************
 * ��    �ƣ� ADCx_Mode_Config
 * ��    �ܣ� ����ADCx�Ĺ���ģʽ
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ���� ���ߣ� ������.
 * �������ڣ� 2015-06-25
 * ��    �ģ�
 * �޸����ڣ�
 * ��    ע��
 *******************************************************************************/
static void  ADCx_Mode_Config(void)
{
    DMA_InitTypeDef DMA_InitStructure;
    ADC_InitTypeDef ADC_InitStructure;

    /* DMA channel1 configuration */
    //��DMA����ͨ��1
    DMA_DeInit(DMA1_Channel1);

    //����DMAԴ���ڴ��ַ���ߴ������ݼĴ�����ַ
    DMA_InitStructure.DMA_PeripheralBaseAddr = ADC1_DR_Address;	    //ADC��ַ
    //�ڴ��ַ��Ҫ����ı�����ָ�룩
    DMA_InitStructure.DMA_MemoryBaseAddr = (u32)&ADC_Value;         //�ڴ��ַ
    //���򣺵�����
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    //���ô���ʱ�������ĳ��ȣ�1����һ��Half-word16λ��
    DMA_InitStructure.DMA_BufferSize = Channel_Times * Channel_Number;
    //�����ַ�̶�
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;//�����ַ�̶�
    //�ڴ��ַ����
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;        //�ڴ��ַ�̶�
    //DMA�ڷ���ÿ�β��� ���ݳ���
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;//���֣�16λ��
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_HalfWord;
    //����DMA�Ĵ��䷽ʽ��ѭ������ģ��
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;		//ѭ������ģʽ
    //DMAͨ��x�����ȵȼ�:��
    DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
    //DMAͨ��x��ֹ�ڴ浽�ڴ�Ĵ���
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    //��ʼ��DMA
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);

    /* Enable DMA channel1 */
    //ʹ��DMA
    DMA_Cmd(DMA1_Channel1, ENABLE);
	DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE); //ʹ��DMA��������ж� 
	
    /* ADC1 configuration */
    ADC_DeInit(ADC1); //������ ADC1 ��ȫ���Ĵ�������Ϊȱʡֵ

    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;	    //����ģʽADCģʽ
    ADC_InitStructure.ADC_ScanConvMode = ENABLE ; 	        //����ɨ��ģʽ��ɨ��ģʽ���ڶ�ͨ���ɼ�
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;	    //��������ת��ģʽ������ͣ�ؽ���ADCת��
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;	//��ʹ���ⲿ����ת��
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right; 	//�ɼ������Ҷ���
    ADC_InitStructure.ADC_NbrOfChannel = Channel_Number;	//Ҫת����ͨ����ĿChannel_Number
    ADC_Init(ADC1, &ADC_InitStructure);

    //----------------------------------------------------------------------
    //ADC��ת��ʱ����ADC��ʱ�ӺͲ���������أ������������ADCת��ʱ��ĺ���
    //ADC����ʱ����㹫ʽ��T = ��������+12.5������

    /*����ADCʱ�ӣ�ΪPCLK2��6��Ƶ����12MHz*/
    //ADCʱ��Ƶ��Խ�ߣ�ת���ٶ�Խ�죬��ADCʱ��������ֵ��������14MHZ��
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);
    /*����ADC1��ͨ��11Ϊ55.	5���������ڣ�����Ϊ1 */
    //RANKֵ��ָ�ڶ�ͨ��ɨ��ģʽʱ����ͨ����ɨ��˳��
#ifdef SAMPLE_BOARD
    ADC_RegularChannelConfig(ADC1, ADC_Channel_11, 1, ADC_SampleTime_239Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_12, 2, ADC_SampleTime_239Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_13, 3, ADC_SampleTime_239Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_14, 4, ADC_SampleTime_239Cycles5);
#else
    ADC_RegularChannelConfig(ADC1, ADC_Channel_11, 1, ADC_SampleTime_239Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_12, 2, ADC_SampleTime_239Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_13, 3, ADC_SampleTime_239Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_14, 4, ADC_SampleTime_239Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_15, 5, ADC_SampleTime_239Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_10, 6, ADC_SampleTime_239Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_8, 	7, ADC_SampleTime_239Cycles5);
#endif
    //----------------------------------------------------------------------

    /* Enable ADC1 DMA */
    //ʹ��ADC1 ��DMA
    ADC_DMACmd(ADC1, ENABLE);

    /* Enable ADC1 */
    //ʹ��ADC
    ADC_Cmd(ADC1, ENABLE);

    //----------------------------------------------------------------------
    //�ڿ�ʼADCת��֮ǰ����Ҫ����ADC����У׼
    /*��λУ׼�Ĵ��� */
    ADC_ResetCalibration(ADC1);
    /*�ȴ�У׼�Ĵ�����λ��� */
    while(ADC_GetResetCalibrationStatus(ADC1));

    /* ADCУ׼ */
    ADC_StartCalibration(ADC1);
    /* �ȴ�У׼���*/
    while(ADC_GetCalibrationStatus(ADC1));
    BSP_IntVectSet(BSP_INT_ID_DMA1_CH1, detect_vol_dma_isr_handler);
    BSP_IntEn(BSP_INT_ID_DMA1_CH1);
    /* ����û�в����ⲿ����������ʹ���������ADCת�� */
	ADC_SoftwareStartConvCmd(ADC1, DISABLE);
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}


/*******************************************************************************
 * ��    �ƣ� ADC1Convert_Begin
 * ��    �ܣ� ��ʼADC1�Ĳɼ���ת��
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ���� ���ߣ� ������.
 * �������ڣ� 2015-06-25
 * ��    �ģ�
 * �޸����ڣ�
 * ��    ע��
 *******************************************************************************/
void ADC1Convert_Begin(void)
{
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
//	ADC_Cmd(ADC1,ENABLE);
}


/*******************************************************************************
 * ��    �ƣ� ADC1Convert_Stop
 * ��    �ܣ� ֹͣADC1�Ĳɼ���ת��
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ���� ���ߣ� ������.
 * �������ڣ� 2015-06-25
 * ��    �ģ�
 * �޸����ڣ�
 * ��    ע��
 *******************************************************************************/
void ADC1Convert_Stop(void)
{
    ADC_SoftwareStartConvCmd(ADC1, DISABLE);
//	ADC_Cmd(ADC1,DISABLE);

}


/*******************************************************************************
 * ��    �ƣ� Get_AD_AverageValue
 * ��    �ܣ� ��ȡAD�ɼ���ƽ��ֵ
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ���� ���ߣ� ������.
 * �������ڣ� 2015-06-25
 * ��    �ģ�
 * �޸����ڣ�
 * ��    ע��
 *******************************************************************************/
void Get_AD_AverageValue(void)
{
    uint8_t count;
    uint8_t i;
    uint32_t sum = 0;

    for(i=0;i<Channel_Number;i++) {
        for(count=0;count<Channel_Times;count++) {
            sum += ADC_Value[count][i];
        }
        ADC_AverageValue[i] = sum/Channel_Times;
        sum = 0;
    }
}
/*******************************************************************************
 * ��    �ƣ� Get_ADC_Value
 * ��    �ܣ� 
 * ��ڲ����� ch ADC����ͨ��
 * ���ڲ����� ADC�������ѹֵ����λΪmV
 * �������ߣ� fourth.
 * �������ڣ� 2017-5-3
 * ��    �ģ�
 * �޸����ڣ�
 *******************************************************************************/
uint16 Get_ADC_Value(uint8 ch)
{
	return ADC_AverageValue[ch]*3300/4095;
}
/*******************************************************************************
 * ��    �ƣ� detect_vol_dma_isr_handler
 * ��    �ܣ�DMAͨ��1�жϺ���
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ��    �ߣ�	 fourth.peng
 * �������ڣ� 2017-05-6
 * ��    �ļ�¼:
 *******************************************************************************/
void detect_vol_dma_isr_handler(void)
{
	OS_ERR err;
	static ST_QUEUETCB mailbox = {0};
	static uint16_t adc_value[Channel_Times][Channel_Number] = {0};
	
	if(DMA_GetITStatus(DMA1_IT_TC1) != RESET) 
	{
	memcpy((void *)&adc_value, (void *)&ADC_Value, sizeof(adc_value));
	mailbox.event = DETECT_TASK_VOL_ADC_UPDATE;
	mailbox.queue_data.pdata = &adc_value;
	mailbox.queue_data.len = sizeof(adc_value);
//	OSQPost(&DET_RxQ, &mailbox, sizeof(mailbox), OS_OPT_POST_FIFO, &err);
	
	DMA_ClearITPendingBit(DMA1_IT_TC1);
#ifdef SAMPLE_BOARD
//	ADC1Convert_Stop();
#endif
	}

}
#endif
void ADC_Mode_Config(void)
{
	ADC_InitTypeDef ADC_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC|RCC_APB2Periph_ADC1,ENABLE);//ʹ��ADC1ʱ��ͨ��
	RCC_ADCCLKConfig(RCC_PCLK2_Div6);//���÷�Ƶ����6 72M/6=12M������ܳ���14M
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;//ģ������
	GPIO_Init(GPIOC,&GPIO_InitStructure);//��ʼ��
	
	ADC_DeInit(ADC1);//��λADC1��������ADC1��ȫ���Ĵ������Ϊȱʡֵ
	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;//ADC����ģʽ
	ADC_InitStructure.ADC_ScanConvMode = DISABLE;//��ͨ��ģʽ
	ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;//����ת��
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;//ת�����������
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;//ADC�����Ҷ���
	ADC_InitStructure.ADC_NbrOfChannel = 1;//˳�����ת����ͨ����
	
	ADC_Init(ADC1,&ADC_InitStructure);//����ָ���Ĳ�����ʼ��ADC
	ADC_Cmd(ADC1,ENABLE);//ʹ��ADC1
	ADC_ResetCalibration(ADC1);//������λУ׼
	while(ADC_GetResetCalibrationStatus(ADC1));//�ȴ���λ����
	ADC_StartCalibration(ADC1);//����ADУ׼
	while(ADC_GetCalibrationStatus(ADC1));//�ȴ�У׼����
}


/*******************************************************************************
* Description  : ��ȡADCֵ������ת�����̲���Ӳ�ȴ������������
* Author       : 2018/1/24 ������, by redmorningcn
*******************************************************************************/
uint16_t Get_ADC(uint8_t ch)//chΪͨ����
{
    //����ָ��ADC�Ĺ�����ͨ�����������ǵ�ת��˳��Ͳ���ʱ��
    uint32  tmp32;
    
	ADC_RegularChannelConfig(ADC1,ch,1,ADC_SampleTime_7Cycles5);//ͨ��1����������Ϊ7.5����+12.5
	ADC_SoftwareStartConvCmd(ADC1,ENABLE);//ʹ��ָ����ADC1�����ת������
    /*******************************************************************************
    * Description  : �ȴ�ADCת��
    * Author       : 2018/3/29 ������, by redmorningcn
    *******************************************************************************/
	while(!ADC_GetFlagStatus(ADC1,ADC_FLAG_EOC));   //�ȴ�ת������
    
    tmp32 = ADC_GetConversionValue(ADC1);           //ȡת��ֵ
    
    tmp32 = (tmp32 * 3300)/4096;                    //���ز�����ѹֵ����λmV
    
	return  tmp32;
}

/*******************************************************************************
 * ��    �ƣ� Bsp_ADC_Init
 * ��    �ܣ� ADC��ʼ��
 * ��ڲ����� ��
 * ���ڲ����� ��
 * ���� ���ߣ� ������.
 * �������ڣ� 2015-06-25
 * ��    �ģ�
 * �޸����ڣ�
 * ��    ע��
 *******************************************************************************/
void Bsp_ADC_Init(void)
{
//	ADCx_GPIO_Config();
//	ADCx_Mode_Config();
	ADC_Mode_Config();
}

/*******************************************************************************
 *              end of file                                                    *
 *******************************************************************************/
