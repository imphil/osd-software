#!/bin/bash
#
# Install build dependencies for different Linux distributions
#

case $(lsb_release -is) in
  Ubuntu)
    sudo apt-get install \
      check \
      doxygen python3 python3-venv python3-pip \
      lcov
    ;;

  *SUSE*)
    sudo zypper install \
      libcheck0 check-devel \
      doxygen python3 python3-pip \
      lcov
    ;;

  *)
    echo Unknown distribution. Please extend this script!
    exit 1
    ;;
esac
