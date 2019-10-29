all:
	gcc TFTP_server.c -o TFTP_server
clean:
	rm TFTP_server