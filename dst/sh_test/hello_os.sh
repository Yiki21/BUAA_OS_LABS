#!/bin/bash

fileA=$1
fileB=$2

sed -n '8p' "$fileA" > "$fileB"
sed -n '32p' "$fileA" >> "$fileB"
sed -n '128p' "$fileA" >> "$fileB"
sed -n '512p' "$fileA" >> "$fileB"
sed -n '1024p' "$fileA" >> "$fileB"
