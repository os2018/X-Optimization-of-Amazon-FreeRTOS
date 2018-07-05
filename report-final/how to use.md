# 文件使用方法

## heap_6.c
将heap_6.c文件放到FreeRTOS内核下的 FreeRTOSv10.0.1\FreeRTOS\Source\portable\MemMang 目录下，并删除或在其他地方备份heap_5.c文件，并在使用FreeRTOS时对main.c和list.h做以下增添<br>
```c
* In main.c:
static void  prvInitialiseHeap( void ){
	static uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
	volatile uint32_t ulAdditionalOffset = 0;
	const HeapRegion_t xHeapRegions[] =
	{
		{ ucHeap, configTOTAL_HEAP_SIZE },
		{ NULL, 0 }
	};
	configASSERT( configTOTAL_HEAP_SIZE );
	( void ) ulAdditionalOffset;
	vPortDefineHeapRegions( xHeapRegions );
}

* In FreeRTOSConfig.h:
	#define configTOTAL_HEAP_SIZE					( ( size_t ) ( 64 * 1024 ) )
```

## coroutine.c
用该文件替换 FreeRTOSv10.0.1\FreeRTOS\Source 目录下的croutine.c文件即可。<br>
<br>
<br>
除了以上改变以外，其他FreeRTOS的使用方法与普通FreeRTOS并无区别，参见FreeRTOS官网<br>
http://aws.amazon.com/freertos<br>
http://aws.amazon.com/freertos<br>
