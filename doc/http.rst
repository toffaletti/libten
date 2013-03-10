.. _http:

####
HTTP
####

Overview
========

Client, server, and protocol related class for HTTP.

Reference
=========

``<http/http_message.hh>``

.. class:: http_headers

    A list of HTTP headers.

.. class:: http_request

    Encapsulates an HTTP request.

.. class:: http_response

    Encapsulates an HTTP response.


``<http/client.hh>``

.. class:: http_client

    HTTP client connection.

.. class:: http_pool

    Pool of HTTP client connections to a single host.

``<http/server.hh>``

.. class:: http_exchange

    Encapsulation of HTTP request, response pair. A transaction in the HTTP sense.

.. class:: http_server

    HTTP 1.1 server that spawns a new task for every connection.

Examples
========

.. code-block:: c++

    #include <ten/http/client.hh>

    int main() {
        http_client c{"www.example.com", 80};
        http_response resp = c.get("/");
    }
