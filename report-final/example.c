#include "FreeRTOS.h"
#include "task.h"
#include "croutine.h"
/*实例代码中，Task1,Task2为主任务，vFlashCoRoutine为设想中的次要的周期任务*/
void vApplicationIdleHook(void)
{
	vCoRoutineSchedule();
}
static void vTask1(void *pvParameters)
{
	portTickType xLastWake;
	xLastWake = xTaskGetTickCount();
	for (;; )
	{
		printf("Hello T1!");
		vTaskDelayUntil(&amp; xLastWake, (3000 / portTICK_RATE_MS));
	}
}
static void vTask2(void *pvParameters)
{
	portTickType xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();
	for (;; )
	{
		printf("Hello T2!");
		vTaskDelayUntil(&amp; xLastWake, (1000 / portTICK_RATE_MS));
	}
}
void vFlashCoRoutine(CoRoutineHandle_t xHandle, UBaseType_t uxIndex)
{
	crSTART(xHandle);
	for (;; )
	{
		if (uxIndex == 0)
		{
			printf("flash0\r\n");
			crDELAY(xHandle, 500);
		}
		else if (uxIndex == 1)
		{
			printf("flash1\r\n");
			crDELAY(xHandle, 1000);
		}
	}
	crEND();
}
int main(void)
{
	xTaskCreate(vTask1, (const char *) "vTask1", 1000, NULL, 1, NULL);
	xTaskCreate(vTask2, (const char *) "vTask2", 1000, NULL, 1, NULL);
	xCoRoutineCreate(vFlashCoRoutine, 0, 0);
	xCoRoutineCreate(vFlashCoRoutine, 0, 1);
	vTaskStartScheduler();
	for (;;);
}