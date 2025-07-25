#!/usr/bin/env python

# Send a command to an ots instance

import sys
import socket
USAGE='send host:port <command string>'

buf=''

def send_gateway_command(host, port, command, parameters):
    buf = command + "," + ",".join(parameters)
    s = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
    s.settimeout(1.0)
    s.sendto( buf.encode('UTF-8'), (host,int(port)) )
    response = ""
    while True:
        try:
            data, addr = s.recvfrom(2048)
            response += data.decode('UTF-8')
        except socket.timeout:
            break    
    return response

def main(argv):
    if len(argv) < 3: print(USAGE); sys.exit()
    node,port = argv[1].split(':')
    
    response = send_gateway_command(node, port, argv[2], argv[3:])
    print(response)
    pass


if __name__ == "__main__": main(sys.argv)
