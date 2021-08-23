#!/usr/bin/env bash

export ACCESS_TOKEN=$(gcloud auth print-access-token)
START_TIME=$(date +%H:%M:%S.%N); /usr/bin/time -v parallel --jobs 50 < parallel.sh; echo "parallel: $START_TIME..$(date +%H:%M:%S.%N)"
