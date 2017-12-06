import socket
import time
HOST = '192.168.1.137'    # The remote host
PORT = 80              # The same port as used by the server
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((HOST, PORT))
    while (True):
        #s.sendall(b'Hello, dan is cool')
        action = input("Press 's' to send\n'r' to receive\n'x' to exit: ")
        if (action == "s"):
            data = input("Enter data to send: ")
            s.sendall(data.encode())
        elif (action == "r"):
            data = s.recv(1024)
            print('Received', repr(data))
        elif (action == "x"):
            break;
        #time.sleep(.5)
