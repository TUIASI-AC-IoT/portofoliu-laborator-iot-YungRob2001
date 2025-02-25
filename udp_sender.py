import socket
import time

PEER_IP = "192.168.89.40"   
PEER_PORT = 10001

 
MESSAGE1 = b"GPIO4=1"
MESSAGE2 = b"GPIO4=0"

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
current_message = MESSAGE1  

try:
    while True:
      
        sock.sendto(current_message, (PEER_IP, PEER_PORT))
        print(f"Trimis: {current_message.decode()} - LED {'ON' if current_message == MESSAGE1 else 'OFF'}")
        
        
        current_message = MESSAGE2 if current_message == MESSAGE1 else MESSAGE1
        
        time.sleep(1)

except KeyboardInterrupt:
    print("\nExecuția a fost întreruptă")
finally:
    sock.close()