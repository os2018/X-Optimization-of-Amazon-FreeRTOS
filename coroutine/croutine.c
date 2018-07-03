#include "FreeRTOS.h"
#include "task.h"
#include "croutine.h"

static List_t pxReadyCoRoutineLists[ configMAX_CO_ROUTINE_PRIORITIES ];	// 协程队列, 处于 ready 状态, 有不同优先级
static List_t xDelayedCoRoutineList1;									/*两个延时队列, 一个是用来存放出国 Tickcount的 */
static List_t xDelayedCoRoutineList2;									
static List_t * pxDelayedCoRoutineList;									// 延时的协程队列
static List_t * pxOverflowDelayedCoRoutineList;							//   超时的
static List_t xPendingReadyCoRoutineList;								// 待 ready 的, 这些协程不能直接放到 ready 队列, 因为他们不能中断

CRCB_t * pxCurrentCoRoutine = NULL;
static UBaseType_t uxTopCoRoutineReadyPriority = 0;
static TickType_t xCoRoutineTickCount = 0, xLastTickCount = 0, xPassedTicks = 0;

/* 初始化状态
 *协程的编写, 使用宏, 使代码易于读懂, 而且节省代码量.
 *这里作用是将一个协程加入就绪队列, 首先判断 它的优先级是否高于规定的最大优先级, 如果高于, 则设为最大优先级
 */
#define corINITIAL_STATE	( 0 ) 
#define prvAddCoRoutineToReadyQueue( pxCRCB )																		\
{																													\
	if( pxCRCB->priority > uxTopCoRoutineReadyPriority )															\
	{																												\
		uxTopCoRoutineReadyPriority = pxCRCB->priority;															\
	}																												\
	vListInsertEnd( ( List_t * ) &( pxReadyCoRoutineLists[ pxCRCB->priority ] ), &( pxCRCB->xGenericListItem ) );	\
}

/*
 * 初始化所有队列,调度者在创建第一个队列时就自动调用
 */
static void prvInitialiseCoRoutineLists( void );
static void prvCheckPendingReadyList( void );

 /* 
  * 用于检查当前延时的协程是否需要唤醒的宏, 协程根据唤醒时间存储
  */
static void prvCheckDelayedList( void );


BaseType_t xCoRoutineCreate( crCOROUTINE_CODE code, UBaseType_t priority, UBaseType_t num )
{
    CRCB_t *pxCoRoutine;

	pxCoRoutine = ( CRCB_t * ) pvPortMalloc( sizeof( CRCB_t ) );
	if( pxCoRoutine )
	{
		if( pxCurrentCoRoutine == NULL )
		{
			pxCurrentCoRoutine = pxCoRoutine;
			prvInitialiseCoRoutineLists();
		}

		if( priority >= configMAX_CO_ROUTINE_PRIORITIES )
		{
			priority = configMAX_CO_ROUTINE_PRIORITIES - 1;
            //同理, 检查优先级是否超过最大优先级, 
            //此优先级作为数组的下标, 由于从0 开始, 所以 减去1
		}

		/* 设置就绪队列协程的 一些基本数据 (通过传入的参数) */
		pxCoRoutine->uxState = corINITIAL_STATE;
		pxCoRoutine->priority = priority;
		pxCoRoutine->num = num;
		pxCoRoutine->pxCoRoutineFunction = code;

		vListInitialiseItem( &( pxCoRoutine->xGenericListItem ) );
		vListInitialiseItem( &( pxCoRoutine->xEventListItem ) );
		listSET_LIST_ITEM_OWNER( &( pxCoRoutine->xGenericListItem ), pxCoRoutine );
		listSET_LIST_ITEM_OWNER( &( pxCoRoutine->xEventListItem ), pxCoRoutine );

		listSET_LIST_ITEM_VALUE( &( pxCoRoutine->xEventListItem ), ( ( TickType_t ) configMAX_CO_ROUTINE_PRIORITIES - ( TickType_t ) priority ) );

		//所有准备工作完成, 可以加入就绪队列了(●ˇ∀ˇ●)
		prvAddCoRoutineToReadyQueue( pxCoRoutine );

		return  pdPASS;
	}else{
		return  errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
	}
}

void addDelayed( TickType_t xTicksToDelay, List_t *pxEventList )
{
    TickType_t xTimeToWake;
	xTimeToWake = xCoRoutineTickCount + xTicksToDelay;

	/*  加入阻塞队列前, 先从就绪队列去除*/
	( void ) uxListRemove( ( ListItem_t * ) &( pxCurrentCoRoutine->xGenericListItem ) );

	/* 按唤醒时间排序插入 */
	listSET_LIST_ITEM_VALUE( &( pxCurrentCoRoutine->xGenericListItem ), xTimeToWake );

	if( xTimeToWake < xCoRoutineTickCount )
	{
		//这种情况发生溢出, 将其加入溢出队列
		vListInsert( ( List_t * ) pxOverflowDelayedCoRoutineList, ( ListItem_t * ) &( pxCurrentCoRoutine->xGenericListItem ) );
	}else{
		//否则加入延迟队列
		vListInsert( ( List_t * ) pxDelayedCoRoutineList, ( ListItem_t * ) &( pxCurrentCoRoutine->xGenericListItem ) );
	}

	if( pxEventList )
	{
		/* 如果有事件, 则加入事件队列*/
		vListInsert( pxEventList, &( pxCurrentCoRoutine->xEventListItem ) );
	}
}

