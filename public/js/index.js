// When page loading, request session info inquiry to server
fetch("/api/me")
    .then(async (res) => {
        const data = await res.json();

        // If the login status is successfully confirmed
        if (res.ok && data.status === "success") {
            document.getElementById("username").textContent = data.username;

            if (data.username === "admin") {
                const adminBtn = document.getElementById("adminBtn");
                adminBtn.style.display = "inline-block";
                adminBtn.addEventListener("click", function() {
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
        alert("Server communication error occurred when checking login status.");
        window.location.href = "/login.html";
    });

// Bind logout event
document.getElementById("logoutBtn").addEventListener("click", function() {
    // Send logout POST request to server
    fetch("/logout", {
        method: "POST",
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
