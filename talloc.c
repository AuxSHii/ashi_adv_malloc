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

#include <unistd.h> //gives sbrk system call   
#include <pthread.h>    //posix mutex

//global decl & init mutex
static pthread_mutext_t heap_lock = PTHREAD_MUTEX_INITIALIZER;

//fxn to lock the heap
//this fxn runs whean a thrad needs to acquire the lock[i.e mem allocation on the way from a perticular thread]

void lock_heap(void){
	pthread_mutext_lock(&heap_lock);
}


//fxn to unlock the heap
//this fxn runs when thread needs to realease its ownership [i.e mem alloc happend for that perticular thread , now naother thread can use it nad acquire the lock]

void unlock_heap(void){
	pthread_mutext_unlock(&heap_lock);
}


//to request memory from OS  [raw mem bytes]
//unallocated and unorganized space in heap
//so ask unix for heap memory by moving the program break;
void* request_os_memory(size_t size){
	void* block = sbrk(size);
	if(block == (void*) - 1)
		return NULL; //os ran out of memory
	return block;
}

#endif
