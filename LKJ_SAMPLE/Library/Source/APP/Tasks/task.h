

/*task �����¼���Ӧ�ص��������Ͷ���*/
typedef void(* TASK_EVENT_CALLBACK)	(void *data, uint16 len);

typedef struct
{
uint16 event;
TASK_EVENT_CALLBACK p_func;
}ST_TASK_EVENT_TBL_TYPE;

