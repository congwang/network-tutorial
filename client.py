import socket

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

s.connect(('localhost', 8080))

s.sendall(b'Hello World')

data = s.recv(1024)

print('Received', repr(data))

s.close()