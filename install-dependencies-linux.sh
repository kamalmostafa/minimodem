#!/bin/bash

# Step 1. Update repository
sudo apt-get update

# Step 2. Setup libraries
sudo apt-get install libfftw3-devel
sudo apt-get install libsndfile-devel libasound2-devel libpulse-devel libsndio-devel

# Step 3. Build tools
sudo apt-get install make ninja-build pkg-config python3
sudo python3 -m pip install -U meson
