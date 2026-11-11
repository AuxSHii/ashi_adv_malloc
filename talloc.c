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







#endif //else os=unix
