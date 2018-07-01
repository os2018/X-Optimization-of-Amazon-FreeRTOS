#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <test.h>
#include <time.h>
#include <intrin.h>

int getrand(int max)
{
	return rand() % max;
}

int main()
{
	static int8_t* list[60];
	int i=0, x, j, count = 0;
	int time1, time2;
	size_t size;
	long tottime = 0;
	long starttime;
	int8_t HEAP[65536];
	HeapRegion_t hp[2] = {{ HEAP, 65536 }, { 0,0 }
	};
	vPortDefineHeapRegions(&hp);
//	vCheckAllBlocks(&(HEAP[65535]));
	srand(time(NULL));
	j = 0;
	int loop = 0;
	FILE *fp;
	fp=fopen("result.txt", "w");
	for (loop = 0; loop < 20; loop++)
	{
		size = getrand(4095) + 1;
		//			printf("GET MEM: SIZE %d\n", size);
		list[i] = pvPortMalloc(size);
		//					printf("Success!\n");
		i++;
	}
	for (loop = 0; loop < 5000; loop++)
	{
		count = 0;
		starttime = clock();
		while (count < 1)
		{
			x = getrand(2);
			if (x == 1)
			{
				size = getrand(4095) + 1;
				//			printf("GET MEM: SIZE %d\n", size);
				time1 = __rdtscp(&i);
						list[i] = pvPortMalloc(size);
				time2 = __rdtscp(&i);
						//					printf("Success!\n");
						i++;
						fprintf(fp, "%d\n", time2 - time1);
			}
			else if(i!=0)
			{
				x = getrand(i);

						//					printf("FREE MEM: SIZE %d--ADDRESS:%x\n", ((UsedHeader_t*)(list[j] - sizeof(UsedHeader_t)))->xBlockSize,list[j]);
				time1 = __rdtscp(&i);
				vPortFree(list[x]);
				time2 = __rdtscp(&i);
						list[x] = NULL;
						for (j = x; j < i; j++)
							list[j] = list[j + 1];
						i--;
						//					printf("Success!\n");
					
						fprintf(fp, "%d\n", time2 - time1);
			}
			xPortGetMinimumEverFreeHeapSize();

			//		printf("Remaining Size: %d \n----------------------------------\n", xPortGetFreeHeapSize());
			//		vCheckAllBlocks();
			//		getchar();
			j++;
			count++;
		}
		
	}
	printf("done!");
	fclose(fp);
	system("pause");
}