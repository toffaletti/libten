.. _internals:

##############
Task Internals
##############

Overview
========

This section describes libten's implementation.

Task Implementation
===================
``src/task_impl.hh`` declares the private implementation part of task.
``src/task.cc`` defines task internals.

.. class:: task::pimpl

Thread Context
==============

``src/thread_context.hh`` 

.. class:: thread_context

    Thread-local storage for task system internals. It contains the per-thread task scheduler and c-ares context used for dns lookups.

    .. member:: scheduler scheduler

    .. member:: shared_ptr<ares_channeldata> dns_channel

.. member:: thread_context this_ctx

    Used to reference the thread local context. Whenever it is used, it potentially causes the task system to be initialized in the thread.

Context
=======
``src/context.hh`` is the context switching component used for swapping stacks. It is a thin wrapper around the boost::context api.

.. class:: context

Stack Allocation
================
``src/stack_alloc.hh`` is used by ``context.hh`` to allocate the alternate stack for spawned tasks. Stacks have a guard page write-protected using ``mprotect``. Stacks are recycled using a thread local cache to reduce calls to ``posix_memalign`` and ``mprotect``.

.. class:: stack_allocator

Scheduler
=========
``src/scheduler.hh`` is where the magic happens. It keeps a list of spawned tasks and schedules them in FIFO order. The exception to this is when a task is spawned it goes to the front of the ready queue and will be run next. When no tasks are ready to run the scheduler either waits on a ``std::condition_variable`` or the io manager calls ``epoll_wait`` if tasks are waiting for io events.

.. class:: scheduler

Epoll IO
========
``src/io.hh`` defines the epoll io manager. There is zero or one of these per thread. The scheduler creates it on-demand the first time a task waits for io. ``timerfd`` is used for timeouts and ``eventfd`` is used to break out of ``epoll_wait`` when a task is woken up from other threads.

.. class:: io

C-ARES
======

``src/cares.cc`` contains the code for non-blocking dns lookups using the c-ares library. Of note is that c-ares only reads ``/etc/resolv.conf`` when ``ares_init`` is called. In order to see changes to ``resolv.conf``, libten uses the ``inotify`` api to watch ``resolv.conf`` for changes.

