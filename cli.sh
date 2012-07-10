#!/bin/sh
echo hit ^D to exit. >&2

COOKIES=cookies.tmp
HEADERS=headers.tmp

    while read line
do
   echo -n '>> '
   curl -b $COOKIES -c $COOKIES -D $HEADERS \
     -s "http://localhost:8008/$line"
   session=$(sed -ne '/session/ s/.*\t//p' "$COOKIES")
   location=$(sed -ne '/Content-Location/ s/.* \([^\r]*\).*/\1/p' "$HEADERS")
   echo " << s="\"$session\"" u="\"$location\"
   #cat $HEADERS
done
