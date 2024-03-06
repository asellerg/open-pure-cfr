#!/bin/sh

grep_output=$(grep -n $1 /sda/data/phs/$2_boards_*)
file=$(echo $grep_output | sed -n "s/.*$2_boards_\([0-9]*\).txt:\([0-9]*\):.*/\1/p")
line=$(echo $grep_output | sed -n "s/.*$2_boards_\([0-9]*\).txt:\([0-9]*\):.*/\2/p")
cluster=$(sed -n "$line"p /home/asellerg/pkmeans/assignments/$2/$file/aa | awk '{print $2}')
grep "^$cluster$3" /sda/open_pure_cfr/avg_strategy/$4
