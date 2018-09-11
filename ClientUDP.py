import socket
import sys

if (len(sys.argv) != 3):
	print("missing server name and/or port number")
	exit()

port = int(sys.argv[2])
if (port not in range(1024, 65536)):
	print("port out of range")
	exit()

print('here')