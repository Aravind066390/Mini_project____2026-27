[SETUP] Provisioning default 'admin' credentials into PostgreSQL database...
WARNING:  database "postgres" has a collation version mismatch
WARNING:  database "postgres" has a collation version mismatch
INSERT 0 1
-c "CREATE TABLE IF NOT EXISTS users (username VARCHAR(128) PRIMARY KEY, password VARCHAR(128));"-c "INSERT INTO users (username, password) VALUES ('admin', 'adminpass') ON CONFLICT DO NOTHING;"[SETUP] Starting server in background process...
[SERVER] Primary server started on port 8012...

================ RUNNING TEST SUITE ================

--- Testing ADD_USER ---
WARNING:  database "postgres" has a collation version mismatch
DETAIL:  The database was created using collation version 2.41, but the operating system provides version 2.42.
HINT:  Rebuild all objects in this database that use the default collation and run ALTER DATABASE postgres REFRESH COLLATION VERSION, or build PostgreSQL with the right library version.
[CLIENT] Authentication successful for 'admin'.
WARNING:  database "postgres" has a collation version mismatch
DETAIL:  The database was created using collation version 2.41, but the operating system provides version 2.42.
HINT:  Rebuild all objects in this database that use the default collation and run ALTER DATABASE postgres REFRESH COLLATION VERSION, or build PostgreSQL with the right library version.
INSERT 0 1
[CLIENT] Command Response: ADD_USER_SUCCESS

--- Testing PUT (local_test.txt -> remote_test.txt) ---
-c "INSERT INTO users (username, password) VALUES ('testuser', 'testpass');"WARNING:  database "postgres" has a collation version mismatch
DETAIL:  The database was created using collation version 2.41, but the operating system provides version 2.42.
HINT:  Rebuild all objects in this database that use the default collation and run ALTER DATABASE postgres REFRESH COLLATION VERSION, or build PostgreSQL with the right library version.
[CLIENT] Authentication successful for 'testuser'.
[CLIENT] Server assigned data transfer port: 41959
[CLIENT] File upload successfully completed.

--- Testing GET (downloading user directory contents for testuser) ---
WARNING:  database "postgres" has a collation version mismatch
DETAIL:  The database was created using collation version 2.41, but the operating system provides version 2.42.
HINT:  Rebuild all objects in this database that use the default collation and run ALTER DATABASE postgres REFRESH COLLATION VERSION, or build PostgreSQL with the right library version.
[CLIENT] Authentication successful for 'testuser'.
[CLIENT] Server assigned data transfer port: 39901
[CLIENT] File download successfully completed.

================ TEST SUITE COMPLETED ================
[CLEANUP] Stopping background server (PID 59501)...
                                                                                                                                                                                    
Received:-
Hello! This payload verifies full-stack TCP transmission.
                                                                                                                                                                                    
Sent:-
Hello! This payload verifies full-stack TCP transmission.
