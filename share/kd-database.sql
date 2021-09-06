CREATE TABLE 'doors' (
	'doorid'        INTEGER PRIMARY KEY AUTOINCREMENT,
	'name'          TEXT
);
CREATE TABLE 'users' (
	'userid'        INTEGER PRIMARY KEY AUTOINCREMENT,
	'card'          TEXT UNIQUE,
	'pin'           TEXT,
	'name'          TEXT,
	'expire_date'   INTEGER
);
CREATE TABLE 'user_door' (
	'userid'        INTEGER,
	'doorid'        INTEGER,
	'allow_no_pin'  BOOLEAN,
	'time_expr'     TEXT,
	FOREIGN KEY(userid) REFERENCES users(userid),
	FOREIGN KEY(doorid) REFERENCES doors(doorid)
);
CREATE TABLE keys (
	'cardid'        TEXT,
	'doorid'        INTEGER,
	FOREIGN KEY(doorid) REFERENCES doors(doorid)
);

-- for database interface can be used also:
--  CREATE TABLE zones (zoneid INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, desc TEXT);
--  CREATE TABLE zone_door (zoneid INT, doorid INT, FOREIGN KEY(zoneid) REFERENCES zones(zoneid), FOREIGN KEY(doorid) REFERENCES doors(doorid));
--  CREATE TABLE user_zone (userid INT, zoneid INT, FOREIGN KEY(userid) REFERENCES users(userid),  FOREIGN KEY(zoneid) REFERENCES zones(zoneid));
-- then 'user_door' table can be generated from zones info via:
--  DELETE FROM user_door; INSERT INTO user_door SELECT user_zone.userid, zone_door.doorid, user_zone.time_expr, '0' FROM user_zone JOIN zones JOIN zone_door ON (user_zone.zoneid = zones.zoneid AND zones.zoneid = zone_door.zoneid);
