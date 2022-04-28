#!/bin/bash

test_dir=$(dirname "$0")
max_test_id=$(ls ${test_dir}/*.in | sort -n | xargs -n 1 basename -s ".in" | sort -n | tail -n 1)
next_id=$(printf "%03d" $((10#$max_test_id + 1)))
touch ${test_dir}/${next_id}.{in,out}
