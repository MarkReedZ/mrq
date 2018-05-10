
import socket
import os, struct
 
def Main():
  host = '127.0.0.1'
  port = 7000

  bstr = "[1,2,3,4,5,6,7,8,9,10]".encode("utf-8")
  bstr = "111111111111111111111111".encode("utf-8")
  bstr = "11111111".encode("utf-8")
  #bstr = b'\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01'
  #bstr = b'\x01\x01\x01\x01\x01\x01\x01\x01'
  #bstr += b'\x01\x01\x01\x01\x01\x01\x01\x01'
  l = len(bstr)
  bstr = b'\x00\x01\x00\x00' + struct.pack("I",l) + bstr
  #print (bstr)
  bench_cmd = b'\x00\x09\x00\x00'
  bstr = bstr*10000+bench_cmd
  #message = "hello"
  #transport.write(bstr)

       
  mySocket = socket.socket()
  mySocket.connect((host,port))
   
  for x in range(10):
    mySocket.send(bstr)

  bench_cmd = b'\x00\x09\x00\x00'
  mySocket.send(bench_cmd)
     
  mySocket.close()
 
if __name__ == '__main__':
    Main()
