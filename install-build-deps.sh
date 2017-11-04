#!/bin/bash
#
# Install development build dependencies for different Linux distributions
#

case $(lsb_release -is) in
  Ubuntu)
    sudo apt-get install \
      check \
      doxygen \
      python3 python3-venv python3-pip \
      lcov valgrind \
      libzmq3-dev \
      libczmq-dev
    ;;

  *SUSE*)
    sudo zypper install \
      libcheck0 check-devel \
      doxygen \
      python3 python3-pip \
      lcov valgrind \
      zeromq-devel zeromq \
      czmq-devel czmq-debuginfo 
    ;;

  *)
    echo Unknown distribution. Please extend this script!
    exit 1
    ;;
esac
