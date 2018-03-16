/*******************************************************************************
 *   Filename:       bsp_ds3231.h
 *   Revised:        All copyrights reserved to Roger.
 *   Date:           2016-11-06
 *   Revision:       v1.0
 *   Writer:	     wumingshen.
 *
 *   Description:    ʵʱʱ��ģ�� ͷ�ļ�
 *                   
 *
 *
 *   Notes:
 *
 *   All copyrights reserved to wumingshen
 *******************************************************************************/
#ifndef	__BSP_DS3231_H__
#define	__BSP_DS3231_H__

/*******************************************************************************
 * INCLUDES
 */
 #include  <global.h>

/*******************************************************************************
 * MACROS
 */

/***********************************************
 * ��������ֵ����
 */

/***********************************************
 * ������
 */

/*******************************************************************************
 * TYPEDEFS
 */
 /***********************************************
 * �������������Ͷ���
 */
typedef struct
{
	uint8_t				Year;							//?����??����Y
	uint8_t				Month;							//?����??����Y
	uint8_t				Day;							//?����??����Y
	uint8_t				Hour;							//?����??����Y
	uint8_t				Min;							//?����??����Y
	uint8_t				Sec;							//?����??����Y
}TIME;



/*******************************************************************************
 * ������ �ⲿ��������
 */
extern void     GPIO_RST_CLK_Configuration  (void);
extern uint8_t  BCD2HEX                     (uint8_t Bcd);
extern uint8_t  HEX2BCD                     (uint8_t Hex);
extern uint8_t  ReadDS3231Byte              (uint8_t addr);
extern void     WriteDS3231Byte             (uint8_t Addr,uint8_t Data);
extern  float   ReadTemp                    (void);
extern void     WriteTime                   (TIME  sTime);
extern void     SetTime                     (uint8_t Year,uint8_t Month,uint8_t Day,uint8_t Hour,uint8_t Min,uint8_t Sec);  
extern void     InitDS3231                  (void);  
extern void     DisplayTime                 (void);
extern void     GetTime                     (TIME *t);

#endif
 /*******************************************************************************
 * 				end of file
 *******************************************************************************/