static void prvCheckPendingReadyList( void )
{
	while( listLIST_IS_EMPTY( &xPendingReadyCoRoutineList ) == pdFALSE )
	{
		CRCB_t *pxUnblockedCRCB;

		// 使用协程
		portDISABLE_INTERRUPTS();
		{
			pxUnblockedCRCB = ( CRCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( (&xPendingReadyCoRoutineList) );
			( void ) uxListRemove( &( pxUnblockedCRCB->xEventListItem ) );
		}
		portENABLE_INTERRUPTS();

		( void ) uxListRemove( &( pxUnblockedCRCB->xGenericListItem ) );
		prvAddCoRoutineToReadyQueue( pxUnblockedCRCB );
	}
}

static void prvCheckDelayedList( void )
{
CRCB_t *pxCRCB;

	xPassedTicks = xTaskGetTickCount() - xLastTickCount;
	while( xPassedTicks )
	{
		xCoRoutineTickCount++;
		xPassedTicks--;

		/* 如果 Tick 发生溢出, 交换 delayed 与 overflowed*/
		if( xCoRoutineTickCount == 0 )
		{
			List_t * pxTemp;
			pxTemp = pxDelayedCoRoutineList;
			pxDelayedCoRoutineList = pxOverflowDelayedCoRoutineList;
			pxOverflowDelayedCoRoutineList = pxTemp;
		}

		/* 检查 Tick 是否用完 */
		while( listLIST_IS_EMPTY( pxDelayedCoRoutineList ) == pdFALSE )
		{
			pxCRCB = ( CRCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxDelayedCoRoutineList );

			if( xCoRoutineTickCount < listGET_LIST_ITEM_VALUE( &( pxCRCB->xGenericListItem ) ) )
			{
				/* 超时了*/
				break;
			}
        
            // 使用宏定义的协程
			portDISABLE_INTERRUPTS();
			{
				( void ) uxListRemove( &( pxCRCB->xGenericListItem ) );
				if( pxCRCB->xEventListItem.pvContainer )
				{
					( void ) uxListRemove( &( pxCRCB->xEventListItem ) );
				}
			}
			portENABLE_INTERRUPTS();

			prvAddCoRoutineToReadyQueue( pxCRCB );
		}
	}

	xLastTickCount = xCoRoutineTickCount;
}

void schedule( void )
{
	/*调度函数 */
	prvCheckPendingReadyList();

	/* 检查延迟的协程是否时间用完了 */
	prvCheckDelayedList();
	while( listLIST_IS_EMPTY( &( pxReadyCoRoutineLists[ uxTopCoRoutineReadyPriority ] ) ) )
	{
		if( uxTopCoRoutineReadyPriority == 0 )
		{
			return;
		}
		--uxTopCoRoutineReadyPriority;
	}
	listGET_OWNER_OF_NEXT_ENTRY( pxCurrentCoRoutine, &( pxReadyCoRoutineLists[ uxTopCoRoutineReadyPriority ] ) );
	( pxCurrentCoRoutine->pxCoRoutineFunction )( pxCurrentCoRoutine, pxCurrentCoRoutine->num );
	return;
}

static void prvInitialiseCoRoutineLists( void )
{
        /* 全部初始化工作 ,注意 与 listInit 的区别,后者只是对一个队列的初始化*/
    UBaseType_t priority;

	for( priority = 0; priority < configMAX_CO_ROUTINE_PRIORITIES; priority++ )
	{
		vListInitialise( ( List_t * ) &( pxReadyCoRoutineLists[ priority ] ) );
	}

	vListInitialise( ( List_t * ) &xDelayedCoRoutineList1 );
	vListInitialise( ( List_t * ) &xDelayedCoRoutineList2 );
	vListInitialise( ( List_t * ) &xPendingReadyCoRoutineList );
	pxDelayedCoRoutineList = &xDelayedCoRoutineList1;
	pxOverflowDelayedCoRoutineList = &xDelayedCoRoutineList2;
}

BaseType_t removeEvent( const List_t *pxEventList )
{
CRCB_t *pxUnblockedCRCB;
BaseType_t xReturn;

	/* 将协程任务 从事件队列中去除*/
	pxUnblockedCRCB = ( CRCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxEventList );
	( void ) uxListRemove( &( pxUnblockedCRCB->xEventListItem ) );
	vListInsertEnd( ( List_t * ) &( xPendingReadyCoRoutineList ), &( pxUnblockedCRCB->xEventListItem ) );

	if( pxUnblockedCRCB->priority >= pxCurrentCoRoutine->priority )
	{
		xReturn = pdTRUE;
	}
	else
	{
		xReturn = pdFALSE;
	}

	return xReturn;
}
