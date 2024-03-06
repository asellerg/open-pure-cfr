cards="AKQJT98765432"
length=${#cards}
for ((i = 0; i < length; i++)); do
    card="${cards:i:1}"
    /home/asellerg/rdbparser/UB22_x86_64/bin/rdbp -f /home/asellerg/data/dump.rdb -e "$card*" > "/home/asellerg/data/rdb/$card.data" && sed -i -e '1d;$d' -e 's/"//g' -e 's/ : /,/g' "/home/asellerg/data/rdb/$card.data" &
done