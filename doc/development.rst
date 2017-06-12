Unit Tests
==========

The OSD software comes with an extensive set of unit tests, written using the `Check framework <https://libcheck.github.io/check/>`_.

You can run the unit tests by calling (in the build directory)

.. code-block:: sh

  make check

Sometimes it's helpful to debug the tests themselves using gdb.
The following command line runs a compiled test under gdb (replace ``check_com`` with your test):

.. code-block:: sh

  make check # build the tests (if not already done)
  CK_FORK=no libtool --mode=execute gdb tests/unit/check_com


To gain insight into which parts of the software are tested you can generate a coverage report.
To build and execute all tests and to finally generate a coverage report as HTML page run

.. code-block:: sh

  make check-code-coverage
  firefox tests/unit/opensocdebug-*-coverage/index.html
