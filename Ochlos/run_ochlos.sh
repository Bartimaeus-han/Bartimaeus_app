#!/bin/bash

echo "Starting Ochlos Attacker Server on Port 9091 using conda environment..."
conda run --no-capture-output -n ochlos python Ochlos/server.py