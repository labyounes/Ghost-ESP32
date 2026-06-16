import serial
import time

PORT      = 'COM3'
BAUD_RATE = 115200

def open_serial():
    return serial.Serial(PORT, BAUD_RATE, timeout=5)

def wait_ready(s):
    print("Waiting for Ghost to boot...")
    while True:
        line = s.readline().decode('latin-1').strip()
        if line:
            print(f"  {line}")
        if 'GHOST:READY' in line:
            print("Ghost is ready.\n")
            break

def send_cmd(s, cmd):
    s.write((cmd + '\n').encode())
    print(f">> {cmd}")

def main():
    s = open_serial()
    wait_ready(s)
    time.sleep(0.5)

    ssid = input("Enter SSID to clone (e.g. HomeNetwork): ").strip()
    if not ssid:
        print("No SSID entered. Exiting.")
        s.close()
        return

    send_cmd(s, f'CMD:TWIN_START:{ssid}')
    print(f"\nFake AP '{ssid}' is now broadcasting.")
    print("Check your phone — you should see the network appear.")
    print("Press Ctrl+C to stop.\n")

    try:
        while True:
            line = s.readline().decode('latin-1').strip()
            if line:
                print(f"  {line}")
    except KeyboardInterrupt:
        print("\nStopping AP...")
        send_cmd(s, 'CMD:TWIN_STOP')
        time.sleep(1)
        while True:
            line = s.readline().decode('latin-1').strip()
            if line:
                print(f"  {line}")
            if 'TWIN:AP_STOPPED' in line:
                break

    s.close()
    print("Done.")

if __name__ == '__main__':
    main()
