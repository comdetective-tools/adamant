# Advanced DAta Movement ANalysis Toolkit: ADAMANT (v2.0)

ADAMANT is a collection of tools to capture and analyze memory activities in
applications. The goal is to use data address tracing and capture data movement
events that can be associated to data objects; the result is an application
profile that is based on data objects.

### Dependencies ###
ADAMANT uses [zlib](http://www.zlib.net/) to compress and decompress the profiles it generates.
ADAMANT uses [libunwind](http://www.nongnu.org/libunwind/) to take snapshots of the stack
when a new object is allocated. Both libraries are required to build and use ADAMANT.

### Compiling ADAMANT ###
To compile the library it is sufficient to enter the
*src* directory, modify the *makefile*, and type *make*. ADAMANT relies on
software modules to capture events of interest (e.g. cache misses). Currently,
there are two modes of operation: instrumentation library, and data collection
library. With the instrumentation library libpadm, an internal module collects
hardware events from the *perf* framework of Linux.  With the data collection
library libadm, and external module is linked to libadm and it is responsible
for generating and passing the events to the library. Using this mode, ADAMANT
has been used with [PEBIL](http://www.sdsc.edu/pmac/tools/pebil.html), a binary
instrumentation tool used to profile applications and simulate caches.

### Running ADAMANT ###
To run an application with libpadm, it is sufficient to preload the library
*LD\_PRELOAD=libpadm.so app*.  This is sufficient to obtain a file (*pid.adm*)
with a list of objects, identified by their address and size, and the
metadata associated with the object.  When running with external modules, the
execution may vary depending on the tool used. For example, with PEBIL, and new
instrumented is generated, which is linked to ADAMANT, and it can be executed as
the native application. To instrument applications with PEBIL, please refer to
the PEBIL documentation.

### Output format ###
The output file is in compressed binary format, with the exception of the first
two lines.  The first line contains the command line used to execute. The
second line identifies the module used, or the counters collected in the case
of hardware counters. This second line is defined in the module used to
generate the events.  The remainder of the file is a list of object records
with the following format (items on the same line are separated by white space,
though a dash is used here for clarity):

start address of the object - size of object in bytes - state (0 = statically allocated, 1 = dynamically allocated, 2 = free)
counters (meaning depends on the module utilized)
name of the object
name of the source file in which the object is defined (compilation unit)
name of the binary (application or library) in which the object is defined
snapshot of the stack

To decompress the profile, the tool *adm_cat* is provided, under the *tools*
directory.

The following schema is utilized for the counters when using PEBIL and PEBS:
l1ld l2ld l3ld memld l1st l2st l3st memst.

### Hardware Events ###
The following is a list of relevant events that support *data address tracing*
on Intel processors, and is taken from the Intel's Intel® 64 and IA-32
Architectures Software Developer Manuals (as of 3/1/2017).  Users are required
to change the event of choice in a macro definition (see Makefile under the
*src* directory).  Support for multiple counters selected at runtime will be
completed soon.  The events are divided by processor type and generation. These
lists may not be complete and more information is available on Intel's Intel®
64 and IA-32 Architectures Software Developer Manuals, Volume 3, chapters 18
and 19.

#### Haswell and Broadwell

Event                                           |
------------------------------------------------|
MEM\_UOPS\_RETIRED.ALL\_STORES                  |
MEM\_UOPS\_RETIRED.ALL\_LOADS                   |
MEM\_LOAD\_UOPS\_LLC\_MISS\_RETIRED.LOCAL\_DRAM |
MEM\_LOAD\_UOPS\_RETIRED.L1\_HIT                |
MEM\_LOAD\_UOPS\_RETIRED.L2\_HIT                |
MEM\_LOAD\_UOPS\_RETIRED.L3\_HIT                |
MEM\_LOAD\_UOPS\_RETIRED.L1\_MISS               |
MEM\_LOAD\_UOPS\_RETIRED.L2\_MISS               |
MEM\_LOAD\_UOPS\_RETIRED.L3\_MISS               |
UOPS\_RETIRED.ALL (if load or store is tagged)  |

#### Knights Landing
Event              |
-------------------|
MEM\_UOPS\_RETIRED |
L2\_HIT\_LOADS     |
L2\_MISS\_LOADS    |

###### Note ###
Feedback is very welcome. ADAMANT is actively developed and changes are very frequent; all comments and
contributions will be taken into account.
