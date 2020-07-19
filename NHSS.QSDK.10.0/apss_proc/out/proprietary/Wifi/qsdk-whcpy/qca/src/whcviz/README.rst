whc
===

This package contains a web based demo GUI for the Whole Home Coverage
feature. This GUI consists of both server side and client side components.

Installation
============

Pre-requisites
--------------

The recommended practice is to install this package in a Python `virtual
environment`_. After activating the virtual environment, first make sure
you have the latest versions of pip and setuptools:

::

    pip install --upgrade pip setuptools

Installing
----------

This package can be installed directly from its tarball as shown below:

::

    pip install whcviz-<version>.tar.gz

Note that this package depends on the *whcdiag* and *whcmvc* packages which
must already be installed in the virtual environment or be installable from a
PyPi server for the above install command to complete successfully.

Alternatively, the package can be installed from the source tree using the
following command:

::

    python setup.py install

Note that when using this method, the Javascript dependencies must be fetched
first. Refer to the next section for these steps. This step can be skipped if
using the source tarball method given above.

Javascript Dependencies
-----------------------

Before running the server for the first time, it is necessary to install
the Javascript dependencies. This package uses `bower`_ to manage the
dependencies. Assuming `node.js`_ is already installed on your system, the
steps to install bower and fetch the dependencies are as follows:

::

    cd whcviz/htdocs
    npm install bower
    node_modules/.bin/bower install


Running the Server
==================

The ``whc-server.py`` script is used to launch a web server that listens on
port 8080. This web server receives diagnostic logs from the target and
conveys the information to connected clients (web browsers) via web sockets.

The server may be launched from any directory and must be provided the
configuration file to use. An example invocation is shown below:

::

    whc-server.py -c configs/generic-multi-ap.yaml

Once the server is running, connect to http://localhost:8080/single-ap/ or
http://localhost:8080/multi-ap/, depending on whether you are testing with a
single AP or a root AP and a range extender.

For information on how to create a configuration file, see the accompanying
documentation. Creating a custom configuration file is a necessary step prior
to running the GUI.

To have the server debug logs output to the terminal include the ``--stderr``
option when invoking the server.

Making Changes to the Package
=============================

The sections below provide some pointers to those looking to modify the package
itself.

Development Setup
-----------------

To make this package available to you for development whereby you can
edit the files directly in the source directory, set up a `virtual
environment`_ and then activate the development mode:

::

    python setup.py develop

Development Style Guidelines
----------------------------

All new code (including test code) should follow `PEP-8`_. To check
whether the code is PEP-8 compliant, use the following command:

::

    python setup.py flake8

Note that we are allowing for lines up to 100 characters in length.

Additionally, new functions and classes should include doc strings
as described in the `Google Python Style Guide`_. There is no automated
tool for checking compliance though.

Development Testing
-------------------

We are using `nose`_ for testing. To initiate the full tests, after
you've naturally setup your virtualenv and installed the required
packages, you'd run:

::

    pip install nose coverage  # only needed one time
    python setup.py nosetests

This will run the tests and report any failures along with the code
coverage information. If you want to run nose by hand, use the following
command:

::

    nosetests -v --exe --with-coverage --cover-package=whc --cover-branches

You should hopefully see all "ok" messages. If not, you can visit the
individual test file to see how and what it was trying to do within the
``tests`` directory.

.. _virtual environment: http://docs.python-guide.org/en/latest/dev/virtualenvs/

.. _PEP-8: https://www.python.org/dev/peps/pep-0008/

.. _Google Python Style Guide: https://google-styleguide.googlecode.com/svn/trunk/pyguide.html#Comments

.. _nose: https://nose.readthedocs.org/en/latest/

.. _bower: http://bower.io/

.. _node.js: https://nodejs.org/en/
