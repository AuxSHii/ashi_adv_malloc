# Writing a Real Memory Allocator in C (Windows & Unix)

ohhh you are so welcome to my cross platform advance malloc implementation guide  
i already wrote and pushed a basic malloc implementation repo , i will encourage you to follow that before you attempt this one... if not then you can also directly attempt this one as i wrote it as thorough and beginner-friednly as possible.   


okk so to truly add value beyond a basic implementation, a custom memory allocator needs to handle real-world challenges. While basic allocators just link chunks of memory, a production-level `malloc` must conserve space, prevent fragmentation, and survive multiple threads asking for memory simultaneously.  


we are going to be writing an advanced allocator with four major upgrades:
1. **memory alignment:** CPUs read memory much faster when it is aligned to specific byte boundaries (like 8 bytes).
2. **thread safety:** using native Operating System Mutexes (locks) to prevent data corruption when multiple threads use `malloc`.
3. **Block Splitting:** if we find a 1024-byte free block but only need 32 bytes, we split it in two to save space
4. **Block Coalescing (Merging):** when `free` is called, we merge adjacent free blocks together to prevent the heap from fracturing into tiny, unusable pieces.[preventing fragmentation]

so lets talk about things that a basic implementation lacks:

If you use a basic allocator in a real program, two things will break it:  
1. Multiple parts of your program will ask for memory at the exact same millisecond, corrupting your list.  
2. Your heap memory will get chopped up into tiny, useless pieces until you run out of RAM.

this guide walks through building an allocator that fixes both problems. We are going to write it for both **unix (linux/macOS)** and **windows**, explaining every concept from scratch before we code.


---

## Section 1: asking the OS for memory & thread safety

### Theory

