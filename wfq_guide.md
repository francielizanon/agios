# To implement the WFQ scheduler



- [ ] 1 - decide on how we will set the weights, implement it we could create a new call

- [x] 2. modify timeline_add_req in req_timeline.h to replicate the behavior of the TWINS scheduler

- [ ] 3. either make a version of timeline_oldest_req, or create an argument, to make it work for the case where there is a list of timelines (TWINS does it by hand but that is not ideal)

- [x] 4. create a constant for it in scheduling_algorithms.h, add a position to the list inscheduling_algorithms.c (the constant is the position of the list)

- [x] 5. create the .c and .h

- [ ] 6. implement the algorithm (see TWINS for guidance, it’s the most similar scheduler)

- [ ] 7. include the .h wherever needed, adapt the agios.conf file

- [ ] 8. test it either use directly or adapt the test/agios_test.c code from the repository




# Installing AGIOS in Ubuntu


1) install libconfig and cmake

```bash
sudo apt install -y libconfig-dev
sudo apt install -y cmake
```


2) execute cmake

```bash
  cd agios/build/
    cmake ..
    make
    make install

```