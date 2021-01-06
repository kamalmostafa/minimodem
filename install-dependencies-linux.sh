#!/bin/bash
set -e
set -x
set -o pipefail

sudo $(which apt) update
sudo $(which apt) install libsndfile1-dev fftw3-dev libasound2-dev libpulse-dev libsndio-dev
sudo $(which apt) install pkg-config automake
