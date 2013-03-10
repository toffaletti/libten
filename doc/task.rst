.. _task:

####
Task
####

Overview
========

Tasks are lightweight cooperatively scheduled threads of execution useful for concurrency. For example, an HTTP server might spawn a new task for every connection. Tasks are scheduled cooperatively within their operating system thread which means they must explicitely give up control by yielding, sleeping, or performing non-blocking io. The :ref:`net module <net>` provides wrappers around non-blocking io that will block a task until a file descriptor is ready.

Blocking
--------
Attention to blocking and non-blocking operations is important when using tasks. Tranditional blocking prevents concurrency. However, there are task-blocking calls like :func:`this_task::sleep_for` which only cause the current task to block, but allows other tasks to run. To avoid confusion we will refer to task-blocking operations as non-blocking.

libten provides non-blocking network io that presents a task-blocking api. This greatly simplifies implementing non-blocking networking.

If you must call something that would block a thread, you can use :ref:`ioproc <ioproc>`.

Reference
=========

namespace this_task
-------------------

``<task/task.hh>``

.. function:: uint64_t this_task::get_id()

    Returns the current task's id.

.. function:: void this_task::yield()

    Yield to allow other tasks to run

.. function:: void this_task::sleep_until(kernel::time_point sleep_time)

    Blocks the execution of the current task until specified sleep_time has been reached.

.. function:: void this_task::sleep_for(std::chrono::duration sleep_duration)

    Blocks the execution of the current task for at least the specified sleep_duration.

task
----

``<task/task.hh>``

.. class:: task

    Task objects represent a thread of execution for concurrent execution.

   .. function:: static void set_default_stacksize(size_t stacksize)

        Set the stack size to be used for future tasks spawned in this thread.

   .. function:: static task spawn<>(Function f, Args args)

        Spawn a new task that will be executed next scheduling cycle.

   .. function:: uint64_t get_id() const

        Return the id of this task.

.. class:: task_interrupted

    Exception used to unwind task stack when task is canceled. :class:`deadline_reached` is the only sub-class.

.. type:: optional<std::chrono::milliseconds> optional_timeout

    Used for passing optional timeouts.

deadline
________

``<task/deadline.hh>``

.. class:: deadline

    deadline is useful when you want to perform several operations within a certain time limit. If the timeout expires a :class:`deadline_reached` exception is thrown within the task.

    .. function:: deadline(optional_timeout timeout)

        Schedule a deadline to occur in timeout milliseconds.

    .. function:: cancel()

        Cancel deadline timeout.

.. class:: deadline_reached : task_interrupted

    Exception thrown in task when deadline timeout is reached.

qutex
-----

``<task/qutex.hh>``

.. class:: qutex

    Task-aware mutex classes.

    .. function:: void lock()

    .. function:: void unlock()

    .. function:: bool try_lock()

rendez
------

``<task/rendez.hh>``

.. class:: rendez

    Task-aware condition_variable.

    .. function:: void sleep(unique_lock<qutex> &lk)

    .. function:: void wakeup()

    .. function:: void wakeupall()

Examples
========

.. code-block:: c++

    #include <ten/task.hh> 

    int main() {
        using namespace ten;
        using namespace std::chrono;
        // kernel::boot is implicitly called here, the first time we use the task system
        task::spawn([] {
            this_task::sleep_for(seconds{1});
        });
        // implicitly wait for all tasks before exit.
    }

