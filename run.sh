#!/bin/bash

# 1. Kill any process occupying port 9090
PID=$(lsof -t -i:9090)
if [ ! -z ]