#!/bin/bash

# Triggers the build of a docker container with all the dependencies that are
# needed to compile LoAT.

docker build --target swine-docker -t swine-docker ../docker
