all:
	gcc -lrbd -lrados rados_connect.c -o /usr/bin/rados_connect
uninstall:
	rm -f /usr/bin/rados_connect 
