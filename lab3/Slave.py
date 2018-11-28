import socket
import sys
import struct
import threading

def byteToHex(byteStr):
    newStr = list(byteStr)
    arr = [None] * len(byteStr)
    for t in range (len(newStr)):
        nex = ord(newStr[t])
        arr[t] = "0x{:02x} ".format(nex)
    return ''.join(arr).strip()

def calculateChecksum(str):
    acc = 0
    for i in range(len(str)):
        acc += ord(str[i])
        acc = (acc & 0xFF) + (acc >> 8)
    return (~acc)&0xFF        

def forwardMessages():
    sock_udp_server = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock_udp_server.bind(("0.0.0.0", myServerPortNumber))

    while True:
        data, addr = sock_udp_server.recvfrom(73)
        
        print("received: %s" %byteToHex(data))

        (slaveGroupID, magicNum, timeToLive, ridDest, ridSource) = struct.unpack('!BiBBB', data[:8])

        #print("Slave Group ID: %d" %slaveGroupID)
        #print("Magic Number: %d" %magicNum)
        #print("Time To Live: %d" %timeToLive)
        #print("Destination RID: %d" %ridDest)
        #print("Source RID: %d" %ridSource)

        msgPayload = data[8:-1]
        (checksum,) = struct.unpack('!B', data[-1:]) 

        #print("Message Payload: %s" %msgPayload)
        #print("Checksum: %d" %checksum)

        if (timeToLive <= 1):
            print("Your message died :(")
            continue


        if (checksum != calculateChecksum(data[:-1])):
            #drop or forward?
            print("Checksums do not match.")
            #print("calculated %d" %calculateChecksum(data[:-1]))
            continue

        if (myRID != ridDest):
            # forward message
            print("Message not for me :(")
            timeToLive -= 1
            data = struct.pack('!BiBBB', slaveGroupID, magicNum, timeToLive, ridDest, ridSource)
            for i in range(len(msgPayload)):
                data += msgPayload[i] 
            data += (chr(calculateChecksum(data)))
            
            sock_udp_server.sendto(data, nextSlaveIpPort)
            continue

        print("Message for you from node %d!" %ridSource)
        print("Message: %s" %msgPayload)
        print("\n")

def promptMessages():
    sock_udp_client = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    while True:
        msgPayload = raw_input("Enter a message: ")
        ridDest = int(raw_input("Enter a destination RID: "))

        groupID = 7
        magicNum = 1248819489
        timeToLive = 255
        ridSource = myRID

        message = struct.pack('!BiBBB', groupID, magicNum, timeToLive, ridDest, ridSource)
        for i in range(len(msgPayload)):
            message += msgPayload[i] 
        message += (chr(calculateChecksum(message)))

        sock_udp_client.sendto(message, nextSlaveIpPort)

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

(masterGroupID, magicNum, myRID, nextSlaveIP1, nextSlaveIP2, nextSlaveIP3, nextSlaveIP4) = struct.unpack('!BiBBBBB', response)

nextSlaveIP = str(nextSlaveIP1) + '.' + str(nextSlaveIP2) + '.' + str(nextSlaveIP3) + '.' + str(nextSlaveIP4)

print("Group ID: %d" %masterGroupID)
print("My RID: %d" %myRID)
print("Next IP: %s" %nextSlaveIP)

myServerPortNumber = 10010 + (masterGroupID % 30) * 5 + myRID
nextSlaveIpPort = (nextSlaveIP, (myServerPortNumber - 1))

threading.Thread(target=forwardMessages).start()
threading.Thread(target=promptMessages).start()