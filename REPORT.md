ECS-150-Project-003
===

## Introduction
This project's goal is to implement a semaphore API and a Thread Private
Storage (TPS) API. Sempahore is a data type that can be used to solve the
critical section problems, and achieve synchronization between threads in
multi-threaded programming. TPS provides a safe way to have memory stored
specific to each thread. Both of the APIs were implemented with the help of
`pthread.h` and the `queue` we implemented on the previous project.

## Features

### Semaphore
For the semaphore API, we created a struct that has an `unsigned int` for
`size`
and a `queue_t` to hold the blocked threads. The size is an `unsigned int`
because the semaphore keeps an internal count to the amount of a resource, so
it will always be positive. This allows us to not worry about a negative count.
We also allow it to hold `queue_t` of `blockedThreads` to hold threads that
are blocked and ineligible for scheduling.

`sem_create` initializes a a `sem` of type `sem_t` and assigns the `size` to
the `count` passed in and then initializes the `blockedThreads`.

`sem_destroy` checks if the `sem` is `NULL` or if there are still queues being
blocked first, and if so, fails by return `-1`. Otherwise, it calls `free` on
the `sem`.

`sem_down` enters a critical section after validating that `sem` exists. We do
this because we want to ensure mutual exclusions from the other threads.
Taking an unavailable semaphore will cause the caller thread to be blocked
until the semaphore becomes available. We then exit the critical section before
going to sleep and re-enter the critical section upon wake-up. We then
decrement the `size` and then exit the critical section because we want to
allow another thread who was waiting to enter the same critical  section to
enter.

`sem_up` enters a critical section after validating that `sem` exists. We
do this because we want to ensure 'mutual exclusions' from the other threads.
If there are any queues being blocked, we call `queue_dequeue` to put the most
recent queue into `tid`. We then unblock thread `tid` and make it ready for
scheduling. We then exit the critical section before going to sleep and re-
enter the critical section upon wake-up. We then increment the `size` and then
exit the critical section  because we want to allow another thread who was
waiting to enter the same critical  section to enter.

`sem_getvalue` first checks if there is a `sem` or `sval`, and returns `-1` if
not. If `size` of `sem` is greater than 0, then `sval` is the `sem -> size`.
If the `sem - > size` is zero, then `sval` is the negative value of the amount
of threads.

### Thread Private Storage (TPS)
For TPS, we created two structs: `TPS` and `Page`. `TPS` contains a `Page`, and
a `pthread_t` that represents the id of the associated thread. `Page` contains
a `void *` to represent the address of its memory, and an `int` that represents
how many threads are pointing to the page, aka a **reference count**. We also
used a `queue_t` to store the tid of threads that has a TPS.

`tps_init` receives an `int` and checks if it's a positive number. If it is, a
custom signal handler will be added to signal any invalid access of memory in
the TPS by printing 'TPS protection error' to the stderr. It also initializes
the `queue_t`, and changes `initialized` to 1 to signal the TPS api is
initialized.

`tps_create` first grabs the calling thread's id with `pthread_self`, and we
utilizes the `queue_iterate` function to iterate through our queue to see if
the TPS has already been initialized to that thread, if yes we will exit right
away. If not, we will create a memory page with no read / write permission with
private and anonymous mapping.

`tps_destroy` is a cleanup function that will check if the current thread has a
TPS, if yes, then it will call `munmap` and free all the dynamically allocated
memories.

`tps_read` takes an `offset`, `length` and a `buffer`, `offset` represents the
bits that should be offset when reading the memory, and `length` is how many
bits do you want to read, and the `buffer` is the target that you want to copy
memory over to. We also utilized `mprotect` to temporarily allow the memory
page to be read, in order to access the memory page and copy over the memory to
the target buffer. Finally we will remove the access to the memory age before
we exit the function.

`tps_write` take an `offset`, `length`, and a `buffer` similar to `tps_read`,
but instead we are writing *from* the `buffer` to the TPS's memory. Also, we
will check the `Page` that we are writing to has a reference count of > 1, if
yes then we will create a new page of memory.

`tps_clone` takes a `tid` and creates a new TPS for the current calling thread.
It will also make the new TPS point to same `Page` as the target thread. It
makes it more memory efficient as if the `Page` is not modified by the copied
TPS, there will not be extra memory allocated for the new page.

## Testing

### Sempahore
For phase 1 testing, after reading and understanding the test files, we ran the
the given test cases from the professor:
`sem_count.c`, `sem_buffer.c`, and `sem_prime.c`.
- `sem_count.c` test prints out numbers from 0 - 19, one number per
thread at a time. This tests the synchronization of two threads sharing two
semaphores.

- `sem_buffer.c` test is where a producer produces x values in a shared buffer,
while a consume consumes y of these values. x and y are always less than the
size of the buffer but can be different. The synchronization is managed through
two semaphores.

- `sem_prime.c` test prints every prime number up to 997. A producer thread
creates numbers and inserts them into a pipeline, a consumer thread (sink) gets
prime numbers from the end of the pipeline. The pipeline consists of filtering
thread, added dynamically each time a new prime number is found and which
filters out subsequent numbers that are multiples of that prime.

### Thread Private Storage (TPS)
Besides the given `tps_simple.c` testing that was given, we created 2 more
files for testing which are `tps_sig.c` and `tps_complex.c`.

- `tps_simple.c` already has a decent code coverage. It first tests the normal
  `tps_create` and `tps_write` calls with a static string, afterwards it calls
  `tps_clone` and read to a buffer to see if the cloning is successful. It also
  tests the copy on write, which is when we write on a copied TPS page, it will
  create a new page that will have different memory then the one that was
  cloned. We also added invalid parameters on various calls (e.g Calling with
  length greater than `TPS_SIZE`, calling functions before calling tps_init..).

- `tps_sig.c` was mainly used to check the signal handler that was created by
  `tp_init`, we wrapped the normal `mmap` function with our own version, so
  that we have the address of the last accessed page's address. Then we will
  attempt to write something on that address and see if our custom signal
  handler gets triggered.

- `tps_complex.c` is similar to `tps_simple.c` but with some slightly more
  complicated test cases like writing and reading with different offsets, using
  data types that are not just `char *` to test the tps functions.

### Sources
- [mprotect man page](https://linux.die.net/man/2/mprotect)
- [sigaction man page](https://pubs.opengroup.org/onlinepubs/7908799/xsh/sigaction.html)
- [Memory-mapped I/O page](https://www.gnu.org/software/libc/manual/html_mono/libc.html#Memory_002dmapped-I_002fO)
