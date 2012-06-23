#!/bin/sh
echo hit ^D to exit. >&2

while read line
do
   echo -n '>> '
   wget -q -O - "http://localhost:8008/$line"
   echo ' <<'
done

