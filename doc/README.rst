Documentation for Open SoC Debug (OSD)
======================================

This repository contains most documentation content for the `Open SoC Debug project <http://www.opensocdebug.org>`_.
It consists of design/overview documents, the Open SoC Debug specification, and implementer documentation.
Some documentation is auto-generated from different repositories, such as API documentations for the reference implementation.

View the documentation
----------------------

You can view the documentation generated from this repository online at
http://opensocdebug.readthedocs.org.


Build the documentation
-----------------------

The documentation is converted to HTML and PDF using `Sphinx <http://www.sphinx-doc.org/>`_. All Python dependencies are installed in a virtual environment, which is automatically created in the build process.

Build requirements
~~~~~~~~~~~~~~~~~~
- Python 3 with venv and pip

Additionally to build PDFs

- Inkscape (to convert images to PDF)
- A texlive installation

If you're using Debian or Ubuntu, run

.. code:: bash

  # in all cases
  apt-get install python3 python3-venv python3-pip

  # to build PDFs
  apt-get install inkscape latexmk texlive-latex-recommended texlive-latex-extra texlive-fonts-recommended

Building
~~~~~~~~
To build the HTML documentation locally, run ``make html`` inside the top-level source directory, or run ``make latexpdf`` for a PDF version.
You can find the resulting documentation in a sub-folder of the ``build`` directory.
