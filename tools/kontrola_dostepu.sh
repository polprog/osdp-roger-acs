#!/bin/sh

runKD() {
        killall kontrola_dostepu;
        cp -f /mnt/mtd/access.db /tmp/access.db;
        /mnt/mtd/kontrola_dostepu "$1" &
}

touch -d "1970-01-01 00:00:00" /tmp/access.db
while true; do
        s1=$(ls -l /mnt/mtd/access.db | awk '{print $5}')
        s2=$(ls -l /tmp/access.db | awk '{print $5}')
        
        if [ /mnt/mtd/access.db -nt /tmp/access.db -o ${s1=0} -ne ${s2=0} ]; then
                runKD  "$1"
        elif [ "`ps | awk '$3 ~ "kontrola_dostepu" {printf("OK"); exit}'`" != "OK" ]; then
                runKD "$1"
        fi
        sleep 60
done

