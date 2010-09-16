#!/bin/sh

# Create the SQLite DB

if [ -f sqlite.db ]; then
	echo "Error: sqlite.db already exists"
	exit 1
fi

sqlite3 sqlite.db <<EOF
CREATE TABLE readings (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	ts datetime,
	device_time time,
	watts int(11),
	temp double,
	device_offset int(11),
	ts_delta double(8,2),
	joules int(11)
);

create index ts_index on readings(ts);
EOF


