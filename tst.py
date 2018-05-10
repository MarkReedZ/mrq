
import multiprocessing
import asyncio
import socket
import os, struct

class MrProtocol(asyncio.Protocol):
  def __init__(self, client):
    pass
  def connection_made(self, transport):
    print ("connect")
    bstr = "[1,2,3,4,5,6,7,8,9,10]".encode("utf-8")
    #bstr = "1111111111111111111111".encode("utf-8")
    l = len(bstr)
    bstr = b'\x00\x01\x00\x00' + struct.pack("I",l) + bstr
    bench_cmd = b'\x00\x09\x00\x00'
    bstr = bstr*1000

    transport.write(bench_cmd)

    for x in range(40000):
      transport.write(bstr)

    transport.write(bench_cmd)

  def data_received(self, data):
    pass
    #print('Data received: {!r}'.format(data.decode()))

  def connection_lost(self, exc):
    print('The server closed the connection')

class Client(object):
  def __init__(self, host, port):
    self._protocol_factory = MrProtocol;
    self.conn = None;

    workers = set()
    for port in [7000]:#[7000,7001,7002,7003]:
      worker = multiprocessing.Process( target=self.doconn, kwargs=dict(port=port) )
      worker.daemon = True
      worker.start()
      workers.add(worker)
    #worker = multiprocessing.Process( target=self.doconn, kwargs=dict(port=7000) )
    #worker.daemon = True
    #worker.start()
    #workers.add(worker)

    for worker in workers:
      worker.join()

  def doconn(self, port):
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    coro = loop.create_connection(lambda: self._protocol_factory(self), '127.0.0.1', port)
    loop.run_until_complete(coro)
    try:
      loop.run_forever()
    finally:
      loop.close()

  def setConnection(self, conn):
    self.conn = conn


if __name__ == '__main__':

  mc = Client( "127.0.0.1", 7000 )


