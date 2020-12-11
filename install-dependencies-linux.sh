#!/bin/bash
set -e
set -x
set -o pipefail

sudo apt update
sudo apt install libsndfile1-dev fftw3-dev libasound2-dev libpulse-dev libsndio-dev
sudo apt install pkg-config automake
