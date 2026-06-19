let csrfToken = "";
let currentUser = ""; // current logged-in user's ID
let currentUserRole = ""; // current logged-in user's role

// Check session and acquire CSRF token on page load
fetch("/api/me")
    .then(async (res) => {
        const data = await res.json();
        if (res.ok && data.status === "success") {
            csrfToken = data.csrf_token;
            currentUser = data.username;
            currentUserRole = data.role;

            // Load posts on login success
            loadPosts();
        } else {
            alert("Please login first!");
            window.location.href = "/login.html";
        }
    })
    .catch(() => {
        alert("Authentication server communication failed.");
        window.location.href = "/login.html";
    });

// Bind event for returning to dashboard
document.getElementById("backBtn").addEventListener("click", () => {
    window.location.href = "/index.html";
});

// Bind event for submitting a new post
document.getElementById("submitPostBtn").addEventListener("click", () => {
    const titleInput = document.getElementById("postTitle");
    const contentInput = document.getElementById("postContent");

    const title = titleInput.value.trim();
    const content = contentInput.value.trim();

    if (!title || !content) {
        alert("Please fill in both title and content.");
        return;
    }

    const params = new URLSearchParams();
    params.append("title", title);
    params.append("content", content);

    // Call create post API with CSRF token header
    fetch("/api/post", {
        method: "POST",
        headers: {
            "Content-Type": "application/x-www-form-urlencoded",
            "X-CSRF-TOKEN": csrfToken,
        },
        body: params.toString(),
    })
        .then(async (res) => {
            const data = await res.json();
            if (res.ok) {
                // Clear inputs and refresh post list
                titleInput.value = "";
                contentInput.value = "";
                loadPosts();
            } else {
                alert(data.message || "Failed to create post.");
            }
        })
        .catch(() => {
            alert("Failed to communicate with the server.");
        });
});

// Load post list and render securely using DOM API
function loadPosts() {
    fetch("/api/posts")
        .then(async (res) => {
            const data = await res.json();
            if (res.ok) {
                const tbody = document.getElementById("postTableBody");
                tbody.innerHTML = ""; // Clear existing list

                data.forEach((post) => {
                    const tr = document.createElement("tr");

                    // Add ID column
                    const tdId = document.createElement("td");
                    tdId.textContent = post.id;
                    tr.appendChild(tdId);

                    // Add Title column with clickable link to view details
                    const tdTitle = document.createElement("td");
                    const aTitle = document.createElement("a");
                    aTitle.href = "#";
                    aTitle.textContent = post.title; // Use textContent to prevent XSS
                    aTitle.addEventListener("click", (e) => {
                        e.preventDefault();
                        viewPostDetail(post.id);
                    });
                    tdTitle.appendChild(aTitle);
                    tr.appendChild(tdTitle);

                    // Add Author column
                    const tdAuthor = document.createElement("td");
                    tdAuthor.textContent = post.author; // Use textContent to prevent XSS
                    tr.appendChild(tdAuthor);

                    // Add Action column with delete button
                    const tdAction = document.createElement("td");

                    // Render Delete button only if the user is the author or an admin
                    if (
                        post.author == currentUser ||
                        currentUserRole == "ADMIN"
                    ) {
                        const btnDelete = document.createElement("button");
                        btnDelete.textContent = "Delete";
                        btnDelete.addEventListener("click", () => {
                            deletePost(post.id);
                        });
                        tdAction.appendChild(btnDelete);
                    } else {
                        tdAction.textContent = "-"; // Show a dash if unauthorized
                    }
                    tr.appendChild(tdAction);

                    tbody.appendChild(tr);
                });
            }
        })
        .catch(() => {
            console.error("Failed to load posts.");
        });
}

// Load specific post detail and display modal
function viewPostDetail(id) {
    fetch("/api/post?id=" + id)
        .then(async (res) => {
            const data = await res.json();
            if (res.ok) {
                // Update modal content securely using textContent
                document.getElementById("modalTitle").textContent = data.title;
                document.getElementById("modalAuthor").textContent =
                    data.author;
                document.getElementById("modalContent").textContent =
                    data.content;

                // Show overlay and modal
                document.getElementById("modalOverlay").style.display = "block";
                document.getElementById("detailModal").style.display = "block";
            } else {
                alert(data.message || "Failed to load post details.");
            }
        })
        .catch(() => {
            alert("Failed to communicate with the server.");
        });
}

// Close modal helper function
function closeModal() {
    document.getElementById("modalOverlay").style.display = "none";
    document.getElementById("detailModal").style.display = "none";
}

// Bind modal close button event
document.getElementById("closeModalBtn").addEventListener("click", closeModal);

// Close modal when clicking on the overlay background
document.getElementById("modalOverlay").addEventListener("click", closeModal);

// Send post deletion request with CSRF token header
function deletePost(id) {
    // Show confirmation dialog before sending delete request
    // In Automation Test Driver(Antigravity right, upper chrome button is that) dismiss confirm(), alert(), etc...
    if (!confirm("Are you sure you want to delete this post?")) {
        return;
    }

    fetch("/api/post?id=" + id, {
        method: "DELETE",
        headers: {
            "X-CSRF-TOKEN": csrfToken,
        },
    })
        .then(async (res) => {
            const data = await res.json();
            if (res.ok) {
                loadPosts(); // Refresh list
            } else {
                alert(data.message || "Failed to delete post.");
            }
        })
        .catch(() => {
            alert("Failed to communicate with the server.");
        });
}
