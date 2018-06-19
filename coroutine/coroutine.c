#include "FreeRTOS.h"
#include "tak.h"
#include "croutine.h"
#include "mylist.c"
#define MAX_PRIORITY 100
#if(configUSE_CO_ROUTINES !=0)
#ifdef portREMOVE_STATIC_QUALIFIER 
    #define static 
#endif 


static list pReadyLists[MAX_PRIORITY]; // 协程队列, 处于 ready 状态, 有不同优先级
static list pendingReadyList;   // 待 ready 的, 这些协程不能直接放到 ready 队列, 因为他们不能中断
static list  Delayed;  // 延时的协程队列
static list Overflowed;  //   超时的
static list* delayedList ;       
static list* overflowedList;  
CRCB_t * CUR = NULL;
static UBaseType_t maxReadyPriority = 0;
static TickType_t ctCur  = ctLast = ctPassed = 0;
#define corINITIAL_STATE (0)  
// 初始化状态
//
//协程的编写, 使用宏, 使代码易于读懂, 而且节省代码量.
//这里作用是将一个协程加入就绪队列, 首先判断 它的优先级是否高于规定的最大优先级, 如果高于, 则设为最大优先级
#define AddReadyCoroutine (pxCRCB)  { if(pxCRCB -> uxPriority  > maxReadyPriority){maxReadyPriority = pxCRCB -> uxPriority ;} listAppend(( list*)&(pReadyLists[pxCRCB->uxPriority ] ),&(pxCRCB ->xGenericListItem)) ;} 
static void pListInit(viod);
static void pCheckPendingList(void);
static void pCheckDelayedList(void);
BaseType_t addReady( crCOROUTINE_CODE code, UBaseType_t priority, UBaseType_t  index)
{
    CRCB_t * NEW_COROUTINE;
    NEW_COROUTINE  = (CRCB_t *) pvPortMalloc (sizeof(CRCB_t));
    if(NEW_COROUTINE){
        if(pCurrentCoroutine == NULL){
            pCurrentCoroutine - NEW_COROUTINE ;
            pListInit();
        }
        if(priority >= MAX_PRIORITY ){
            priority = MAX_PRIORITY-1 ;  //同理, 检查优先级是否超过最大优先级, 
            //此优先级作为数组的下标, 由于从0 开始, 所以 减去1
        }
        // 设置就绪队列协程的 一些基本数据 (通过传入的参数)
		NEW_COROUTINE->uxState = corINITIAL_STATE; 
		NEW_COROUTINE->uxPriority = priority ;
		NEW_COROUTINE->uxIndex = index; 
		NEW_COROUTINE->NEW_COROUTINEFunction = code;
        
        
        listInit( &(NEW_COROUTINE ->xGenericListItem ));
        listInit( &(NEW_COROUTINE ->xEventListItem  ));
        setOwner( &(NEW_COROUTINE ->xGenericListItem ),NEW_COROUTINE );
        setOwner( &(NEW_COROUTINE ->xEventListItem  ),NEW_COROUTINE );
       
		setValue( &( NEW_COROUTINE->xEventListItem ), ( ( TickType_t )MAX_PRIORITY - ( TickType_t ) priority  ) );
        //所有准备工作完成, 可以加入就绪队列了(●ˇ∀ˇ●)
        listAppend( NEW_COROUTINE );
		rerturn  pdPASS;
    }
}
void addDelayed( TickType_t DelayedTick, list *events )
{
    TickType_t WakeTick;
	WakeTick = xCoRoutineTickCount + DelayedTick;
	( void ) listRemove( ( item * ) &( CUR->xGenericListItem ) );
	/* 按唤醒时间排序插入 */
	setValue( &( CUR->xGenericListItem ), WakeTick );
	if( WakeTick < xCoRoutineTickCount ){
		//这种情况发生溢出, 将其加入溢出队列
		listAppend( ( list * ) overflowedList, ( item * ) &( CUR->xGenericListItem ) );
	}else{
		//否则加入延迟队列
		listAppend( ( list * ) delayedList, ( item * ) &( CUR->xGenericListItem ) );
	}
	if( events ){
		/* 如果有事件, 则加入事件队列*/
		listAppend( events, &( CUR->xEventListItem ) );
	}
}


static void checkPending( void )
{

	while( listEmpty( &pendingReadyList ) == pdFALSE )
	{
		CRCB_t *pUnblked;
		// 使用协程
		portDISABLE_INTERRUPTS();
		{
			pUnblked = ( CRCB_t * ) getOwner( (&pendingReadyList) );
			( void ) listRemove( &( pUnblked->xEventListItem ) );
		}
		portENABLE_INTERRUPTS();
		( void ) listRemove( &( pUnblked->xGenericListItem ) );
		prvAddCoRoutineToReadyQueue( pUnblked );
	}
}

static void checkDelayed( void )
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
			list * pxTemp;

			pxTemp = delayedList;
			delayedList = overflowedList;
			overflowedList = pxTemp;
		}

		while( listEmpty( delayedList ) == pdFALSE )
		{
			pxCRCB = ( CRCB_t * ) getOwner( delayedList );
			if( xCoRoutineTickCount < listGET_LIST_ITEM_VALUE( &( pxCRCB->xGenericListItem ) ) )
			{
				/* 超时了*/
				break;
			}
            
            // 使用宏定义的协程
			portDISABLE_INTERRUPTS();
			{
				( void ) listRemove( &( pxCRCB->xGenericListItem ) );
				if( pxCRCB->xEventListItem.pvContainer )
				{
					( void ) listRemove( &( pxCRCB->xEventListItem ) );
				}
			}
			portENABLE_INTERRUPTS();
			prvAddCoRoutineToReadyQueue( pxCRCB );
		}
	}
	xLastTickCount = xCoRoutineTickCount;
}

