#!/bin/bash

file=$1
origin=$2
modify=$3

sed -i "s/$origin/$modify/g" "$file"
