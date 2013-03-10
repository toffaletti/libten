.. _ioproc:

#######
IO Proc
#######

Overview
========

ioproc provides a non-blocking api to a thread pool for performing blocking operations without blocking tasks.

Reference
=========

.. type:: iochannel

    channel type used for communicating with ioproc thread-pool.

.. class:: ioproc

    Reference to an ioproc thread-pool

    .. member:: iochannel ch

        Internal channel used to send operations to thread pool.

    .. function:: ioproc()
        
        Constructor

.. function:: iocall(ioproc &io, Function f)

    Call function in thread-pool and wait for result.

.. function:: iocallasync(ioproc &io, Function f)

    Call function in thread-pool without waiting for a result.

.. function:: iowait(iochannel reply_chan)

    Wait for result of a previous :func:`iocallasync` call.

Examples
========

.. code-block:: c++
    
    #include <ten/ioproc.hh>

    int main() {
        using namespace ten;
        ioproc io;

        int ret = iocall(io, [] {
            // this will execute in a thread
            return usleep(100);
        });

        // make several calls
        iochannel reply_chan;
        for (int i=0; i<4; ++i) {
            iocallasync(io, blocking_work, reply_chan);
        }

        // collect results
        std::vector<result> results;
        for (int i=0; i<4; ++i) {
            results.push_back(iowait<result>(reply_chan));
        }
    }

