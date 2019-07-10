# AGIOS

an I/O request scheduling library at file level

## Introduction

AGIOS was developped to be easily integrated into any I/O service that treats requests at a file level, such as a local file system or intermediate nodes in an I/O forwarding scheme. The user (the I/O service) gives requests to AGIOS, which applies a scheduling algorithm, and gives requests back to the user when they are scheduled. Multiple options are provided in scheduling algorithms, and new algorithms can be easily added to it. The actual processing of requests is left to the user, so the library can be generic.

Tracing can be enabled so the library will generate trace files containing all request arrivals. If you want to use AGIOS for its tracing capabilities only, you are advised to choose the NOOP scheduling algorithm, as it will only induce minimal overhead.

## Notes

This is the second version of AGIOS. The first version, developped between 2012 and 2015, included both user-level library and kernel module implementations. Over time the library was almost completely rewritten, and the kernel module version was dropped. However, G. Koloventzos, from the Barcelona Supercomputing Center, adapted the kernel module version for his use, and it is still available in his repository: https://github.com/gkoloventzos/agios

## Requirements

AGIOS uses [libconfig](https://www.hyperrealm.com/libconfig/libconfig_manual.html) to read the configuration file. 

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

### Processing and releasing requests

After agios_add_request has added the requests to the internal data structure, the scheduling thread will apply a scheduling algorithm and eventually decide to process requests, and call the user-provided callbacks to do so. 

Please notice the callbacks are used by the scheduling thread itself, which cannot proceed to schedule new requests until it has returned, and hence its implementation will affect the behavior of the system. If the callback directly synchronously processes requests (accessing files from the storage system), than the system will process only one request at a time, because no more requests will be schedule until the end of the callback. Alternatively, the callback might create threads to process requests (as done in agios_test.c) and return immediately, for instance, or put the request into a sort of dispatch queue that will be consumed by other concurrent threads.

**After** the request was definetely processed, with data read/written from/to storage, the user **must** call agios_release_request **to each request** providing the same information given to agios_add_request: file identifier, type, length and offset. This function is required to clean the request from the internal data structures, freeing all that was dynamically allocated. 

The reason for calling it after the processing of requests is that this function also keeps track of the performance being attained by requests, which may be used internally by dynamic scheduling policies or parameter tuning. If you are using a simple scheduling algorithm with no dynamic behavior, you can call agios_release_request anytime you wish after the request was given to the callback, but you must still call it to free memory.

### End of utilization

Call agios_exit to stop the scheduling tread and free all allocated memory for the library.

## Adding a new scheduling algorithm

To add a new scheduling algorithm to AGIOS, follow these steps:

### Choose a data structure

Existing data structures are the hashtable and the timeline, and only one of them is used at any given moment to hold requests. In the hashtable, there are a fixed number of lines, and files are placed in the hashtable according to their string identifiers (provided to agios_add_request). Each line is thus a list of file_t structures for different files, and inside each file there is a read_queue and a write_queue where requests are placed in offset order. Contiguous requests in the same queue will be aggregated into a virtual request, that is a request_t structure containing a list of other request_t structs inside. Access to the hashtable is protected by one mutex per line of the hashtable.

In the timeline, there is a single queue and requests are added at the end of it. The whole queue is protected by a single mutex. Even when the timeline is being used to hold the requests, the hashtable will still exist and must be updated to hold statistics about file accesses. In this case the per-line mutexes are not used, and the timeline mutex protects the whole hashtable.

First of all you need to decide to which of these data structures requests are to be added to be consumed by your scheduling algorithm. Adding a different data structure is possible but will require deep modifications to the library. Alternatively, you can force a different behavior for the timeline (see the timeline_add_request function in req_timeline.c). When using TO-agg requests are added at the end of the queue only after checking for possible aggregations, with SW they are inserted following a different ordering, and with TWINS a set of multiple queues is used instead.

### Implement your algorithm

It is recommended to do so in a separate .c file. Your algorithm must implement the schedule function, which will be called to schedule requests. When called, the function will access the relevant data structures, making sure to hold the adequate lock, and select requests to be processed. Once selected, the request must be removed from the data structure, then given to the process_requests_step1 function, which will fill a struct with information about it. Then the scheduling algorithm must call the generic_post_process function, free the lock, and **only then** call process_requests_step2 giving as argument the struct returned by step1. 

The second process_requests function returns a boolean, which may be true to notify that some periodic event is due. The scheduling algorithm must always check this return, and, if notified, return from the schedule function immediately. 

The schedule function returns a 64-bit integer which is a waiting time in ns. It is to be zero unless the scheduling algorithm wants the scheduling thread to sleep for some time. You should **never** explicitly sleep in the schedule function, as it affects periodic events.

Additionally, you may implement initialization and ending functions for the scheduling algorithm. Add your source files to src/CMakeLists.txt

See SJF.c for an example of scheduling algorithm that uses the hashtable and TO.c for an example using the timeline. Additionally, see TWINS.c for an example of algorithm that asks for sleeping time.

### Make it known to AGIOS

In scheduling_algorithms.h, add your algorithm at the end of the list of #define and update IO_SCHEDULER_COUNT.

In scheduling algorithms.c, add a 

### About dynamic scheduling policies


If you opt for a different behavior while using the timeline, you may want to check the migration between data structures, implemented in data_structures.c. This concerns the use of a dynamic scheduler that periodically changes the scheduling algorithm being used, and thus sometimes must migrate requests between data structures. 

## TO DO

The library keeps statistics on past accesses, global and separated by file and type (read or write), and also performance measurements. These information are available internally to be used by scheduling algorithms, but users might be interested in this information. Hence in the future it would be useful to design an interface to do so adequately.

## Credit

If AGIOS is useful to you, consider citing one of its publications in your research work:

- “Automatic I/O scheduling algorithm selection for parallel file systems”. In Concurrency and Computation: Practice and Experience, Wiley, 2015. http://onlinelibrary.wiley.com/doi/10.1002/cpe.3606/abstract

• “AGIOS: Application-Guided I/O Scheduling for Parallel File Systems”. In Parallel and Distributed Systems (ICPADS), 2013 International Conference on. IEEE. http://ieeexplore.ieee.org/xpl/articleDetails.jsp?arnumber=6808156

Experiments that allowed the progress of AGIOS were conducted on the Grid'5000 experimental test bed, developed under the INRIA ALADDIN development action with support from CNRS, RENATER and several Universities as well as other funding bodies (see https://www.grid5000.fr).


