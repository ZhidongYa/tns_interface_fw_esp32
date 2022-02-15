import socket

import time

server=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
address=("192.168.0.71",3333)
server.bind(address)
server.listen(10)
print("Start Server!")
def sensorconfig(add, config):
   
    return add+ config
while 1:
    try:
        conn,addr=server.accept()
        
        print("recv client from {0}".format(addr))
        while 1:
            data=conn.recv(2048)
            print("recv data: {0}".format(data.decode('utf-8')))
            add='255,'
            config='1,3,0,0,0,0,0,0,0,0,0,18,152,7,160'
            reply =sensorconfig(add, config) 
            conn.sendall(reply.encode())
        
    except Exception as e:
        conn.close()
