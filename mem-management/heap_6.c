/*
* FreeRTOS Kernel V10.0.1
* Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to
* use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
* the Software, and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
* FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
* COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
* IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
* http://www.FreeRTOS.org
* http://aws.amazon.com/freertos
*
* 1 tab == 4 spaces!
*/
/****************************************************
Makes the following changes :
* In main.c:static void  prvInitialiseHeap( void )
	static uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
	volatile uint32_t ulAdditionalOffset = 0;
	const HeapRegion_t xHeapRegions[] =
	{
		{ ucHeap, configTOTAL_HEAP_SIZE },
		{ NULL, 0 }
	};
	configASSERT( configTOTAL_HEAP_SIZE );

* In FreeRTOSConfig.h:
	#define configTOTAL_HEAP_SIZE					( ( size_t ) ( 64 * 1024 ) )
*****************************************************/
#include <stdlib.h>

/* Defining MPU_WRAPPERS_INCLUDED_FROM_API_FILE prevents task.h from redefining
all the API functions to use the MPU wrappers.  That should only be done when
task.h is included from an application file. */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include "FreeRTOS.h"
#include "task.h"

#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#if( configSUPPORT_DYNAMIC_ALLOCATION == 0 )
#error This file must not be used if configSUPPORT_DYNAMIC_ALLOCATION is 0
#endif

/* Block sizes must not get too small. */
#define heapMINIMUM_BLOCK_SIZE	( ( uint16_t ) ( xHeapStructSize << 1 ) )

/* Assumes 8bit bytes! */
#define heapBITS_PER_BYTE		( ( uint16_t ) 8 )


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
/* The highest bit of uint16 */
static const uint16_t xSizeHighestBit = (uint16_t)0x8000;
/* Gets set to the t bit and f bit.  When t bit = 1 the block is used and when t bit = 0
the block is free. */
static const uint16_t xBlockTBit = 2;
/* When f bit = 1 the block is the end of the free list. */
static const uint16_t xBlockFBit = 1;

/*-----------------------------------------------------------*/

/* Get LOG2 / the highest bit 1*/
int16_t LOG2(uint16_t _val)
{
	int re;
	__asm bsr EAX, _val;
	__asm mov re, EAX;
	return (int16_t)re;
}
/*-----------------------------------------------------------*/

/* The size of the structure placed at the beginning of each allocated memory
block must by correctly byte aligned. */
static const uint16_t xHeapStructSize = (sizeof(UsedHeader_t) + ((uint16_t)(portBYTE_ALIGNMENT - 1))) & ~((uint16_t)portBYTE_ALIGNMENT_MASK);


/* Keeps track of the number of free bytes remaining, but says nothing about
fragmentation. */
static size_t xFreeBytesRemaining = 0U;
static size_t xMinimumEverFreeBytesRemaining = 0U;				/* NOT USED! */
/* Records the end of the heap. */
int8_t* pxHeapEnd = NULL;
/* Define the First Level Index. */
static FreeHeader_t **FLI = NULL;
/* Define the bitmap. */
static uint16_t *pxBitMap = NULL;
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

				if (xSize + heapMINIMUM_BLOCK_SIZE < ((pxBlockStartAddress->xBlockSize)&~portBYTE_ALIGNMENT_MASK))
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
	(void)xTaskResumeAll();

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
		{
			mtCOVERAGE_TEST_MARKER();
		}
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
				else
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
	return xMinimumEverFreeBytesRemaining;
}
/*-----------------------------------------------------------*/

void vPortDefineHeapRegions(const HeapRegion_t * const pxHeapRegions)
{
	size_t xTotalHeapSize = 0;
	int i;
	uint16_t xInfoSize = 0;
	uint16_t xBlocksSize = 0;

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