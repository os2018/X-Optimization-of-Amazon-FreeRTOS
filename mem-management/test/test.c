#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <test.h>
#include <conio.h>

int getrand(int max)
{
	return rand() % max;
}

int main()
{
	static int8_t* list[40];
	int i, x, j;
	size_t size;
	int8_t HEAP[65536];
	HeapRegion_t hp = { HEAP, 65536 };
	vPortDefineHeapRegions(&hp);
	vCheckAllBlocks(&(HEAP[65535]));
	srand(time(NULL));
	j = 0;
	while (1)
	{
		x = getrand(2);
		if (x == 1)
		{
			size = getrand(4095) + 1;
			printf("GET MEM: SIZE %d\n", size);
			for (i = 0; i < 40; i++)
				if (list[i] == NULL)
				{
					list[i] = pvPortMalloc(size);
					printf("Success!\n");
					break;
				}
		}
		else
		{
			x = getrand(40);
			for (i = 0; i < 40; i++)
			{
				j = (i + x) % 40;
				if (list[j] != NULL)
				{
					printf("FREE MEM: SIZE %d--ADDRESS:%x\n", ((UsedHeader_t*)(list[j] - sizeof(UsedHeader_t)))->xBlockSize,list[j]);
					vPortFree(list[j]);
					list[j] = NULL;
					printf("Success!\n");
					break;
				}
			}
		}
		xPortGetMinimumEverFreeHeapSize();
		printf("Remaining Size: %d \n----------------------------------\n", xPortGetFreeHeapSize());
		vCheckAllBlocks();
//		getchar();
		j++;
	}
	system("pause");
}