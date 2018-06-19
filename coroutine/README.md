# CoRoutine
>使用协程来管理,执行任务,占空间更少, 协程切换开销更小.
## 背景
对于多数嵌入式系统来说, 内存都非常的少, 是很宝贵的资源. 平常的任务调度使用的中断和堆栈, 这在嵌入式设备中是不可能的. 因为每个人物函数都有自己的局部变量, 每一次切换人物都要保存状态,不仅内存不够, 而且在恢复时要恢复很多变量, 延迟而可能达不到高实时性. 


## 协程原理
协程可以使用很少的资源达到状态切换的功能. 

下面是一个简单的例子
```c
#define INIT 0
#define __LINE__ 1
typedef STATE int;

int demo()
{
    static int ct =0;
    static STATE state = INIT;
    switch(staate){
        case 0: 
            while(ct<10){
                ct++;s
                state = __LINE__;
                return ct;
                case __LINE__:;
            }
    }
}
```
这里利用了 switch 的跳转功能, 通过设置状态可以达到切换的效果.

这里只需要两个静态变量的内存开销,就能实现上下文的切换, 可以节省很大的内存.
静态变量是必需的, 用来记住上一次的状态, 以及传递的数据(这里简单的传递 `ct`)

## 封装
直接在代码上体现协程, 可能难于理解, 直观上不易看出来. 而且不能实现代码的重用, 所以通过宏, 来代码替换,进行封装

上面的代码可以改成这样, 一样的效果

```c
#define INIT 0
#define __LINE__ 1
typedef STATE int;

#define BEGIN()  static int ct=0; switch(state){ case 0;
#define YIELD(i)   {state=__LINE__ ;  return i; case __LINE__:;}
#define END() }

int demo()
{
    static int i=0;
    BEGIN();
    while(i<10) YIELD(i);
    END();
}
```
这里只有一个 _____LINE__状态 做了事, 可以一直增加其他状态.


## 详细内容见 `coroutine.c`
