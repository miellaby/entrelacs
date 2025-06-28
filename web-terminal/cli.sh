#!/bin/sh
echo hit ^D to exit. >&2
COOKIES=/var/tmp/cookies.$$.tmp
HEADERS=/var/tmp/headers.$$.tmp
echo -n '>> '

while read line
do
    case "$line" in 
        \#*) 
            # comment
            ;;
        *)
            # check line contains "@@@"
            cited=$(echo $line | sed -ne 's/.*@@@\(.*\)/\1/p')
            if [ "$cited" ]
            then
                # $prog@@@$cited formated line send a POST request of $cited file at url $prog
                prog=$(echo $line | sed -e "s/@@@.*//")
                curl -b $COOKIES -c $COOKIES -D $HEADERS --data-binary "@$cited" \
                    -s "http://localhost:8080/$prog"
            else
                curl -b $COOKIES -c $COOKIES -D $HEADERS \
                    -s "http://localhost:8080/$line"
            fi  
            echo
            session=$(sed -ne '/session/ s/.*\t//p' "$COOKIES")
            location=$(sed -ne '/Content-Location/ s/.* \([^\r]*\).*/\1/p' "$HEADERS")
            echo " s="\"$session\"" u="\"$location\"
            ;;
    esac
    #cat $HEADERS
    echo -n '>> '
done
rm "$COOKIES"
rm "$HEADERS"