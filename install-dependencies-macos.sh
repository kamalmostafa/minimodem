#!/bin/bash
set -e
set -x
set -o pipefail

brew install libsndfile fftw
brew install automake || brew upgrade automake
