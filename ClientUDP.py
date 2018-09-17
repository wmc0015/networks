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
	try:
		opCode = int(input('Enter operation code: '))
		if (opCode not in range(0, 255)):
			raise ValueError
	except (SyntaxError, ValueError, NameError):
		print("Invalid input. Try again.")
		continue
	
	try:
		op1 = int(input('Enter operand 1: '))
		if (op1 not in range(-32768, 32767)):
			raise ValueError
	except (SyntaxError, ValueError, NameError):
		print("Invalid input. Try again.")
		continue
	
	if (opCode != 6):
		try:
			op2 = int(input('Enter operand 2: '))
			if (op2 not in range(-32768, 32767)):
				raise ValueError
		except (SyntaxError, ValueError, NameError):
			print("Invalid input. Try again.")
			continue
		numOperands = 2
		msgLength = 8
	else:
		numOperands = 1
		msgLength = 6

	requestID = random.randint(0, 256)

	if numOperands == 2:
		request = struct.pack('!BBBBhh', msgLength,
		requestID, int(opCode), numOperands,
		int(op1), int(op2))
			
		print("\nSent: %s %s %s %s %s %s %s %s" % 
			(hex(ord(request[0])), hex(ord(request[1])),
			hex(ord(request[2])), hex(ord(request[3])),
			hex(ord(request[4])), hex(ord(request[5])),
			hex(ord(request[6])), hex(ord(request[7]))))
	else:
		request = struct.pack('!BBBBh', msgLength,
		requestID, int(opCode), numOperands,
		int(op1))
		
		print("\nSent: %s %s %s %s %s %s %s" % 
			(hex(ord(request[0])), hex(ord(request[1])),
			hex(ord(request[2])), hex(ord(request[3])),
			hex(ord(request[4])), hex(ord(request[5])),
			hex(ord(request[6]))))

	start = time.time();
	numBytes = sock.sendto(request, (serverName, port))

	response, addr = sock.recvfrom(7)
	end = time.time();
	timeElapsed = 1000 * (end - start);

	print("\nReceived: %s %s %s %s %s %s %s" % 
			(hex(ord(response[0])), hex(ord(response[1])),
			hex(ord(response[2])), hex(ord(response[3])),
			hex(ord(response[4])), hex(ord(response[5])),
			hex(ord(response[6]))))
	
	(msgLength, requestID, errorCode, result) = struct.unpack('!BBBi', response)

	if (errorCode == 127):
		print("\n----------- Request %d -----------" %requestID)
		print("\tERROR: Invalid request")
		print("----- took %.2f ms round trip -----" %timeElapsed)
	else:
		print("\n----------- Request %d -----------" %requestID)
		print("\tRequest Number: %d" %requestID)
		print("\tResult: %d" %result)
		print("----- took %.2f ms round trip -----" %timeElapsed)
		
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