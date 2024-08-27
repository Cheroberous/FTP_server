
//    testing

to test the project run "./fast.sh"
that will properly compile the server and client ".c" files (including the server
functions stored in "common.h" and the client functions in "common_c.h") 
producing a "./s" and a "./c".


You can use the client to login in your main account or create new account from the client itself
inserting as credentials "username,password" the username and password of the new account to create on the server (see below)

First start the server (run as sudo) , then the clients

To test them a sample file is provided "file_test.txt" (for the basic command "ls,put,get") 
Place the file in the (at least two) accounts on the server to start the 6-steps test phase provided in the slides

- basic informations

Some strings will be displayed at client side to explain how to properly use the 
program, at first the client is requested to send it's credential in the format:

"username,password"   *

*if you want to create a new account just put the username and password you want to set

then (if the autentication succeeds) you will be able to use the implemented command:

"get/put filename" 

(use relative filename path)

the same pattern is used for mkdir/rmdir commands:

"mkdir/rmdir directory"

while for ls we can just send "ls" to the server.


Note: most of the printf calls present a sintax like this:  if(DEBUG)printf("....");
      to avoid those print to be displayed please set the value of "DEBUG" in the .h files to 0. (and run ./fast.sh)


//               CLIENT  Description

The client can be run with (and without) command line argument, if we want to specify an ip address
or a custom domain name:

1) ./c numeric_ipaddress 
2) ./c example.com 
3) ./c

if run without arguments it will try the connection on the local host
on a well-known port.

- after the connection it will show the welcome message from the server and enter a "login loop"

This loop will take from standard input a string containing credentials in the format
"username,password" and send it to the server that will check them for correctness

- based on the server response it will skip to the "main" while loop or cycle in the first one

Note: root access is blocked at client side by not sending the msg to the server is username==root

Note_1: to avoid forced closured the Ctrl+C combination is intercepted (SIGINT) and proper messages are sent to the server before closing.




--// command exchange

- the second loop will take from standard input the commands that the client want to remotly
execute on behalf of the user on the server, available commands are:

put -> used to upload files in the remote home directory on the server "put filename"
get -> used to retrive files from the remote directory "get filename"
ls -> used to list files and dir in the remote home directory
mkdir/rmdir -> create or delete a new directory "mkdir new_dir"/"rm new_dir"


Note: proper checks when trying to put and get a file are performed, 
      when non existent files are requested, the server will send back an error msg (the client can retry)
      when the client will try to send a non existent local file, the program will tell him to select an existente file

based on the command sent (checked using strncpm) the client will prepare
to receive an ack from the server "received message", or to send/receive
the file/content requested.

the strings exchanged between client and server are "\n" terminated and the function
that receive the messages will read until it find the special character:


 do {
        ret = recv(socket_fd, buf + ricevuti, 1, 0);
        if (ret == -1 && errno == EINTR) continue;
        if (ret == -1) handle_error("Cannot read from the socket");
        if (ret == 0) break;
	} while ( buf[ricevuti++] != '\n' );


while for file send/receive a function will read from file/socket until no more bytes are sent:

(file is the file descriptor opened for the local or remote file)

while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        int r=send(clientSocket, buffer, bytesRead, 0);
        printf("sent %d bytes \n",r);
    }
    ..........continue




----// special commands

- if the client wants to quit from the connection we can use the "QUIT" command (taken from standard input)
  defined in common_c.h as "SERVER_COMMAND" 

- if the client want to shutdown the server we can send "SD" (shutdown) defined in common_c.h as "SERVER_SHUTDOWN"
  (see also the server description for further details)


Note: this commands are available both in the login loop and the command exchange loop


// SERVER description

-- main

As requested the server takes an argument from command line but, given that user are 
effectivly created in the server (they are not fictional), the "useradd" command will automatically 
place their home directory in "/home".

- the main will set up the handler to be used when a SIGCHLD signal is received (used later), 

- create the needed semaphore (used to signal the intent of the client to shutdown the server, so to exchange information from 
  the child process to the main process) (proper check on successfull opening is done)

- populate the file "name_uid" with account information taken from /etc/passwd (for later use)

- populate the structure to be used by the "bind" and "listen" function (statefull connection -> stream socket)

- then call "mprocserver()"


-- function "mprocserver"

    a while loop characterized by the "accept" function handle the different clients that
    try to connect to the server (proper check of descriptor is performed)

    - for each accepted connection a fork is done and the function "connection_handler" is called to 
      manage the specific client connection

    - the main process will instead update the child counter (used later in the sigchld_handler)
      and start again the loop waiting for a new client
    
-- function "connection_handler"

    - set up variables and buffers to be use in the function
    - send a welcome message and enter in the authentication loop

    // authentication/creation loop

    - check if client is sending credential or want to exit/shutdown (a "canary" will be set to skip the second loop in this case)
    - if the client is sending credential and the username already exists the server will try to login using the provided password (and will send an ACK/NACK)
    - else it will create the client calling "crea_utente"

    - if the autentication succeds/the user is created, the process will set the proper uid and gid and change the 
      working directory to the one of the new user

    - now we enter in the second while loop 

    // Commands/file exchange loop

        the received command is matched against the ones that are supported by the server using
        several "strcmp(comando,bytes)==0", different behaviour are used to properly fullfill the requests

        - get: the path where the file will be taken is build concatenating the user home directory with the file name 
          and the "sendFile" function is called to send it (proper check on the file existence are performed)

        - put: the path where the file will be put (on the server) is built and the file is received using 
          the "receiveFile" function (proper check on the file existence are performed client side)

        - ls: using the "popen" with argument ls, the file list of the user home directory will be retrived 
          and sent using "send_msg" function (if it is empty a custom message will be sent)

        - mkdir/rmdir: thanks to the use of "execvp" the requested command is executed and a ACK message is sent to the client


        --// special commands
        
        - the received command is also checked in case the user sent "QUIT" or "SD"

          -> if "QUIT" is received the clien will close the descriptor and exit
          -> if "SD" is received the semaphore previously opened will be updated to 1 (sem_post) and the child process
             will exit triggering the "sigchld_handler"



-- sigchld_handler

every time a child process terminate this function will be triggered, we are interested to check if a child process handling a client
have updated the semaphore value to indicate the willing of the client to shut down the server,
if so the server will stop listening for further connection using "shutdown" and will wait for the other child to return.
Then a cleanup function will be called to release the semaphore, close descriptor and then the server will be closed.

///////////////////////////////////


Note: as reported in this file the "EXTRA BONUS (1)" was covered by implementing the pair "mkdir/rmdir" commands
