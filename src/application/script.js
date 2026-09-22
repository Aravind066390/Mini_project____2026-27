```javascript
let selectedUser = null;


/* =========================================================
   MODE SWITCHING
   ========================================================= */

function switchMode(mode) {

    const oneOne = document.getElementById("oneOneSection");
    const oneMany = document.getElementById("oneManySection");
    const buttons = document.querySelectorAll(".mode-btn");

    buttons.forEach(button => {
        button.classList.remove("active");
    });

    if (mode === "one-one") {

        oneOne.style.display = "block";
        oneMany.style.display = "none";

        if (buttons[0]) {
            buttons[0].classList.add("active");
        }

        loadUsers();

    } else if (mode === "one-many") {

        oneOne.style.display = "none";
        oneMany.style.display = "block";

        if (buttons[1]) {
            buttons[1].classList.add("active");
        }
    }
}


/* =========================================================
   ONE-ONE
   LOAD USERS
   ========================================================= */

async function loadUsers() {

    const userList = document.getElementById("userList");
    const fileList = document.getElementById("fileList");

    if (!userList) {
        return;
    }

    userList.innerHTML = "<p>Loading users...</p>";

    if (fileList) {
        fileList.innerHTML =
            "<p>Select a user to view files.</p>";
    }

    try {

        const response = await fetch("/api/users");

        if (!response.ok) {
            throw new Error(
                "Server returned HTTP " + response.status
            );
        }

        const data = await response.json();

        userList.innerHTML = "";

        if (!data.users || data.users.length === 0) {

            userList.innerHTML =
                "<p>No users found.</p>";

            return;
        }

        data.users.forEach(username => {

            const userButton =
                document.createElement("button");

            userButton.className = "user-item";

            userButton.textContent = username;

            userButton.onclick = function () {
                selectUser(username);
            };

            userList.appendChild(userButton);
        });

    } catch (error) {

        console.error("Error loading users:", error);

        userList.innerHTML =
            "<p>Unable to connect to server.</p>";
    }
}


/* =========================================================
   SELECT USER
   ========================================================= */

function selectUser(username) {

    selectedUser = username;

    const userItems =
        document.querySelectorAll(".user-item");

    userItems.forEach(item => {

        item.classList.remove("active");

        if (item.textContent === username) {
            item.classList.add("active");
        }
    });

    loadUserFiles(username);
}


/* =========================================================
   LOAD USER FILES
   ========================================================= */

async function loadUserFiles(username) {

    const fileList =
        document.getElementById("fileList");

    if (!fileList) {
        return;
    }

    fileList.innerHTML =
        "<p>Loading files...</p>";

    try {

        const response = await fetch(
            `/api/users/${encodeURIComponent(username)}/files`
        );

        if (!response.ok) {
            throw new Error(
                "Server returned HTTP " + response.status
            );
        }

        const data = await response.json();

        fileList.innerHTML = "";

        if (!data.files || data.files.length === 0) {

            fileList.innerHTML =
                "<p>No files found.</p>";

            return;
        }

        data.files.forEach(file => {

            const row =
                document.createElement("div");

            row.className = "file-item";


            /* ---------------------------------------------
               FILE INFORMATION
               --------------------------------------------- */

            const info =
                document.createElement("div");

            info.className = "file-info";


            const name =
                document.createElement("div");

            name.className = "file-name";

            name.textContent = file.name;


            const size =
                document.createElement("div");

            size.className = "file-size";

            size.textContent =
                formatBytes(file.size);


            info.appendChild(name);
            info.appendChild(size);


            /* ---------------------------------------------
               FILE ACTIONS
               --------------------------------------------- */

            const actions =
                document.createElement("div");

            actions.className = "file-actions";


            /* DOWNLOAD BUTTON */

            const downloadButton =
                document.createElement("button");

            downloadButton.className =
                "download-btn";

            downloadButton.textContent =
                "Download";

            downloadButton.onclick =
                function () {

                    downloadFile(
                        username,
                        file.name
                    );
                };


            actions.appendChild(downloadButton);


            /* ---------------------------------------------
               PLAY BUTTON
               Only displayed for video files
               --------------------------------------------- */

            if (isVideo(file.name)) {

                const playButton =
                    document.createElement("button");

                playButton.className =
                    "play-btn";

                playButton.textContent =
                    "▶ Play";

                playButton.onclick =
                    function () {

                        playVideo(
                            username,
                            file.name
                        );
                    };

                actions.appendChild(playButton);
            }


            row.appendChild(info);
            row.appendChild(actions);

            fileList.appendChild(row);
        });

    } catch (error) {

        console.error(
            "Error loading files:",
            error
        );

        fileList.innerHTML =
            "<p>Unable to load files.</p>";
    }
}


/* =========================================================
   DOWNLOAD FILE
   ========================================================= */

function downloadFile(username, filename) {

    const url =
        `/api/users/${encodeURIComponent(username)}` +
        `/files/${encodeURIComponent(filename)}`;

    /*
       Opening this URL allows the C server to send
       the requested file.
    */

    window.open(url, "_blank");
}


/* =========================================================
   PLAY VIDEO
   ========================================================= */

async function playVideo(username, filename) {

    try {

        console.log(
            "Starting video:",
            username,
            filename
        );


        const response =
            await fetch("/api/play-video", {

                method: "POST",

                headers: {
                    "Content-Type":
                        "application/json"
                },

                body: JSON.stringify({

                    username: username,

                    filename: filename
                })
            });


        let data;

        try {

            data = await response.json();

        } catch (jsonError) {

            throw new Error(
                "Invalid response from server"
            );
        }


        if (!response.ok) {

            throw new Error(
                data.error ||
                "Unable to start video"
            );
        }


        console.log(
            "Video process started:",
            data
        );


        /*
           The C server is expected to start:

               ./test <file-path>

           using fork() + execv().
        */

        if (data.message) {

            alert(data.message);

        } else if (data.output) {

            console.log(data.output);

        } else {

            alert("Video started.");
        }

    } catch (error) {

        console.error(
            "Error starting video:",
            error
        );

        alert(
            "Unable to start video:\n" +
            error.message
        );
    }
}


/* =========================================================
   VIDEO FILE DETECTION
   ========================================================= */

function isVideo(filename) {

    const extension =
        getExtension(filename);

    const videoExtensions = [

        "mp4",
        "mkv",
        "avi",
        "mov",
        "webm",
        "m4v",
        "flv",
        "wmv",
        "mpeg",
        "mpg",
        "3gp"
    ];

    return videoExtensions.includes(
        extension
    );
}


/* =========================================================
   ONE-MANY
   RUN APPLICATION
   ========================================================= */

async function runApplication() {

    const activityElement =
        document.getElementById("activity");

    const urlElement =
        document.getElementById("url");

    const portElement =
        document.getElementById("port");

    const filenameElement =
        document.getElementById("filename");

    const extraArgsElement =
        document.getElementById("extraArgs");

    const output =
        document.getElementById(
            "applicationOutput"
        );


    const activity =
        activityElement ?
        activityElement.value :
        "";


    const url =
        urlElement ?
        urlElement.value.trim() :
        "";


    const port =
        portElement ?
        portElement.value.trim() :
        "";


    const filename =
        filenameElement ?
        filenameElement.value.trim() :
        "";


    const extraArgs =
        extraArgsElement ?
        extraArgsElement.value.trim() :
        "";


    if (output) {

        output.textContent =
            "Starting application...";
    }


    try {

        const response =
            await fetch("/api/runner", {

                method: "POST",

                headers: {

                    "Content-Type":
                        "application/json"
                },

                body: JSON.stringify({

                    activity: activity,

                    url: url,

                    port: port,

                    filename: filename,

                    extraArgs: extraArgs
                })
            });


        let data;

        try {

            data = await response.json();

        } catch (jsonError) {

            throw new Error(
                "Invalid response from server"
            );
        }


        if (!response.ok) {

            throw new Error(
                data.error ||
                "Application failed"
            );
        }


        if (output) {

            output.textContent =
                data.output ||
                data.message ||
                "Application completed.";
        }


    } catch (error) {

        console.error(
            "Application error:",
            error
        );


        if (output) {

            output.textContent =
                "ERROR: " +
                error.message;
        }
    }
}


/* =========================================================
   FILE EXTENSION
   ========================================================= */

function getExtension(filename) {

    if (!filename) {
        return "";
    }

    const parts =
        filename.split(".");

    if (parts.length < 2) {
        return "";
    }

    return parts[
        parts.length - 1
    ].toLowerCase();
}


/* =========================================================
   FORMAT FILE SIZE
   ========================================================= */

function formatBytes(bytes) {

    if (bytes === 0) {
        return "0 B";
    }

    if (!bytes || bytes < 0) {
        return "Unknown size";
    }


    const units = [

        "B",
        "KB",
        "MB",
        "GB",
        "TB"
    ];


    const index =
        Math.floor(
            Math.log(bytes) /
            Math.log(1024)
        );


    const safeIndex =
        Math.min(
            index,
            units.length - 1
        );


    return (
        (bytes /
            Math.pow(
                1024,
                safeIndex
            )
        ).toFixed(2)
        + " "
        + units[safeIndex]
    );
}


/* =========================================================
   HTML ESCAPING
   ========================================================= */

function escapeHtml(value) {

    const div =
        document.createElement("div");

    div.textContent =
        value;

    return div.innerHTML;
}


/* =========================================================
   JAVASCRIPT STRING ESCAPING
   ========================================================= */

function escapeJs(value) {

    return String(value)

        .replace(
            /\\/g,
            "\\\\"
        )

        .replace(
            /'/g,
            "\\'"
        );
}


/* =========================================================
   REFRESH USERS
   ========================================================= */

function refreshUsers() {

    loadUsers();
}


/* =========================================================
   INITIALIZATION
   ========================================================= */

document.addEventListener(
    "DOMContentLoaded",
    function () {

        loadUsers();

    }
);
```
