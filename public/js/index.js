// Store CSRF token global variable
// This value is stored in `javascript runtime memory variable`
let csrfToken = "";

// When page loading, request session info inquiry to server
fetch("/api/me")
    .then(async (res) => {
        const data = await res.json();

        // If the session verification is successfully completed
        if (res.ok && data.status === "success") {
            document.getElementById("username").textContent = data.username;

            // Assign CSRF toekn returned in the response to the variable
            csrfToken = data.csrf_token;

            // Check the role (Role Based Access Control)
            if (data.role === "ADMIN") {
                const adminBtn = document.getElementById("adminBtn");
                adminBtn.style.display = "inline-block";
                adminBtn.addEventListener("click", function () {
                    window.location.href = "/admin.html";
                });
            }
        } else {
            // If not logged in or invalid session, redirect to login page
            alert("please login!");
            window.location.href = "/login.html";
        }
    })
    .catch(() => {
        alert(
            "Server communication error occurred when checking login status.",
        );
        window.location.href = "/login.html";
    });

// Bind logout event
document.getElementById("logoutBtn").addEventListener("click", function () {
    // Send logout POST request to server
    fetch("/logout", {
        method: "POST",
        headers: {
            "X-CSRF-Token": csrfToken, // send stored token
        },
    })
        .then((res) => {
            if (res.ok) {
                // If logout is successful, redirect to login page
                window.location.href = "/login.html";
            } else {
                alert("Logout failed on server");
            }
        })
        .catch(() => {
            alert("Server Communication error occurred during logout.");
        });
});
