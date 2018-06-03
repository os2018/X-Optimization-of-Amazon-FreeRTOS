#include <stdint.h>
#include <stdio.h>
#include <time.h>
#define configASSERT()
#define vTaskSuspendAll()
#define mtCOVERAGE_TEST_MARKER()
#define traceMALLOC()
#define xTaskResumeAll()
/* Define the block header of free blocks. */
typedef struct FREE_BLOCK_HEADER
{
	uint16_t xBlockSize;									/* Block size. Include the header size. */
	struct FREE_BLOCK_HEADER *pxPrevPhysBlock;				/* The address of the block before this one. */
	struct FREE_BLOCK_HEADER *pxNextFreeBlock;				/* The address of the next free block. */
	struct FREE_BLOCK_HEADER *pxPrevFreeBlock;				/* The address of the previous free block. */
}FreeHeader_t;
/* Define the block header of used blocks */
typedef struct USED_BLOCK_HEADER
{
	uint16_t xBlockSize;									/* Block size. Include the header size. */
	struct FREE_BLOCK_HEADER *pxPrevPhysBlock;				/* The address of the block before this one. */
}UsedHeader_t;
/* Define the First Level Index. */
static FreeHeader_t **FLI = NULL;
/* Define the bitmap. */
static uint16_t *pxBitMap = NULL;
static const uint16_t xSizeHighestBit = (uint16_t)0x8000;
static const uint16_t xSizeMask = (uint16_t)~3;

typedef struct assd {
	int8_t *pucStartAddress;
	size_t xSizeInBytes;
}HeapRegion_t;

void *pvPortMalloc(size_t xWantedSize);
void vPortFree(void *pv);
void vPortDefineHeapRegions(const HeapRegion_t * const pxHeapRegions);
int16_t LOG2(uint16_t _val);
void vCheckAllBlocks(void);
