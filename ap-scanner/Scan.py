import serial
import time

s = serial.Serial('COM3', 115200, timeout=10)

print("Waiting for boot...")
while True:
    line = s.readline().decode('latin-1').strip()
    print(line)
    if 'Ghost ready' in line:
        print("Ghost is ready! Sending command...")
        break

time.sleep(1)
s.write(b'CMD:SCAN_AP\n')
print("Command sent!")

start = time.time()
while time.time() - start < 15:
    line = s.readline().decode('latin-1').strip()
    if line:
        print(line)

s.close()