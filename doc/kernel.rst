######
Kernel
######

Overview
========

The kernel of the task system. Types and functions for setup and information about the environment.

Reference
=========

.. type:: kernel::clock

   The clock used for task related time. Currently std::chrono::steady_clock.

.. type:: kernel::time_point

   A point in time. std::chrono::time_point<std::chrono::steady_clock>.

.. function:: time_point kernel::now()

   Return cached time from scheduler loop. Not precise, but fast.

.. function:: bool kernel::is_main_thread()

   Returns true if this is the thread main() is called in.

.. function:: size_t kernel::cpu_count()

   Returns the number of available cpus.

.. function:: void kernel::boot()

   Bootstrap the task system. Must be called from the main thread before any other threads are created. Can be implicitely called by other task functions.

.. function:: void kernel::shutdown()

   Perform a clean shutdown of the task system. Cancel all tasks and wait for them to exit.
