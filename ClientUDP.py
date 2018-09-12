import socket
import sys
import struct
import random
import time

if (len(sys.argv) != 3):
	print("missing server name and/or port number")
	exit()

serverName = sys.argv[1]
port = int(sys.argv[2])
if (port not in range(1024, 65536)):
	print("port out of range")
	exit()

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

promptForRequest = True;

while promptForRequest:
	opCode = input('Enter operation code: ')
	op1 = input('Enter operand 1: ')
	if (opCode != 6):
		op2 = input('Enter operand 2: ')
		numOperands = 2
		msgLength = 8
	else:
		numOperands = 1
		msgLength = 6

	requestID = random.randint(0, 256)

	request = struct.pack('!BBBBHH', msgLength,
		requestID, int(opCode), numOperands,
		int(op1), int(op2))

	start = time.time();
	numBytes = sock.sendto(request, (serverName, port))

	response, addr = sock.recvfrom(7)
	end = time.time();
	timeElapsed = 1000 * (end - start);

	print("Packet received with contents:")
	
	(msgLength, requestID, errorCode, result) = struct.unpack('!BBBI', response)

	if (errorCode == 127):
		print("Server returned an error. Please try again.")
	else:
		print("----------- Request %d -----------" %requestID)
		print("\tRequest Number: %d" %requestID)
		print("\tResult: %d" %result)
		print("----- took %.3f ms round trip -----" %timeElapsed)
		print()
		
	promptForRequestChar = '\0'
	while (promptForRequestChar != 'y' and promptForRequestChar != 'n'):
		promptForRequestChar = raw_input("Send another request? (y/n) ")
		if len(promptForRequestChar) == 0:
			continue
		promptForRequestChar = promptForRequestChar[0].lower()
	
	if (promptForRequestChar == 'y'):
		promptForRequest = True
	else:
		promptForRequest = False