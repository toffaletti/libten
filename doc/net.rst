.. _net:

###
Net 
###

Overview
========

``net.hh`` contains task-aware networking apis.

Reference
=========

.. class:: netsock

    Task-aware non-blocking socket class.

.. class:: netsock_server

    Task-aware socket server. Spawns a new task for each connection.

Example
-------

.. code-block:: c++

    // TODO: netsock_server echo server example
    #include <ten/net.hh>


