/*
 * =========================================================
 * ZERO COPY DATA TRANSFER SYSTEM
 * Frontend JavaScript
 * =========================================================
 *
 * Backend API expected:
 *
 * GET  /api/users
 * GET  /api/users/<username>/files
 * POST /api/runner
 *
 * The browser cannot directly access:
 *
 * /APP/DATA/
 *
 * Therefore the backend must expose the filesystem
 * through these API endpoints.
 */


/* =========================================================
   GLOBAL STATE
========================================================= */

let selectedUser = null;


/* =========================================================
   MODE SWITCHING
========================================================= */

function switchMode(mode) {

    const oneOneSection =
        document.getElementById("oneOneSection");

    const oneManySection =
        document.getElementById("oneManySection");

    const oneOneBtn =
        document.getElementById("oneOneBtn");

    const oneManyBtn =
        document.getElementById("oneManyBtn");


    if (mode === "one-one") {

        oneOneSection.classList.remove("hidden");

        oneManySection.classList.add("hidden");

        oneOneBtn.classList.add("active");

        oneManyBtn.classList.remove("active");

        loadUsers();

    }


    else {

        oneOneSection.classList.add("hidden");

        oneManySection.classList.remove("hidden");

        oneOneBtn.classList.remove("active");

        oneManyBtn.classList.add("active");

    }
}


/* =========================================================
   LOAD USERS
========================================================= */

async function loadUsers() {

    const userList =
        document.getElementById("userList");

    const userCount =
        document.getElementById("userCount");


    userList.innerHTML = `
        <div class="loading">
            Loading users...
        </div>
    `;


    try {

        /*
         * Backend should return:
         *
         * {
         *     "users": [
         *         "aravind",
         *         "rahul",
         *         "john"
         *     ]
         * }
         */

        const response =
            await fetch("/api/users");


        if (!response.ok) {
            throw new Error("Failed to load users");
        }


        const data =
            await response.json();


        const users =
            data.users || [];


        userCount.textContent =
            users.length;


        if (users.length === 0) {

            userList.innerHTML = `
                <div class="empty-state">

                    <div class="empty-icon">
                        Ø
                    </div>

                    <h3>No users found</h3>

                    <p>
                        No user directories exist.
                    </p>

                </div>
            `;

            return;
        }


        userList.innerHTML = "";


        users.forEach(username => {

            const userElement =
                document.createElement("div");


            userElement.className =
                "user-item";


            userElement.dataset.username =
                username;


            userElement.innerHTML = `

                <div class="user-icon">
                    U
                </div>

                <div class="user-info">

                    <div class="user-name">
                        ${escapeHtml(username)}
                    </div>

                    <div class="user-path">
                        /APP/DATA/${escapeHtml(username)}
                    </div>

                </div>

            `;


            userElement.addEventListener(
                "click",
                () => selectUser(username)
            );


            userList.appendChild(userElement);

        });

    }


    catch (error) {

        console.error(error);


        userList.innerHTML = `

            <div class="empty-state">

                <div class="empty-icon">
                    !
                </div>

                <h3>Unable to load users</h3>

                <p>
                    Backend API is unavailable.
                </p>

            </div>

        `;

        userCount.textContent = "0";
    }
}


/* =========================================================
   SELECT USER
========================================================= */

async function selectUser(username) {

    selectedUser = username;


    /*
     * Highlight selected user.
     */

    document
        .querySelectorAll(".user-item")
        .forEach(item => {

            item.classList.remove("active");

            if (item.dataset.username === username) {
                item.classList.add("active");
            }

        });


    document.getElementById(
        "selectedUserTitle"
    ).textContent = username.toUpperCase();


    loadUserFiles(username);
}


/* =========================================================
   LOAD USER FILES
========================================================= */

async function loadUserFiles(username) {

    const fileList =
        document.getElementById("fileList");

    const fileCount =
        document.getElementById("fileCount");


    fileList.innerHTML = `
        <div class="loading">
            Loading files...
        </div>
    `;


    try {

        /*
         * Backend should return:
         *
         * {
         *     "files": [
         *         {
         *             "name": "video.mp4",
         *             "size": 123456,
         *             "type": "mp4"
         *         }
         *     ]
         * }
         */

        const response =
            await fetch(
                `/api/users/${encodeURIComponent(username)}/files`
            );


        if (!response.ok) {
            throw new Error("Failed to load files");
        }


        const data =
            await response.json();


        const files =
            data.files || [];


        fileCount.textContent =
            files.length;


        if (files.length === 0) {

            fileList.innerHTML = `

                <div class="empty-state">

                    <div class="empty-icon">
                        □
                    </div>

                    <h3>No files</h3>

                    <p>
                        This user's directory is empty.
                    </p>

                </div>

            `;

            return;
        }


        fileList.innerHTML = "";


        files.forEach(file => {

            const fileElement =
                document.createElement("div");


            fileElement.className =
                "file-item";


            const extension =
                getExtension(file.name);


            fileElement.innerHTML = `

                <div class="file-icon">
                    ${escapeHtml(extension)}
                </div>

                <div class="file-details">

                    <div class="file-name">
                        ${escapeHtml(file.name)}
                    </div>

                    <div class="file-meta">

                        <span>
                            ${formatBytes(file.size)}
                        </span>

                        <span>
                            ${escapeHtml(file.type || "FILE")}
                        </span>

                    </div>

                </div>

                <button
                    class="download-btn"
                    onclick="downloadFile(
                        '${escapeJs(username)}',
                        '${escapeJs(file.name)}'
                    )">

                    Download

                </button>

            `;


            fileList.appendChild(fileElement);

        });

    }


    catch (error) {

        console.error(error);


        fileList.innerHTML = `

            <div class="empty-state">

                <div class="empty-icon">
                    !
                </div>

                <h3>Unable to load files</h3>

                <p>
                    Could not communicate with backend.
                </p>

            </div>

        `;

        fileCount.textContent = "0";
    }
}


