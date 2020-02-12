cmake_minimum_required(VERSION 3.5)
project(libagios)

#the DEBUG option, which makes the library very verbose
option(DEBUG "Should AGIOS output debug messages (very verbose)?" OFF)

#the library
add_library(agios SHARED "")
include(src/CMakeLists.txt)

#how to do make install
install(TARGETS agios DESTINATION lib)
install(FILES src/agios.h DESTINATION include)

#make the example code
add_executable(agios_test test/agios_test.c)
target_compile_options(agios_test PUBLIC -Wall -Werror)
target_link_libraries(agios_test PUBLIC agios)
target_link_libraries(agios_test PUBLIC -lpthread)
add_executable(multicallback_test test/multicallback_test.c)
target_compile_options(multicallback_test PUBLIC -Wall -Werror)
target_link_libraries(multicallback_test PUBLIC agios)
target_link_libraries(multicallback_test PUBLIC -lpthread)

#documentation
#include_directory(docs)
find_package(Doxygen)
add_custom_target(doc doxygen ${CMAKE_CURRENT_SOURCE_DIR}/agios.doxygen)
