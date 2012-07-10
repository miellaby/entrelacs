#!/bin/sh
echo hit ^D to exit. >&2
if [ -x /user/local/bin/wget ] ; then
  alias wget=/usr/local/bin/wget
fi
COOKIES=cookies.tmp

    while read line
do
   echo -n '>> '
   wget --load-cookies=$COOKIES --save-cookies=$COOKIES --keep-session-cookies \
     -q -O - "http://localhost:8008/$line"
   session=$(sed -ne '/session/ s/.*\t//p' "$COOKIES")
   echo " << ["$session"]"
done

