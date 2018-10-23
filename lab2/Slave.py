import socket
import sys
import struct

if (len(sys.argv) != 3):
    print("missing master host name and/or port number")
    exit()

masterName = sys.argv[1]
port = int(sys.argv[2])
if (port not in range(1024, 65536)):
    print("port out of range")
    exit()

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((masterName, port))

joinRequest = struct.pack('!Bi', 7, 1248819489)

sock.send(joinRequest)

response = sock.recv(75)

(groupID, magicNum, yourRID, nextSlaveIP1, nextSlaveIP2, nextSlaveIP3, nextSlaveIP4) = struct.unpack('!BiBBBBB', response)
(groupID, magicNum, yourRID, nextSlaveIPasInt) = struct.unpack('!BiBI', response)

nextSlaveIPasInt = socket.ntohl(nextSlaveIPasInt)

print("%x" %nextSlaveIPasInt)

nextSlaveIP = str(nextSlaveIP1) + '.' + str(nextSlaveIP2) + '.' + str(nextSlaveIP3) + '.' + str(nextSlaveIP4)

print("GroupID: %d" %groupID)
print("Magic Num: %d" %magicNum)
print("Your RID: %d" %yourRID)
print("Next IP: %s" %nextSlaveIP)