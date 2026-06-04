document.getElementById("signupForm").addEventListener("submit", function (e) {
    e.preventDefault();

    const msg = document.getElementById("msg");
    msg.textContent = "";

    const params = new URLSearchParams();
    params.append("username", document.getElementById("username").value);
    params.append("password", document.getElementById("password").value);

    fetch("signup", {
        method: "POST",
        headers: {
            "Content-Type": "application/x-www-form-urlencoded",
        },
        body: params.toString(),
    })
        .then(async (res) => {
            if (res.ok) {
                alert("Sign up completed, Redirecting to login page...");
                window.location.href = "/login.html";
            } else {
                const data = await res.json();

                msg.style.color = "red";
                msg.textContent = data.message;
            }
        })
        .catch(() => {
            msg.style.color = "red";
            msg.textContent = "Server Communication Error";
        });
});
