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
static list delayedList ;       // 延时的协程队列
static list overflowedList;   //   超时的
static list pendingReadyList;   // 待 ready 的, 这些协程不能直接放到 ready 队列, 因为他们不能中断

static list * pDelayed;
static list *pOverflowed;


CRCB_t * pCurrentCoroutine = NULL;

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

BaseType_t createCoroutine( crCOROUTINE_CODE code, UBaseType_t priority, UBaseType_t  index)
{
    CRCB_t * pxCoRoutine;
    pxCoRoutine  = (CRCB_t *) pvPortMalloc (sizeof(CRCB_t));
    if(pxCoRoutine){
        if(pCurrentCoroutine == NULL){
            pCurrentCoroutine - pxCoRoutine ;
            pListInit();
        }
        if(priority >= MAX_PRIORITY ){
            priority = MAX_PRIORITY-1 ;  //同理, 检查优先级是否超过最大优先级, 
            //此优先级作为数组的下标, 由于从0 开始, 所以 减去1
        }
        // 设置就绪队列协程的 一些基本数据 (通过传入的参数)
		pxCoRoutine->uxState = corINITIAL_STATE; 
		pxCoRoutine->uxPriority = priority ;
		pxCoRoutine->uxIndex = index; 
		pxCoRoutine->pxCoRoutineFunction = code;
        
        
        listInit( &(pxCoRoutine ->xGenericListItem ));
        listInit( &(pxCoRoutine ->xEventListItem  ));

        listPass(j &(pxCoRoutine ->xGenericListItem ),pxCoRoutine );
        listPass( &(pxCoRoutine ->xEventListItem  ),pxCoRoutine );
        
		listSET_LIST_ITEM_VALUE( &( pxCoRoutine->xEventListItem ), ( ( TickType_t )MAX_PRIORITY - ( TickType_t ) priority  ) );


        //所有准备工作完成, 可以加入就绪队列了(●ˇ∀ˇ●)
        listAppend( pxCoRoutine );

		rerturn  pdPASS;
    }
}
/*-----------------------------------------------------------*/
