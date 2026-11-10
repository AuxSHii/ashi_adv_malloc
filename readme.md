# ashi's advanced malloc

its an advanced malloc implementation i wrote in C , after my basic implementation "shalloc" , i will also be writing a guide for any beginner to follow along , learn the concepts and code this on their own...  

this memmory allocater will have four major upgrades:  
1. **Memory Alignment:** CPUs read memory much faster when it is aligned to specific byte boundaries (like 8 bytes).  
2. **Thread Safety:** Using native Operating System Mutexes (locks) to prevent data corruption when multiple threads use `malloc`.  
3. **Block Splitting:** If we find a 1024-byte free block but only need 32 bytes, we split it in two to save space [cite: 1].  
4. **Block Coalescing (Merging):** When `free` is called, we merge adjacent free blocks together to prevent the heap from fracturing into tiny, unusable pieces.   

can see the [guide](guide.md) for learning..