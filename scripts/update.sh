#!/usr/local/bin/bash
#
# Called periodically by the daemon to log the data
#
# $1 = timestemp
# $2 = watts
# $3 = temp
# $4 = device timestamp
# $5 = change in time
# $6 = joules used
# $7 = offset between clocks


RRDFILE=/var/currentcost/powertemp.rrd

/usr/local/bin/rrdtool update $RRDFILE N:$2:$3
/usr/local/bin/sqlite3 /var/currentcost/sqlite.db "INSERT into readings (ts, watts, temp, device_time, device_offset, ts_delta, joules) VALUES(DATETIME('now','localtime'), ${2}, ${3}, '${4}', ${7}, ${5}, ${6})"

