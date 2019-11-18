 ncat -k -l 5678 | while read line; do echo "`date +'%F %T'` $line"; done >> /tmp/kd.log
