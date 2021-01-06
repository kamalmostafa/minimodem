#!/bin/bash
set -e
set -x
set -o pipefail

brew install libsndfile fftw autoconf libtool
brew install automake || brew upgrade automake
