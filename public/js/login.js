document.getElementById("loginForm").addEventListener("submit", function (e) {
    e.preventDefault();

    const errorMsg = document.getElementById("errorMsg");
    errorMsg.textContent = "";

    const params = new URLSearchParams();
    params.append("username", document.getElementById("username").value);
    params.append("password", document.getElementById("password").value);

    fetch("/login", {
        method: "POST",
        headers: {
            "Content-Type": "application/x-www-form-urlencoded",
        },
        body: params.toString(),
    })
        .then(async (res) => {
            const data = await res.json();

            if (res.ok) {
                window.location.href = "/index.html";
            } else {
                errorMsg.textContent = data.message;
            }
        })
        .catch(() => {
            errorMsg.textContent = "Server Communication Error";
        });
});
