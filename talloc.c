#include <stdio.h>  //maybe printing debug info 
#include <stddef.h> //gives me size_t and null
#include <stdbool.h>  //for bool

#ifdef _WIN32
//if os = windows
#include <windows.h>
//setup mutex
static CRITICAL_SECTION heap_lock; 
static bool lock_initialized; //lock init = true ; => another threadhave that key  
//this bool is a state tracker var

//fxn to lock the heap
//this fxn runs when a thread needs to acquirre the lock/mutex or lock the heap mem alloc pass to itself
void lock_heap(void){
	if(!lock_initialized){
		InitializeCriticalSection(&heap_lock);//fields with default values prepared
		lock_initialized = true; //lock is initlzd
	}
	//if alrady init , and another thread needs to lock it - it should wait for it
	EnterCrititcalSection(&heap_lock);
}

//fxn to unlock the heap 
//this fxn runs when a thread needs to release its ownership[i.e mem allocation done for that thread]
void unlock_heap(void){
	LeaveCriticalSection(&heap_lock);

}


//request memory from os
//fxn that requests raw bytes of memory into heap from OS.
void* request_os_memory(size_t size){
	//handler for heap of the process
	HANDLE heap = GetProcessHeap();
	//check null case
	if(heap == NULL)
		return NULL;
    //0 flag : default behaviour[no auto zeroing]
	return HeapAlloc(heap, 0, size);
}

#else //else os=unix





#endif
