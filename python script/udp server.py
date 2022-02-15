import socket
import time

udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)  
# AF_INET 表示使用IPv4, SOCK_DGRAM 则表明数据将是数据报(datagrams)，
# 对应 TCP 的是 SOCK_STREAM 表明数据对象是字节流(stream)
udp.bind(('192.168.0.71', 3333))
def sensorconfig(add, config):
   
    return add+ config
while 1:
    data, addr = udp.recvfrom(1024)  # 返回数据以及发送数据的地址
    
    print(data.decode('utf-8'))
    add='255 ,'
    config='1,3,0,0,0,0,0,0,0,0,0,18,152,7,160'
    
    reply=sensorconfig(add, config)
    udp.sendto(reply.encode(),addr)
    #time.sleep(5)
udp.close()

 
