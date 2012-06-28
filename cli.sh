#!/bin/sh
echo hit ^D to exit. >&2
if [ -x /user/local/bin/wget ] ; then
  alias wget=/usr/local/bin/wget
fi
session=$(/usr/local/bin/wget -q -S -O /dev/null \
        "http://localhost:8008/." 2>&1| grep session= \
          | cut -d= -f2 | cut '-d;' -f1)
echo Session is $session

                


while read line
do
   echo -n '>> '
   wget -q --header="Cookie: session=$session;" -O - "http://localhost:8008/$line"
   echo ' <<'
done