#### 1. Stack vs. Heap and the Operating System  
when your program starts, the Operating System (OS) gives it two places to store data in RAM:  
* **[The Stack](https://en.wikipedia.org/wiki/Stack-based_memory_allocation):** A small region of memory where the size of every variable is known before the program even runs (like `int a = 10;`). The CPU manages this automatically.[happens at compile time]    
* **[The Heap](https://en.wikipedia.org/wiki/C_dynamic_memory_allocation):** A huge pool of memory used when you do not know how much space you need ahead of time (like loading a file or a webpage). [happens at runtime]   

the OS protects your computer by stopping your program from writing outside its assigned heap. However, inside your own heap, the OS does not care what you do. if you write new data on top of old data by mistake, your program corrupts itself. `malloc()` solves this by tracking which parts of the heap are currently in use and handing you a safe, empty memory address.

when our allocator runs out of empty space to hand out, it has to ask the OS to make the heap bigger. because this requires a **[Context Switch](https://en.wikipedia.org/wiki/Context_switch)** (the CPU pausing your program to let the OS kernel do work, which is slow), we only ask the OS when we have no free blocks left to recycle.   
* **On Unix:** your heap has an upper border called the **[Program Break](https://man7.org/linux/man-pages/man2/sbrk.2.html)**. we use the `sbrk(size)` system call to push that border upward by `size` bytes. If the computer is out of memory, `sbrk` returns `(void*) -1`.  
* **On Windows:** windows manages heap memory through **[Handles](https://learn.microsoft.com/en-us/windows/win32/sysinfo/handles-and-objects)** (an ID number the OS uses to track a resource). first, we call **[GetProcessHeap()](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-getprocessheap)** to get the handle for our program's default heap. Then, we pass that handle into **[HeapAlloc()](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapalloc)** to get raw bytes for our program's heap  ..  

#### 2. what is a Thread?  
normally, a program runs one line of code at a time, top to bottom. a **[Thread](https://en.wikipedia.org/wiki/Thread_(computing))** is a separate path of execution inside the same program. modern CPUs have multiple cores, allowing a program to run multiple threads at the exact same physical time. For example, a video game might use thread A to draw graphics and thread B to calculate physics.
thread runs a perticular process and execute those perticular set of instructions.  

#### 3. race conditions and Mutex locks  
all threads in a program share the same Heap. imagine we have one free block of memory left. thread A and thread B both call `malloc()` at the exact same microsecond.   
1. Thread A checks the block and sees `free = true`.  
2. Before Thread A can mark it as `in use`, thread B *also* checks the same block and sees `free = true`.  
3. Both threads take the exact same memory address and start writing different data to it.

this bug is called a **[Race Condition](https://en.wikipedia.org/wiki/Race_condition)**. 

to prevent this, we use a **[Mutex](https://en.wikipedia.org/wiki/Lock_(computer_science))** (short for *Mutual Exclusion*). think of a mutex like a single key to a single-person bathroom. Before a thread is allowed to touch our memory list, it must grab the key (**lock** the mutex). If Thread B comes along while Thread A has the key, the OS puts Thread B to sleep in a waiting line. Once Thread A finishes and puts the key back (**unlocks** the mutex), the OS wakes Thread B up and hands it the key.  

*(Note: Some beginner tutorials use a `bool is_locked` variable with a `while(is_locked) sleep(1);` loop. Do not do that in real code , ...two threads can still pass the `while` check at the exact same nanosecond, and sleeping for a whole second destroys program speed. Always use the real OS mutexes we use below: **[pthread_mutex_t](https://man7.org/linux/man-pages/man3/pthread_mutex_lock.3p.html)** on Unix and **[CRITICAL_SECTION](https://learn.microsoft.com/en-us/windows/win32/sync/critical-section-objects)** on Windows).*

## lets talk more about mutexes of our OS [read after code for a deep-dive]
### what is a mutex and what does it do?

a Mutex (Mutual Exclusion object) is a kernel-backed synchronization primitive that enforces atomicity (all-or-nothing execution) across a block of code called a Critical Section.
When a thread locks a mutex, the CPU uses special hardware-level Atomic Operations (like LOCK CMPXCHG on x86 processors) to check and flip the lock's state in a single, uninterruptible clock cycle.  
if another thread already owns the lock, the Operating System's thread Scheduler removes the waiting thread from the CPU's "Ready Queue" and puts it into a "Sleeping" state so it wastes 0% CPU cycles while waiting...

### its Headers, Data Types, and Functions

because windows and unix-like systems (linux/macOS) manage threads differently at the kernel level, we use #ifdef _WIN32 to map our lock_heap() and unlock_heap() wrapper functions to each OS's native API.[we did that in the basic implementation also]  

#### on unix (linux / macOS)

• header file: #include <pthread.h> (POSIX threads standard).  
• data type: pthread_mutex_t — an opaque C struct (typically 40 bytes on 64-bit Linux) that stores the lock state, owner thread ID, and wait-queue pointers.  
• initialization: PTHREAD_MUTEX_INITIALIZER — a compile-time macro that pre-fills the struct with default unlocked values so no setup function is needed at runtime.  
• functions used in our code:  
	• int pthread_mutex_lock(pthread_mutex_t *mutex) — takes a pointer to our lock. blocks the calling thread until the lock is acquired. Returns 0 on success, or an error code (like EDEADLK) on failure.
	• int pthread_mutex_unlock(pthread_mutex_t *mutex) — takes a pointer to our lock. releases ownership and wakes up the next sleeping thread in line. Returns 0 on success.

On Windows

• header File: #include <windows.h>.
• data type: CRITICAL_SECTION — a Windows struct (RTL_CRITICAL_SECTION) designed specifically for fast locking inside a single process. (Note: windows also has a HANDLE mutex via CreateMutex(), which works across multiple processes, but CRITICAL_SECTION is standard for allocators because it avoids slow kernel transitions unless threads actually collide).
• functions used code:
	• void InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection) — Takes a pointer to our CRITICAL_SECTION variable (LPCRITICAL_SECTION is just Windows typedef shorthand for CRITICAL_SECTION*). Prepares internal tracking fields before first use; returns nothing (void).
	why we need this initialization function?
    ```
    when you declare static CRITICAL_SECTION heap_lock; in c, windows just hands you an empty chunk of memory filled with zeroes... it isn't actually a working lock yet.
calling InitializeCriticalSection(&heap_lock) goes inside that CRITICAL_SECTION struct and fills out the hidden variables windows needs to manage the lock under the hood. specifically it sets up things like:
• lock count: a counter tracking whether the lock is currently free (-1) or how many times a thread has locked it (0 or higher).
• owning thread: a field to store the thread id of whichever thread currently holds the key so other threads cant steal it.
• recursion count: tracks if the same thread locked it multiple times in a row so it knows how many times it needs to unlock.
• debug info & event handle: internal pointers windows uses to put waiting threads to sleep if a traffic jam happens.
and becauuse this function just modifies your heap_lock variable directly in memory using its pointer (&heap_lock), it doesn't need to return a new value or status code... which is why its return type is void (nothing).
```

• void EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection) - takes a pointer to the lock. Spins briefly in user-mode, then puts the thread to sleep if still locked; returns void.

to understand what goes on under the hood.. read this
```theory

1. what does "spins in user-mode" actually mean?

to understand spinning, we have to understand the two worlds  cpu runs in:  
• user mode: where your regular c program runs. fast, restricted access to hardware.
• kernel mode: where the windows os core lives. switching from user mode to kernel mode (a context switch) takes thousands of cpu cycles because the cpu has to pause your program, save all your variables/registers, ask the os scheduler to put your thread to sleep, and load a different thread.
now imagine thread A is inside my_malloc() and thread b calls my_malloc() at the exact same moment...
• my_malloc only takes a tiny fraction of a microsecond to finish.
• if windows immediately put thread b to sleep in kernel mode, it would waste thousands of cpu cycles just pausing and waking thread b back up... even though thread a was literally 2 nanoseconds away from unlocking heap_lock.

so instead of going straight to sleep, EnterCriticalSection spins... meaning thread b runs a super tight while loop right inside user mode, checking the lock variable over and over (like 4000 times) using fast cpu atomic instructions (LOCK CMPXCHG).
if thread A finishes and unlocks heap_lock during those few thousand loop ticks, thread B grabs the lock immediately and never had to ask the os kernel to put it to sleep. if the spin loop finishes and thread A still hasn't let go, only then does EnterCriticalSection give up, switch into kernel mode, and put thread B to sleep so it stops burning cpu power.
(note: on a single-core cpu, spinning is pointless because thread A cant run while thread B is hogging the only core... so windows automatically skips spinning on single-core machines).

2. why do we call it inside lock_heap() right after InitializeCriticalSection()?

look at the two separate jobs those functions do inside lock_heap():
1. InitializeCriticalSection(&heap_lock); builds the door and the lock. it only runs once in the entire lifetime of your program (thanks to if (!lock_initialized)). building a lock does not lock it... the door is still wide open.
2. EnterCriticalSection(&heap_lock); actually locks the door behind you. this has to run every single time a thread calls lock_heap().
so on the very first my_malloc() call in your program, lock_heap() sets up the lock once, and then immediately locks it so that first thread can safely touch global_base. on every my_malloc() or my_free() call after that, lock_initialized is already true, so it skips straight to EnterCriticalSection(&heap_lock) to wait in line and grab the key.
```



• void LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection) — Takes a pointer to the lock and releases ownership so waiting threads can proceed; returns void.



###  industry standard vs. our teaching implementation

• Global Lock vs. Thread Caching (Real-World Trade-off): using one global mutex (heap_lock) is the classic, rock-solid pattern for single-threaded or lightly threaded applications. however, in massive multi-core production systems (like web servers with 64 threads), a single global lock creates a traffic jam called Lock Contention. Production allocators like Google's tcmalloc and jemalloc solve this by giving each thread its own private local cache of small memory blocks so threads only lock the global heap when their local cache runs dry.
• Subtle Edge Case in Our Windows Setup: Notice how our Windows lock_heap() uses a bool lock_initialized check on the first call. While fine for learning, if two threads call my_malloc() for the very first time at the exact same nanosecond, both could enter InitializeCriticalSection() simultaneously! In production Windows code, engineers prevent this initialization race using InitOnceExecuteOnce (One-Time Initialization).






### High-Level Steps  
1. Check which OS the compiler is running on (`#ifdef _WIN32`).  
2. Set up the native OS lock (`CRITICAL_SECTION` on Windows, `pthread_mutex_t` on Unix).  
3. Write wrapper functions to lock the heap, unlock the heap, and request raw bytes from the OS.

### Detailed Low-Level Steps  
1. Include `<stddef.h>`, `<stdio.h>`, and `<stdbool.h>` so we have access to `size_t`, `NULL`, and `bool`.  
2. Check `#ifdef _WIN32` (a macro automatically defined by C compilers on Windows).  
3. **In the Windows block:**  
   * Include `<windows.h>`.  
   * Declare a global `CRITICAL_SECTION heap_lock` and a `bool lock_initialized = false`.  
   * Write `lock_heap()`: if `lock_initialized` is false, call `InitializeCriticalSection(&heap_lock)` and set it to true. Then call `EnterCriticalSection(&heap_lock)`.  
   * Write `unlock_heap()`: call `LeaveCriticalSection(&heap_lock)`.  
   * Write `request_os_memory(size_t size)`: call `GetProcessHeap()`, check that it is not `NULL`, and return `HeapAlloc(heap, 0, size)`.  
4. **In the Unix (`#else`) block:**  
   * Include `<unistd.h>` (for `sbrk`) and `<pthread.h>` (for mutexes).  
   * Declare and initialize `pthread_mutex_t heap_lock = PTHREAD_MUTEX_INITIALIZER;`.  
   * Write `lock_heap()` using `pthread_mutex_lock(&heap_lock)` and `unlock_heap()` using `pthread_mutex_unlock(&heap_lock)`.  
   * Write `request_os_memory(size_t size)`: call `sbrk(size)`. If the return value equals `(void*) -1`, return `NULL`. Otherwise, return the memory address.

### Pseudo Code  
```text  
IF compiling on Windows THEN  
    CREATE global Windows CriticalSection lock  
      
    FUNCTION lock_heap()  
        IF lock is not set up yet THEN  
            Initialize the lock  
        END IF  
        Acquire Windows lock (wait if another thread holds it)  
    END FUNCTION

    FUNCTION unlock_heap()  
        Release Windows lock  
    END FUNCTION

    FUNCTION request_os_memory(size)  
        handle = GetProcessHeap()  
        IF handle is NULL THEN RETURN NULL  
        RETURN HeapAlloc(handle, 0, size)  
    END FUNCTION

ELSE (Compiling on Unix/Linux)  
    CREATE global pthread mutex lock (initialized ready-to-use)

    FUNCTION lock_heap()  
        Acquire pthread lock (wait if another thread holds it)  
    END FUNCTION

    FUNCTION unlock_heap()  
        Release pthread lock  
    END FUNCTION

    FUNCTION request_os_memory(size)  
        ptr = sbrk(size)  
        IF ptr == -1 THEN RETURN NULL  
        RETURN ptr  
    END FUNCTION  
END IF  
```

### The Exact Code  
```c  
#include <stddef.h>  // Gives us size_t and NULL  
#include <stdio.h>   // Useful if you want to print debug info later  
#include <stdbool.h> // Gives us bool, true, and false

#ifdef _WIN32  
    #include <windows.h>

    // Windows lock variables  
    static CRITICAL_SECTION heap_lock;  
    static bool lock_initialized = false;

    void lock_heap(void) {  
        if (!lock_initialized) {  
            InitializeCriticalSection(&heap_lock);  
            lock_initialized = true;  
        }  
        EnterCriticalSection(&heap_lock);  
    }

    void unlock_heap(void) {  
        LeaveCriticalSection(&heap_lock);  
    }

    // Ask Windows for raw heap memory  
    void* request_os_memory(size_t size) {  
        HANDLE heap = GetProcessHeap();  
        if (heap == NULL) {  
            return NULL;  
        }  
        // Flag 0 means default behavior (no auto-zeroing, normal thread serialization)  
        return HeapAlloc(heap, 0, size);  
    }

#else  
    #include <unistd.h>  
    #include <pthread.h>

    // Unix lock variable (statically initialized so it works right away)  
    static pthread_mutex_t heap_lock = PTHREAD_MUTEX_INITIALIZER;

    void lock_heap(void) {  
        pthread_mutex_lock(&heap_lock);  
    }

    void unlock_heap(void) {  
        pthread_mutex_unlock(&heap_lock);  
    }

    // Ask Unix for raw heap memory by moving the program break  
    void* request_os_memory(size_t size) {  
        void* block = sbrk(size);  
        if (block == (void*) -1) {  
            return NULL; // OS is out of memory  
        }  
        return block;  
    }  
#endif  
```
