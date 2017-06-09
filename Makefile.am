# Copyright (c) 2012-2017 by the author(s)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
# Author(s):
#   Philipp Wagner <philipp.wagner@tum.de>

EXTRA_DIST =
CLEANFILES =
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory

SUBDIRS = src tests

# prepare pkg-config file
SED_PROCESS = \
    $(AM_V_GEN)$(MKDIR_P) $(dir $@) && $(SED) \
    -e 's,@VERSION\@,$(VERSION),g' \
    -e 's,@prefix\@,$(prefix),g' \
    -e 's,@exec_prefix\@,$(exec_prefix),g' \
    -e 's,@libdir\@,$(libdir),g' \
    -e 's,@includedir\@,$(includedir),g' \
    < $< > $@ || rm $@

%.pc: %.pc.in
	$(SED_PROCESS)

.PHONY: doc
if BUILD_DOCS
SUBDIRS += doc
doc:
	$(MAKE) -C doc html
else
doc:
	@echo Run configure with --enable-docs to enable documentation building.
endif

.PHONY: check-code-coverage
if CODE_COVERAGE_ENABLED
check-code-coverage:
	$(MAKE) -C tests/unit check-code-coverage
else
	@echo Run configure with --enable-code-coverage for coverage support.
endif

# The current directory must be listed *after* the doc directory to make sure
# 'make distclean' doesn't delete doc/Makefile before 'make distclean -C doc'
# had a chance to run.
SUBDIRS += .

pkgconfigdir = $(datadir)/pkgconfig
pkgconfig_DATA = osd.pc
EXTRA_DIST += osd.pc.in $(DX_CONFIG)
CLEANFILES += osd.pc
