```javascript
let selectedUser = null;

function switchMode(mode) {
    const oneOne = document.getElementById("oneOneSection");
    const oneMany = document.getElementById("oneManySection");
    const buttons = document.querySelectorAll(".mode-btn");

    buttons.forEach(btn => btn.classList.remove("active"));

    if (mode === "one-one") {
        oneOne.style.display = "block";
        oneMany.style.display = "none";
        buttons[0].classList.add("active");

        loadUsers();
    } else {
        oneOne.style.display = "none";
        oneMany.style.display = "block";
        buttons[1].classList.add("active");
    }
}


/* =========================
   ONE-ONE : USERS
   ========================= */

async function loadUsers() {
    const userList = document.getElementById("userList");
    const fileList = document.getElementById("fileList");

    userList.innerHTML = "<p>Loading users...</p>";
    fileList.innerHTML = "<p>Select a user to view files.</p>";

    try {
        const response = await fetch("/api/users");

        if (!response.ok) {
            throw new Error("Failed to load users");
        }

        const data = await response.json();

        userList.innerHTML = "";

        if (!data.users || data.users.length === 0) {
            userList.innerHTML = "<p>No users found.</p>";
            return;
        }

        data.users.forEach(username => {
            const userButton = document.createElement("button");

            userButton.className = "user-item";
            userButton.textContent = username;

            userButton.onclick = () => selectUser(username);

            userList.appendChild(userButton);
        });

    } catch (error) {
        console.error(error);
        userList.innerHTML = "<p>Unable to load users.</p>";
    }
}


/* =========================
   SELECT USER
   ========================= */

function selectUser(username) {
    selectedUser = username;

    const userItems = document.querySelectorAll(".user-item");

    userItems.forEach(item => {
        item.classList.remove("active");

        if (item.textContent === username) {
            item.classList.add("active");
        }
    });

    loadUserFiles(username);
}


/* =========================
   LOAD USER FILES
   ========================= */

async function loadUserFiles(username) {
    const fileList = document.getElementById("fileList");

    fileList.innerHTML = "<p>Loading files...</p>";

    try {
        const response = await fetch(
            `/api/users/${encodeURIComponent(username)}/files`
        );

        if (!response.ok) {
            throw new Error("Failed to load files");
        }

        const data = await response.json();

        fileList.innerHTML = "";

        if (!data.files || data.files.length === 0) {
            fileList.innerHTML = "<p>No files found.</p>";
            return;
        }

        data.files.forEach(file => {
            const row = document.createElement("div");
            row.className = "file-item";

            const info = document.createElement("div");
            info.className = "file-info";

            const name = document.createElement("div");
            name.className = "file-name";
            name.textContent = file.name;

            const size = document.createElement("div");
            size.className = "file-size";
            size.textContent = formatBytes(file.size);

            info.appendChild(name);
            info.appendChild(size);

            const actions = document.createElement("div");
            actions.className = "file-actions";

            /* Download button */
            const downloadButton = document.createElement("button");
            downloadButton.textContent = "Download";
            downloadButton.className = "download-btn";

            downloadButton.onclick = () => {
                downloadFile(username, file.name);
            };

            actions.appendChild(downloadButton);

            /* Play button for videos */
            if (isVideo(file.name)) {
                const playButton = document.createElement("button");

                playButton.textContent = "▶ Play";
                playButton.className = "play-btn";

                playButton.onclick = () => {
                    playVideo(username, file.name);
                };

                actions.appendChild(playButton);
            }

            row.appendChild(info);
            row.appendChild(actions);

            fileList.appendChild(row);
        });

    } catch (error) {
        console.error(error);
        fileList.innerHTML = "<p>Unable to load files.</p>";
    }
}


/* =========================
   DOWNLOAD FILE
   ========================= */

function downloadFile(username, filename) {
    const url =
        `/api/users/${encodeURIComponent(username)}/files/` +
        `${encodeURIComponent(filename)}`;

    window.open(url, "_blank");
}


/* =========================
   PLAY VIDEO
   ========================= */

async function playVideo(username, filename) {

    try {
        const response = await fetch("/api/play-video", {
            method: "POST",

            headers: {
                "Content-Type": "application/json"
            },

            body: JSON.stringify({
                username: username,
                filename: filename
            })
        });

        const data = await response.json();

        if (!response.ok) {
            throw new Error(data.error || "Unable to start video");
        }

        console.log("Video started:", data.output || data.message);

    } catch (error) {
        console.error(error);
        alert("Unable to start video: " + error.message);
    }
}


/* =========================
   CHECK VIDEO FILE
   ========================= */

function isVideo(filename) {
    const extension = getExtension(filename);

    const videoExtensions = [
        "mp4",
        "mkv",
        "avi",
        "mov",
        "webm",
        "m4v",
        "flv",
        "wmv"
    ];

    return videoExtensions.includes(extension);
}


/* =========================
   ONE-MANY APPLICATION
   ========================= */

async function runApplication() {

    const activity =
        document.getElementById("activity")?.value || "";

    const url =
        document.getElementById("url")?.value.trim() || "";

    const port =
        document.getElementById("port")?.value.trim() || "";

    const filename =
        document.getElementById("filename")?.value.trim() || "";

    const extraArgs =
        document.getElementById("extraArgs")?.value.trim() || "";

    const output =
        document.getElementById("applicationOutput");

    if (output) {
        output.textContent = "Starting application...";
    }

    try {

        const response = await fetch("/api/runner", {
            method: "POST",

            headers: {
                "Content-Type": "application/json"
            },

            body: JSON.stringify({
                activity: activity,
                url: url,
                port: port,
                filename: filename,
                extraArgs: extraArgs
            })
        });

        const data = await response.json();

        if (!response.ok) {
            throw new Error(data.error || "Application failed");
        }

        if (output) {
            output.textContent =
                data.output || "Application completed.";
        }

    } catch (error) {

        console.error(error);

        if (output) {
            output.textContent =
                "ERROR: " + error.message;
        }
    }
}


/* =========================
   HELPERS
   ========================= */

function getExtension(filename) {

    const parts = filename.split(".");

    if (parts.length < 2) {
        return "";
    }

    return parts.pop().toLowerCase();
}


function formatBytes(bytes) {

    if (bytes === 0) {
        return "0 B";
    }

    const units = [
        "B",
        "KB",
        "MB",
        "GB",
        "TB"
    ];

    const index =
        Math.floor(Math.log(bytes) / Math.log(1024));

    return (
        (bytes / Math.pow(1024, index)).toFixed(2)
        + " "
        + units[index]
    );
}


/* =========================
   SAFE HTML HELPERS
   ========================= */

function escapeHtml(value) {

    const div = document.createElement("div");

    div.textContent = value;

    return div.innerHTML;
}


function escapeJs(value) {

    return String(value)
        .replace(/\\/g, "\\\\")
        .replace(/'/g, "\\'");
}


/* =========================
   START
   ========================= */

document.addEventListener("DOMContentLoaded", () => {

    loadUsers();

});
```
