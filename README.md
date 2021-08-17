This is Access Control system for Linux based single board computers (e.g. Orange Pi Zero)
and OSDP readers (e.g. Idesco ....) or EPSO readers (eg. Roger ....).

It provide own libraries for [OSDP](https://www.securityindustry.org/industry-standards/open-supervised-device-protocol/) and [EPSO](http://www.alse.ro/wp-content/uploads/2017/03/EPSO2-Protocol-v2.29-Rev.D.pdf).


## install

1. install *libsqlite3-dev* and *libgpiod-dev*
2. build access control application by run `make kd-idesco` or `make kd-roger`
3. install aplication, systemd service, configs, database:

	cp build/kd-idesco.elf /usr/local/bin/AccessControl
	
	cp share/AccessControl.service /etc/systemd/system/
	cp share/config /etc/kd-config
	cp share/kd-database config /var/lib/kd-config
	
	systemctl enable AccessControl
	systemctl start AccessControl

## history

This projects start as merge code from two [ICM UW](http://icm.edu.pl/) projects:
 1) OSDP implementation on Orange Pi zero (2019)
 2) Roger RFID reader based access control system (2013)
see LICENSE file for details.
