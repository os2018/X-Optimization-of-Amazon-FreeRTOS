#include <stdlib.h>
#include <test.h>
FreeHeader_t *pxHeapStart = NULL;
int8_t* pxHeapEnd = NULL;
/* Block sizes must not get too small. */
#define heapMINIMUM_BLOCK_SIZE	( ( uint16_t ) ( xHeapStructSize << 1 ) )

/* Assumes 8bit bytes! */
#define heapBITS_PER_BYTE		( ( uint16_t ) 8 )




/*-----------------------------------------------------------*/

/*
* Inserts a block of memory that is being freed into the correct position in
* the list of free memory blocks.  The block being freed will be merged with
* the block in front it and/or the block behind it if the memory blocks are
* adjacent to each other.
*/
//static void prvInsertBlockIntoFreeList(BlockLink_t *pxBlockToInsert);
/* Get LOG2 / the highest bit 1*/
int16_t LOG2(uint16_t _val)
{
	int re;
	__asm bsr EAX, _val;
	__asm mov re, EAX;
	return (int16_t)re;
}
/*-----------------------------------------------------------*/
#define portBYTE_ALIGNMENT 4
#define portBYTE_ALIGNMENT_MASK 3
/* The size of the structure placed at the beginning of each allocated memory
block must by correctly byte aligned. */
static const uint16_t xHeapStructSize = (sizeof(UsedHeader_t) + ((uint16_t)(portBYTE_ALIGNMENT - 1))) & ~((uint16_t)portBYTE_ALIGNMENT_MASK);


/* Keeps track of the number of free bytes remaining, but says nothing about
fragmentation. */
static size_t xFreeBytesRemaining = 0U;
static size_t xMinimumEverFreeBytesRemaining = 0U;

/* Gets set to the t bit and f bit.  When t bit = 1 the block is used and when t bit = 0
the block is free. */
static const uint16_t xBlockTBit = 2;
/* When f bit = 1 the block is the end of the free list. */
static const uint16_t xBlockFBit = 1;
/* Add the block to FLI. */
void vADDtoFLI(FreeHeader_t *pxStartAddress);
/* Get the position in FLI according to a given size. */
int16_t xGetFLIPosition(uint16_t xSize);
/* Maintain the FLI and bitmap. */
void vGetFreeBlock(FreeHeader_t *pxTargetBlock);

/*-----------------------------------------------------------*/
int16_t xGetFLIPosition(uint16_t xSize)
{
	uint16_t xBitMapTemp;
	int16_t xFLIPosition, xlogSize = 0;
	xSize &= ~xBlockTBit;
	xSize &= ~xBlockFBit;
	xlogSize = LOG2(xSize - 1) + 1;
	xBitMapTemp = *pxBitMap << xlogSize;
	if (xBitMapTemp == 0)
	{
		return -1;
	}
	else
	{
		xFLIPosition = xlogSize + 12 - LOG2(xBitMapTemp);
		return xFLIPosition;
	}
}

/*-----------------------------------------------------------*/

