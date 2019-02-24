# Event-Counts

Standalone event-count module from facebook's [folly](https://github.com/facebook/folly).

An eventcount is a condition variable for lockfree algorithms. That is, it permits a thread to efficiently wait for an arbitrary condition to occur, but unlike condition variables, an eventcount does not require a mutex to protect the state (it's kind of stupid to surround a lockfree data structure with a mutex to permit conditional waiting).

Eventcounts allow to separate a lockfree data structure and blocking/signaling logic, so that there is generally no need to reimplement and inject it into each and every lockfree algorithm. For example, some people tend to implement so called blocking producer-consumer queues (instead of returning 'false' it blocks until new elements available), that's not only complicates the implementation, it also does not permit to poll, for example, several producer-consumer queues.

More information [here](http://www.1024cores.net/home/lock-free-algorithms/eventcounts).

Usage - 

```
Waiter:
     if (!condition()) {  // handle fast path first
         for (;;) {
             auto key = eventCount.prepareWait();
             if (condition()) {
                eventCount.cancelWait();
                break;
             } else {
                eventCount.wait(key);
             }
         }
     }
 ```
 
 ```
 Poster:
    make_condition_true();
    eventCount.notifyAll();    
```
Compile with `-DPSHARED` flag for use across multiple processes.

Build using `cmake` and run the demo program from `main.cpp`
