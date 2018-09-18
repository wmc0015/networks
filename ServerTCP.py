import socket
import sys
import struct

if (len(sys.argv) != 2):
	print("missing port number")
	exit()


port = int(sys.argv[1])


if (port not in range(1024, 65536)):
	print("port out of range")
	exit()
	
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.bind((socket.gethostbyname(socket.gethostname()), port))
sock.listen(1)

conn, addr = sock.accept()
print 'Connection address: ', addr

while True:

	data = conn.recv(8)

	print "received data: ", data

	(total_message_length) = struct.unpack('!b', data[:1])

	if (total_message_length != len(data)):
		error_code = 127
		result = 0
		response = struct.pack('!bbbi', ord(chr(7)), 0, ord(chr(error_code)), result)
		conn.send(response)
		conn.close()
		continue

	(total_message_length, request_identification, op_code, number_of_operands, operand_one) = struct.unpack('!bbbbh', data[:6])

	if (total_message_length == 8):
		(operand_two,) = struct.unpack('!h', data[-2:])

	print "TML: ", total_message_length 
	print "Request ID: ", request_identification
	print "OP Code: ", op_code
	print "Number of Operands: ", number_of_operands
	print "Operand One: ", operand_one
	if (total_message_length == 8):
		print "Operand Two: ", operand_two

	error_code = 0
	result = 0

	if (op_code == 0x00):
		result = operand_one + operand_two
	elif (op_code == 0x01):
		result = operand_one - operand_two
	elif (op_code == 0x02):
		result = operand_one | operand_two
	elif (op_code == 0x03):
		result = operand_one & operand_two
	elif (op_code == 0x04):
		result = operand_one >> operand_two
	elif (op_code == 0x05):
		result = operand_one << operand_two
	elif (op_code == 0x06):
		result = ~operand_one
	else:
		error_code = 127

	response = struct.pack('!bbbi', ord(chr(7)), request_identification, ord(chr(error_code)), result)

	conn.send(response)

conn.close()