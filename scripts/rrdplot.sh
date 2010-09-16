#!/usr/local/bin/bash

cd `dirname $0`

# Output directory
DESTDIR=/var/currentcost/graphs

/usr/local/bin/rrdtool graph $DESTDIR/power-10min.png \
--start end-10m --width 700 --end now --slope-mode \
--no-legend --vertical-label Watts --lower-limit 0 \
--alt-autoscale-max \
DEF:Power=powertemp.rrd:Power:AVERAGE \
LINE3:Power#0000FF:"Average" > /dev/null 2> /dev/null

/usr/local/bin/rrdtool graph $DESTDIR/power-1hour.png \
--start end-60m --width 700 --end now --slope-mode \
--no-legend --vertical-label Watts --lower-limit 0 \
--alt-autoscale-max \
DEF:Power=powertemp.rrd:Power:AVERAGE \
LINE3:Power#0000FF:"Average" > /dev/null 2> /dev/null

/usr/local/bin/rrdtool graph $DESTDIR/power-12hour.png \
--start end-12h --width 700 --end now --slope-mode \
--no-legend --vertical-label Watts --lower-limit 0 \
--alt-autoscale-max \
DEF:Power=powertemp.rrd:Power:AVERAGE \
LINE3:Power#0000FF:"Average" > /dev/null 2> /dev/null


/usr/local/bin/rrdtool graph $DESTDIR/power-1day.png \
--start end-24h --width 700 --end now --slope-mode \
--no-legend --vertical-label Watts --lower-limit 0 \
--alt-autoscale-max \
DEF:Power=powertemp.rrd:Power:AVERAGE \
DEF:PowerMin=powertemp.rrd:Power:MIN \
DEF:PowerMax=powertemp.rrd:Power:MAX \
CDEF:PowerRange=PowerMax,PowerMin,- \
LINE1:PowerMin: \
AREA:PowerRange#0000FF11:"Error Range":STACK \
LINE1:PowerMin#0000FF33:"Min" \
LINE1:PowerMax#0000FF33:"Max" \
LINE1:Power#0000FF:"Average" > /dev/null 2> /dev/null

/usr/local/bin/rrdtool graph $DESTDIR/power-1week.png \
--start end-7d --width 700 --end now --slope-mode \
--no-legend --vertical-label Watts --lower-limit 0 \
--alt-autoscale-max \
DEF:Power=powertemp.rrd:Power:AVERAGE \
DEF:PowerMin=powertemp.rrd:Power:MIN \
DEF:PowerMax=powertemp.rrd:Power:MAX \
CDEF:PowerRange=PowerMax,PowerMin,- \
LINE1:PowerMin: \
AREA:PowerRange#0000FF11:"Error Range":STACK \
LINE1:PowerMin#0000FF33:"Min" \
LINE1:PowerMax#0000FF33:"Max" \
LINE1:Power#0000FF:"Average" > /dev/null 2> /dev/null

/usr/local/bin/rrdtool graph $DESTDIR/power-1month.png \
--start end-1m --width 700 --end now --slope-mode \
--no-legend --vertical-label Watts --lower-limit 0 \
--alt-autoscale-max \
DEF:Power=powertemp.rrd:Power:AVERAGE \
DEF:PowerMin=powertemp.rrd:Power:MIN \
DEF:PowerMax=powertemp.rrd:Power:MAX \
CDEF:PowerRange=PowerMax,PowerMin,- \
LINE1:PowerMin: \
AREA:PowerRange#0000FF11:"Error Range":STACK \
LINE1:PowerMin#0000FF33:"Min" \
LINE1:PowerMax#0000FF33:"Max" \
LINE1:Power#0000FF:"Average" > /dev/null 2> /dev/null

/usr/local/bin/rrdtool graph $DESTDIR/power-1year.png \
--start end-1y --width 700 --end now --slope-mode \
--no-legend --vertical-label Watts --lower-limit 0 \
--alt-autoscale-max \
DEF:Power=powertemp.rrd:Power:AVERAGE \
DEF:PowerMin=powertemp.rrd:Power:MIN \
DEF:PowerMax=powertemp.rrd:Power:MAX \
CDEF:PowerRange=PowerMax,PowerMin,- \
LINE1:PowerMin: \
AREA:PowerRange#0000FF11:"Error Range":STACK \
LINE1:PowerMin#0000FF33:"Min" \
LINE1:PowerMax#0000FF33:"Max" \
LINE1:Power#0000FF:"Average" > /dev/null 2> /dev/null

