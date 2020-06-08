# dynamic_memory_tools
Few tools and stats for dynamic memory allocations in C and C++.
The tool splits the stats to spatial and temporal. It supports basic (few threads) multithreading.
In version 0.1 it has one mutex to protect access to the global "stats" handle.
The idea came from some previous need to gather stats for dynamic memory allocation in our C/C++ dev env.
Wanted to make it work as both regular object library and shared one.
Plan to extend it to C++ support of new and delete, unique and share pointers.
Need to work on multithreading by adding more mutexes or other sync vars to achieve high performance in case say of hundreds or thousands reads & writes each with its own threads.
Also, need to benchmark it to see what its current (version 0.1) performance.
