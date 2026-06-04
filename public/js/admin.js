fetch("/api/users")
    .then(async (res) => {
        const data = await res.json();
        if (res.ok) {
            const tbody = document.getElementById("userTableBody");
            const fragment = document.createDocumentFragment();

            data.forEach((user) => {
                const tr = document.createElement("tr");
                const td = document.createElement("td");

                td.textContent = user.username; // completely blocked Stored XSS

                tr.appendChild(td);
                // add node on virtual fragment
                fragment.appendChild(tr);
            });

            tbody.appendChild(fragment);
        } else {
            document.getElementById("errorMsg").textContent =
                data.message || "Access Denied";
        }
    })
    .catch(() => {
        document.getElementById("errorMsg").textContent =
            "Server communication error occured when loading data.";
    });