/* =========================================================
   DOWNLOAD FILE
========================================================= */

function downloadFile(username, filename) {

    const url =
        `/api/users/${encodeURIComponent(username)}/files/${encodeURIComponent(filename)}`;

    window.open(url, "_blank");
}


/* =========================================================
   ONE-MANY APPLICATION RUNNER
========================================================= */

document
    .getElementById("runnerForm")
    .addEventListener("submit", async function(event) {

        event.preventDefault();


        const activity =
            document.getElementById("activity").value;

        const url =
            document.getElementById("url").value;

        const port =
            document.getElementById("port").value;

        const filename =
            document.getElementById("filename").value;

        const extraArgs =
            document.getElementById("extraArgs").value;


        const runButton =
            document.getElementById("runBtn");

        const output =
            document.getElementById("runnerOutput");

        const status =
            document.getElementById("runnerStatus");


        /*
         * Show running state.
         */

        runButton.disabled = true;

        status.textContent = "RUNNING";

        status.className =
            "runner-status running";


        output.innerHTML = `

            <div class="terminal-line">

                <span class="terminal-prefix">
                    $
                </span>

                <span>
                    Starting application_runner...
                </span>

            </div>

        `;


        /*
         * Create request for backend.
         */

        const requestData = {

            activity: activity,

            url: url,

            port: port,

            filename: filename,

            extraArgs: extraArgs

        };


        try {

            /*
             * Backend endpoint:
             *
             * POST /api/runner
             *
             * The backend is responsible for
             * executing application_runner.c.
             */

            const response =
                await fetch("/api/runner", {

                    method: "POST",

                    headers: {
                        "Content-Type":
                            "application/json"
                    },

                    body:
                        JSON.stringify(requestData)

                });


            const result =
                await response.json();


            if (!response.ok) {

                throw new Error(
                    result.error ||
                    "Application failed"
                );

            }


            /*
             * Display application output.
             */

            output.innerHTML = `

                <div class="terminal-line">

                    <span class="terminal-prefix">
                        $
                    </span>

                    <span>
                        application_runner started
                    </span>

                </div>

                <div class="terminal-line">

                    <span>
                        Activity:
                        ${escapeHtml(activity)}
                    </span>

                </div>

                <div class="terminal-line">

                    <span>
                        URL:
                        ${escapeHtml(url)}
                    </span>

                </div>

                <br>

                <div class="terminal-line output-success">

                    <span>
                        ${escapeHtml(
                            result.output ||
                            "Application completed successfully."
                        )}
                    </span>

                </div>

            `;


            status.textContent =
                "COMPLETED";

        }


        catch (error) {

            console.error(error);


            status.textContent =
                "ERROR";

            status.className =
                "runner-status error";


            output.innerHTML = `

                <div class="terminal-line">

                    <span class="terminal-prefix">
                        $
                    </span>

                    <span class="output-error">
                        Application failed
                    </span>

                </div>

                <br>

                <div class="terminal-line output-error">

                    <span>
                        ${escapeHtml(error.message)}
                    </span>

                </div>

            `;

        }


        finally {

            runButton.disabled = false;

        }

    });


/* =========================================================
   HELPER FUNCTIONS
========================================================= */

function getExtension(filename) {

    const parts =
        filename.split(".");

    if (parts.length < 2) {
        return "FILE";
    }

    return parts
        .pop()
        .toUpperCase();
}


function formatBytes(bytes) {

    if (!bytes || bytes === 0) {
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
        Math.floor(
            Math.log(bytes) /
            Math.log(1024)
        );


    return (
        parseFloat(
            (bytes /
             Math.pow(1024, index))
            .toFixed(2)
        )
        + " "
        + units[index]
    );
}


/*
 * Prevent HTML injection when displaying
 * filenames/usernames returned by backend.
 */

function escapeHtml(value) {

    return String(value)
        .replaceAll("&", "&amp;")
        .replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;")
        .replaceAll('"', "&quot;")
        .replaceAll("'", "&#039;");
}


/*
 * Used inside onclick attributes.
 */

function escapeJs(value) {

    return String(value)
        .replaceAll("\\", "\\\\")
        .replaceAll("'", "\\'");
}


/* =========================================================
   INITIALIZATION
========================================================= */

document.addEventListener(
    "DOMContentLoaded",
    () => {

        loadUsers();

    }
);
