#!/bin/ksh

directory="/path/to/directory"

for file in "$directory"/*; do
    basename "$file"
done