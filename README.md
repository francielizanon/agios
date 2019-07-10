# AGIOS

an I/O request scheduling library at file level

## Installing it

First obtain the repository, then use the following commands to build and install the library:

cd agios/build/
cmake ..
make
make install

You can pass arguments to cmake as usually to define where to install it. Depending on these configurations, the last line might require sudo.
If you want AGIOS to output debug messages when in use (this is very verbose and should be used for development only), replace the cmake line by:

cmake -DDEBUG=ON ..

In addition to the library, the commands above will build a simple application, agios_test, that can be used to generate some requests to the library.

You can use the following line to build the code documentation with doxygen:

make doc

That will create a build/docs folder.

## Using it

To use AGIOS, the application must include agios.h and explicitly link to libagios with -lagios. 
See test/agios_test.c in the repository for an example of utilization of the library.

### Initialization

To initialize the library, the application has to call agios_init, passing as arguments two callback functions that will be used by the library when it is time to process requests. The first callback function will receive as argument a single request to be processed, and the second a list of requests at once. The second callback is optional, and NULL might be passed instead. The list of requests passed to the second callback are always contiguous requests of the same type for the same file.

In addition to the two callbacks, a path to a configuration file may be provided (if not, AGIOS will try to read from the default /etc/agios.conf). See agios.conf in the repository for an example of configuration file.

Finally, the last argument to agios_init is the number of existing queue ids that may be passed to agios_add_request. Two scheduling algorithms provided by AGIOS (SW and TWINS) use these queue ids to represent either the application that issued the request or the data server that holds the data being accessed. Hence this parameter is only relevant when using one of these algorithms (or a dynamic algorithm that may sometimes choose to use one of them). In other cases, 0 is to be provided to agios_init. If max_queue_id is passed to agios_init, then the queue ids provided to agios_add_request **must** be between 0 and [max_queue_id]-1, otherwise the library will crash (specially in the case of TWINS, SW is somewhat more robust).

**All functions in the interface between AGIOS and its user return true in case of success, and false otherwise** (except agios_exit, which returns nothing).

If agios_init is successful, then a scheduling thread will be executing in a loop.

### Adding requests

Requests are to be added to the library through the agios_add_request function. The arguments are: a string representing the file being accessed by the request, its type (either RT_READ or RT_WRITE), starting offset in bytes, length in bytes, identifier and queue_id. Identifier is a 64-bit integer that uniquely represents that request for the user, and is what is given as argument in the callbacks to process requests. See the previous section about initialization for a discussion on queue_id.

This function is thread-safe and can be called by concurrent threads without problems (although the parallelism may be limited by some internal locks).

### Processing requests

After agios_add_request has added the requests to the internal data structure, the scheduling thread will apply a scheduling algorithm and eventually decide to process requests, and call the user-provided callbacks to do so. 

Please notice the callbacks are used by the scheduling thread itself, which cannot proceed to schedule new requests until it has returned, and hence its implementation will affect the behavior of the system. If the callback directly synchronously processes requests (accessing files from the storage system), than the system will process only one request at a time, because no more requests will be schedule until the end of the callback. Alternatively, the callback might create threads to process requests (as done in agios_test.c) and return immediately, for instance, or put the request into a sort of dispatch queue that will be consumed by other concurrent threads.