void *pvPortMalloc(size_t xWantedSize)
{
	void *pvReturn = NULL;
	uint16_t xSize;
	int16_t xFLIPosition;
	FreeHeader_t *pxBlockStartAddress = NULL, *pxRemainAddress = NULL, *pxNextBlock = NULL;

	/* The heap must be initialised before the first call to
	prvPortMalloc(). */
	configASSERT(FLI);

	vTaskSuspendAll();
	{
		/* Check the requested block size is not so large */
		if ((xWantedSize & xSizeHighestBit) == 0)
		{
			/* The wanted size is increased so it can contain a header
			structure in addition to the requested amount of bytes. */
			if (xWantedSize > 0)
			{
				xWantedSize += xHeapStructSize;
				if (xWantedSize < sizeof(FreeHeader_t))
				{
					xWantedSize = sizeof(FreeHeader_t);
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
				/* Ensure that blocks are always aligned to the required number
				of bytes. */
				if ((xWantedSize & portBYTE_ALIGNMENT_MASK) != 0x00)
				{
					/* Byte alignment required. */
					xWantedSize += (portBYTE_ALIGNMENT - (xWantedSize & portBYTE_ALIGNMENT_MASK));
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			if ((xWantedSize > 0) && (xWantedSize <= xFreeBytesRemaining))
			{
				/* Size is already added by the header size. */
				xSize = (uint16_t)xWantedSize;
				/* First find the according place in the FLI. */
				xFLIPosition = xGetFLIPosition(xSize);
				if (xFLIPosition == -1)
				{
					return NULL;
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
				/* Get the actual address of the block. */
				pxBlockStartAddress = FLI[xFLIPosition];
				/* Set FLI and bitmap correct. */
				FLI[xFLIPosition] = pxBlockStartAddress->pxNextFreeBlock;
				if (FLI[xFLIPosition] == NULL)
				{
					*pxBitMap = (uint16_t)((*pxBitMap)&~((uint16_t)1 << (12 - xFLIPosition)));
				}
				else
				{
					FLI[xFLIPosition]->pxPrevFreeBlock = NULL;
				}
				/* Allocate the block. */
				pvReturn = (void*)&(pxBlockStartAddress->pxNextFreeBlock);

				if (xSize + heapMINIMUM_BLOCK_SIZE < ((pxBlockStartAddress->xBlockSize)&xSizeMask))
				{
					pxRemainAddress = (FreeHeader_t*)((int8_t*)pxBlockStartAddress + xSize);
					pxRemainAddress->pxPrevPhysBlock = pxBlockStartAddress;
					pxRemainAddress->xBlockSize = pxBlockStartAddress->xBlockSize - xSize;
					pxRemainAddress->xBlockSize = (uint16_t)(pxRemainAddress->xBlockSize&~portBYTE_ALIGNMENT_MASK);
					pxNextBlock = (FreeHeader_t*)((int8_t*)pxRemainAddress + pxRemainAddress->xBlockSize);
					pxNextBlock->pxPrevPhysBlock = pxRemainAddress;
					pxBlockStartAddress->xBlockSize = xSize;
					vADDtoFLI(pxRemainAddress);
					xFreeBytesRemaining -= xSize;
				}
				else
				{
					xFreeBytesRemaining -= pxBlockStartAddress->xBlockSize;
				}
				pxBlockStartAddress->xBlockSize = (uint16_t)(pxBlockStartAddress->xBlockSize | xBlockTBit);
				pxBlockStartAddress->xBlockSize = (uint16_t)(pxBlockStartAddress->xBlockSize&(~xBlockFBit));
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		traceMALLOC(pvReturn, xWantedSize);
	}
//	(void)xTaskResumeAll();

#if( configUSE_MALLOC_FAILED_HOOK == 1 )
	{
		if (pvReturn == NULL)
		{
			extern void vApplicationMallocFailedHook(void);
			vApplicationMallocFailedHook();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
#endif
//	printf("REAL SIZE:%d\n", pxBlockStartAddress->xBlockSize);
	return pvReturn;
}
/*-----------------------------------------------------------*/

void vGetFreeBlock(FreeHeader_t *pxTargetBlock)
{
	int16_t xFLIPosition;
	/* If the block is the first one in the FLI */
	if (pxTargetBlock->pxNextFreeBlock != NULL)
	{
		pxTargetBlock->pxNextFreeBlock->pxPrevFreeBlock = pxTargetBlock->pxPrevFreeBlock;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}
	if (pxTargetBlock->pxPrevFreeBlock != NULL)
	{
		pxTargetBlock->pxPrevFreeBlock->pxNextFreeBlock = pxTargetBlock->pxNextFreeBlock;
	}
	else
	{
		xFLIPosition = LOG2(pxTargetBlock->xBlockSize) - 3;
		if (FLI[xFLIPosition] == pxTargetBlock)
		{
			FLI[xFLIPosition] = pxTargetBlock->pxNextFreeBlock;
			if (FLI[xFLIPosition] == NULL)
			{
				*pxBitMap = (uint16_t)((*pxBitMap)&~((uint16_t)1 << (12 - xFLIPosition)));
			}
			else
			{
				FLI[xFLIPosition]->pxPrevFreeBlock = NULL;
			}
		}
		else
			printf("-&&&&&&&&&&&&&&&&&&&&ERROR&&&&&&&&&&&&&&&&&&&&&&&-\n");
	}
}
/*-----------------------------------------------------------*/

void vPortFree(void *pv)
{
	int8_t *puc = (int8_t *)pv;
	FreeHeader_t *pxTargetBlock = NULL, *pxBlockAfter = NULL;
	if (pv != NULL)
	{
		/* The memory being freed will have a header structure immediately
		before it. */
		puc -= xHeapStructSize;
		pxTargetBlock = (FreeHeader_t*)puc;
		/* Check the block is actually allocated. */
		configASSERT((pxTargetBlock->xBlockSize & xBlockTBit) != 0);
		if ((pxTargetBlock->xBlockSize & xBlockTBit) != 0)
		{
			pxTargetBlock->xBlockSize = (uint16_t)(pxTargetBlock->xBlockSize&~xBlockFBit);
			pxTargetBlock->xBlockSize = (uint16_t)(pxTargetBlock->xBlockSize&~xBlockTBit);
			xFreeBytesRemaining += pxTargetBlock->xBlockSize;
			/* If the previous physical block is free, combine them. */
			if (pxTargetBlock->pxPrevPhysBlock != NULL && (pxTargetBlock->pxPrevPhysBlock->xBlockSize&xBlockTBit) == 0)
			{
				vGetFreeBlock(pxTargetBlock->pxPrevPhysBlock);
				pxBlockAfter = (FreeHeader_t*)((int8_t*)pxTargetBlock + pxTargetBlock->xBlockSize);
				if ((int8_t*)pxBlockAfter < pxHeapEnd && (pxBlockAfter->pxPrevPhysBlock == pxTargetBlock))
				{
					pxBlockAfter->pxPrevPhysBlock = pxTargetBlock->pxPrevPhysBlock;
				}
				else if ((int8_t*)pxBlockAfter < pxHeapEnd)
				{
					printf("error!");
				}
				pxTargetBlock->pxPrevPhysBlock->xBlockSize += pxTargetBlock->xBlockSize & ~portBYTE_ALIGNMENT_MASK;
				pxTargetBlock = pxTargetBlock->pxPrevPhysBlock;
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
			/* If the block next is free, combine them */
			pxBlockAfter = (FreeHeader_t*)((int8_t*)pxTargetBlock + pxTargetBlock->xBlockSize);
			if ((int8_t*)pxBlockAfter < pxHeapEnd && ((pxBlockAfter->xBlockSize&xBlockTBit) == 0))
			{
				vGetFreeBlock(pxBlockAfter);
				pxTargetBlock->xBlockSize += pxBlockAfter->xBlockSize & ~portBYTE_ALIGNMENT_MASK;
				pxBlockAfter = (FreeHeader_t*)((int8_t*)pxBlockAfter + pxBlockAfter->xBlockSize);
				if ((int8_t*)pxBlockAfter < pxHeapEnd)
				{
					pxBlockAfter->pxPrevPhysBlock = pxTargetBlock;
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
			pxTargetBlock->xBlockSize = (uint16_t)(pxTargetBlock->xBlockSize&(~xBlockTBit));
			pxTargetBlock->xBlockSize = (uint16_t)(pxTargetBlock->xBlockSize&(~xBlockFBit));
			/* Put the new block back to FLI. */
			vADDtoFLI(pxTargetBlock);
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
}
/*-----------------------------------------------------------*/

void vADDtoFLI(FreeHeader_t *pxStartAddress)
{
	int16_t xFLIPosition;
	xFLIPosition = LOG2(pxStartAddress->xBlockSize) - 3;
	
	pxStartAddress->pxNextFreeBlock = FLI[xFLIPosition];
	pxStartAddress->pxPrevFreeBlock = NULL;
	if (FLI[xFLIPosition])
	{
		FLI[xFLIPosition]->pxPrevFreeBlock = pxStartAddress;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}
	FLI[xFLIPosition] = pxStartAddress;
	*pxBitMap = (uint16_t)(*pxBitMap | ((uint16_t)1 << (12 - xFLIPosition)));
}
/*-----------------------------------------------------------*/

size_t xPortGetFreeHeapSize(void)
{
	return xFreeBytesRemaining;
}
/*-----------------------------------------------------------*/

size_t xPortGetMinimumEverFreeHeapSize(void)
{
//	printf("BitMap: %d\n", *pxBitMap);
	return xMinimumEverFreeBytesRemaining;
}
/*-----------------------------------------------------------*/

void vPortDefineHeapRegions(const HeapRegion_t * const pxHeapRegions)
{
	size_t xTotalHeapSize = 0;
	int i;
	uint16_t xInfoSize = 0;
	uint16_t xBlocksSize = 0;
	int j = 0;

	/* Can only call once! */
	configASSERT(FLI == NULL);

	xTotalHeapSize = pxHeapRegions[0].xSizeInBytes;
	FLI = (FreeHeader_t**)pxHeapRegions[0].pucStartAddress;
	pxHeapEnd = (int8_t*)(pxHeapRegions[0].pucStartAddress + pxHeapRegions[0].xSizeInBytes);
	pxBitMap = (void*)(pxHeapRegions[0].pucStartAddress + 14 * sizeof(FreeHeader_t*));
	xInfoSize = (uint16_t)(13 * sizeof(FreeHeader_t*) + 2);
	*pxBitMap = (uint16_t)0;

	/* Set up FLI and initial blocks. */
	for (i = 3; i < 16; i++)
	{
		if (i <= 14)
		{
			FLI[i - 3] = NULL;
		}
		else
		{
			xBlocksSize = (uint16_t)xTotalHeapSize - ((uint16_t)1 << (LOG2(xInfoSize) + 1));
			xBlocksSize++;
			*pxBitMap = (uint16_t)(*pxBitMap | (uint16_t)1 << (15 - i));
			FLI[i - 3] = (void*)(pxHeapRegions[0].pucStartAddress + ((uint16_t)1 << (LOG2(xInfoSize) + 1)));
			if (j == 0)
			{
				pxHeapStart = FLI[i - 3];
				j = 1;
			}
			FLI[i - 3]->pxNextFreeBlock = NULL;
			FLI[i - 3]->pxPrevFreeBlock = NULL;
			FLI[i - 3]->pxPrevPhysBlock = NULL;
			FLI[i - 3]->xBlockSize = xBlocksSize;
		}
	}

	xFreeBytesRemaining = xTotalHeapSize - ((size_t)1 << (LOG2(xInfoSize) + 1));
	/* Check something was actually defined before it is accessed. */
	configASSERT(xTotalHeapSize);
}

/*-----------------------------------------------------------*/

void vCheckAllBlocks(void)
{
	FreeHeader_t *temp = NULL;
	FreeHeader_t *Start = pxHeapStart;
	while (1)
	{
		temp = Start;
		Start = (FreeHeader_t*)((temp->xBlockSize&~3) + (int8_t*)temp);
//		printf("BLOCK:   ADDRESS:%x----SIZE:%d", temp, temp->xBlockSize&~3);
//		if ((temp->xBlockSize&xBlockTBit) != 0)
//			printf("  IN USE!\n");
//		else
//			printf("  FREE!\n");
		if (Start == pxHeapEnd)
			return;
		if ((Start->pxPrevPhysBlock) == temp)
		{
			;
		}
		else
		{
			printf("error\n");
		}
	}
}