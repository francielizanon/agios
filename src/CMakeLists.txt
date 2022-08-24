
set(warnings "-Wall")
set(CMAKE_C_FLAGS "-Wall")

add_definitions(-lconfig)

if(DEBUG)
   message("adding debug!")
   add_definitions(-DAGIOS_DEBUG=1)
endif(DEBUG)

target_sources(agios 
	PRIVATE
${CMAKE_CURRENT_LIST_DIR}/agios_add_request.c
${CMAKE_CURRENT_LIST_DIR}/agios_add_request.h
${CMAKE_CURRENT_LIST_DIR}/agios.c
${CMAKE_CURRENT_LIST_DIR}/agios_cancel_request.c
${CMAKE_CURRENT_LIST_DIR}/agios_config.c
${CMAKE_CURRENT_LIST_DIR}/agios_config.h
${CMAKE_CURRENT_LIST_DIR}/agios_counters.c
${CMAKE_CURRENT_LIST_DIR}/agios_counters.h
${CMAKE_CURRENT_LIST_DIR}/agios.h
${CMAKE_CURRENT_LIST_DIR}/agios_release_request.c
${CMAKE_CURRENT_LIST_DIR}/agios_request.c
${CMAKE_CURRENT_LIST_DIR}/agios_request.h
${CMAKE_CURRENT_LIST_DIR}/agios_thread.c
${CMAKE_CURRENT_LIST_DIR}/agios_thread.h
${CMAKE_CURRENT_LIST_DIR}/aIOLi.c
${CMAKE_CURRENT_LIST_DIR}/aIOLi.h
${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt
${CMAKE_CURRENT_LIST_DIR}/common_functions.c
${CMAKE_CURRENT_LIST_DIR}/common_functions.h
${CMAKE_CURRENT_LIST_DIR}/data_structures.c
${CMAKE_CURRENT_LIST_DIR}/data_structures.h
${CMAKE_CURRENT_LIST_DIR}/hash.c
${CMAKE_CURRENT_LIST_DIR}/hash.h
${CMAKE_CURRENT_LIST_DIR}/MLF.c
${CMAKE_CURRENT_LIST_DIR}/MLF.h
${CMAKE_CURRENT_LIST_DIR}/mylist.c
${CMAKE_CURRENT_LIST_DIR}/mylist.h
${CMAKE_CURRENT_LIST_DIR}/NOOP.c
${CMAKE_CURRENT_LIST_DIR}/NOOP.h
${CMAKE_CURRENT_LIST_DIR}/performance.c
${CMAKE_CURRENT_LIST_DIR}/performance.h
${CMAKE_CURRENT_LIST_DIR}/process_request.c
${CMAKE_CURRENT_LIST_DIR}/process_request.h
${CMAKE_CURRENT_LIST_DIR}/req_hashtable.c
${CMAKE_CURRENT_LIST_DIR}/req_hashtable.h
${CMAKE_CURRENT_LIST_DIR}/req_timeline.c
${CMAKE_CURRENT_LIST_DIR}/req_timeline.h
${CMAKE_CURRENT_LIST_DIR}/scheduling_algorithms.c
${CMAKE_CURRENT_LIST_DIR}/scheduling_algorithms.h
${CMAKE_CURRENT_LIST_DIR}/SJF.c
${CMAKE_CURRENT_LIST_DIR}/SJF.h
${CMAKE_CURRENT_LIST_DIR}/statistics.c
${CMAKE_CURRENT_LIST_DIR}/statistics.h
${CMAKE_CURRENT_LIST_DIR}/SW.c
${CMAKE_CURRENT_LIST_DIR}/SW.h
${CMAKE_CURRENT_LIST_DIR}/TO.c
${CMAKE_CURRENT_LIST_DIR}/TO.h
${CMAKE_CURRENT_LIST_DIR}/trace.c
${CMAKE_CURRENT_LIST_DIR}/trace.h
${CMAKE_CURRENT_LIST_DIR}/TWINS.c
${CMAKE_CURRENT_LIST_DIR}/TWINS.h
${CMAKE_CURRENT_LIST_DIR}/WFQ.c
${CMAKE_CURRENT_LIST_DIR}/WFQ.h
${CMAKE_CURRENT_LIST_DIR}/waiting_common.c
${CMAKE_CURRENT_LIST_DIR}/waiting_common.h
)



find_library(CONFIG_LIBRARY config HINTS /usr/local/lib) 
#target_link_libraries(agios PUBLIC ${CONFIG_LIBRARY})
target_link_libraries(agios PUBLIC -lconfig)

target_compile_options(agios PUBLIC -Wall -Werror) 
target_include_directories(agios PUBLIC ${CMAKE_CURRENT_LIST_DIR})
