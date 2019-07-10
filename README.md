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

To initialize the library, the application has to call agios_init, passing as arguments two callback functions that will be used by the library when it is time to process requests. The first callback function will receive as argument a single request to be processed, and the second a list of requests at once. The second callback is optional, and NULL might be passed instead. 

In addition to the two callbacks, a path to a configuration file may be provided (if not, AGIOS will try to read from the default /etc/agios.conf). See agios.conf in the repository for an example of configuration file.

Finally, 





