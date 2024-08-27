#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>  // htons()
#include <netinet/in.h> // struct sockaddr_in
#include <sys/socket.h>
#include <semaphore.h>
#include <fcntl.h> 
#include <signal.h>
#include <sys/wait.h>
#include "common.h"


typedef struct handler_args_s
{
    int socket_desc;
    struct sockaddr_in* client_addr;
} handler_args_t;

int total_proc=0;
int num_figli=0;

//Specify fields for the arguments that will be populated in the
//main thread and then accessed in connection_handler(void* arg).
 

// we use a global variable to store a pointer to the named semaphore              // insert the cleanup (done)
sem_t* named_semaphore;
sem_t* named_sem_file;
int server_socket_desc;


void cleanup() {
    /** Close and then unlink the named semaphore **/
    printf("\rSome cleaning...\n");

    if (sem_close(named_semaphore)) handle_error("sem_close error");
    if (sem_close(named_sem_file)) handle_error("sem_close1 error");

    /* We unlink the semaphores, otherwise it would remain in the
     * system after the server dies. */
    if (sem_unlink(SEMAPHORE_NAME)) handle_error("sem_unlink error");
    if (sem_unlink(SEMAPHORE_FILE)) handle_error("sem_unlink1 error");

    
}

// Function to handle the SIGCHLD signal
void sigchld_handler(int signo) {
    if(DEBUG)printf("called handle per figli chiusi\n");
    (void)signo; // Unused parameter
    int r;
    int current_value=0;
    int status;
    pid_t child_pid;


    r =sem_getvalue(named_semaphore,&current_value);                     
       
        if(current_value==1){
            printf("client asked for server shutdown...goodbye my friend\n");

            // disabling further send/receive operations (no more incoming connections accepted)
            shutdown(server_socket_desc,SHUT_RDWR);

            printf("waiting for all child to terminate\n");
            for (int i = 0; i < num_figli; i++) {
                r = wait(&status);
                if(r == -1 && errno == ECHILD) {
                    printf("no other child to wait\n");
                }
                if (WEXITSTATUS(status)) {
                    fprintf(stderr, "ERROR: child died with code %d\n", WEXITSTATUS(status));
                    
                }
            }
            printf("[Main] All the children have terminated!!!\n");
            cleanup();
            close(server_socket_desc);
            printf("\rShutting down the server...goodbye\n");

            exit(EXIT_SUCCESS);
        }
        else{
            if(DEBUG)printf("current value : %d\n",current_value);
            if(DEBUG)printf("the client didn't want to exit\n");
            
        }




   

}


//uso struct client_addr
void connection_handler(int socket_desc, struct sockaddr_in* client_addr) {      

    // ovviamente descrittore socket client
    //printf("nuovo processo figlio creato per gestire client %d \n",num_figli);


    //semaphore open

    sem_t* my_named_semaphore = sem_open(SEMAPHORE_NAME, 0); // oflag is 0: sem_open is not allowed to create it!
    if (my_named_semaphore == SEM_FAILED) {
        printf("Could not open the named semaphore from child \n");
        
    }
    sem_t* my_named_semaphore_file = sem_open(SEMAPHORE_FILE, 0); // oflag is 0: sem_open is not allowed to create it!
    if (my_named_semaphore_file == SEM_FAILED) {
        printf("Could not open the named semaphore1 from child \n");
        
    }
    // used variables

    int ret, recv_bytes;
    char* uid_utente = (char*)malloc (20);
    memset(uid_utente,0,20);

    char* gid_utente = (char*)malloc (20);
    memset(gid_utente,0,20);

    char* home_directory= (char*)malloc(20);
    memset(home_directory,0,20);




    uid_t uid_format;
    gid_t gid_format;


    char buf[1024];
    char buf1[1024];
    char buf_standard[1024];
    char buf_succ[1024];
    char buf_fail[1024];
    size_t buf_len = sizeof(buf);
    int msg_len;
    int canary=0;

    char* quit_command = SERVER_COMMAND;
    size_t quit_command_len = strlen(quit_command);
    char* shutdown_command = SERVER_SHUTDOWN;
    size_t shutdown_command_len = strlen(shutdown_command);
    // username,passwor,uid

    char** string_cred = (char**)malloc(2 * sizeof(char*));
    if (string_cred == NULL) {
            perror("Memory allocation failed");
            exit(EXIT_FAILURE);
        }

   

    /////////////////////////////////////////////////////////

    // parse client IP address and port
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);
    uint16_t client_port = ntohs(client_addr->sin_port); // port number is an unsigned short

 

    // set up buffer for standard response

    sprintf(buf, "Hi! I'm an echo server. You are %s talking on port %hu.\nI will send you back whatever"
            " you send me. I will stop if you send me %s :-)\n", client_ip, client_port, quit_command);
    sprintf(buf1, "Hi! I'm an ftp server, please login sending 'username,password' or register using the same sintax\n");
    sprintf(buf_standard, "received your message\n");
    sprintf(buf_succ, "ok\n");
    sprintf(buf_fail, "error\n");

    msg_len = strlen(buf1);
    int bytes_sent = 0;

    int r=send_msg(socket_desc,buf1,msg_len,bytes_sent);          // send welcome message




    // authentication/creation loop    ----------------------------------------- 

    while (1) {

      
        // read message from client
        memset(buf1, 0, buf_len);          //reset buffer to receive
        recv_bytes = 0;

        recv_msg(socket_desc,buf1,&recv_bytes);  // receive message from user (nome , password)
       


       // check if he wants to quit
        if (recv_bytes == 0){ printf("esco\n");canary=1;break;}
        if (recv_bytes == shutdown_command_len && !memcmp(buf1, shutdown_command, shutdown_command_len)){            
            printf("chiamato shutdown dal client\n");
            if (sem_post(my_named_semaphore)) {
            printf("Could not unlock the semaphore from child\n");
          
            }
            else{
                if(DEBUG)printf("agiornato valore?\n");
                int c_v=0;
                ret = sem_getvalue(named_semaphore, &c_v);

                if (ret) {
                    printf("Could not access the named semaphore");
                    exit(1);
                }

                else{
                    printf("valore nel sem == %d\n",c_v);
                }
            }
            if(sem_close(my_named_semaphore)){
            printf("Could not close the semaphore from child \n");
          

            }
            canary=1;
            break;
        }
        if (recv_bytes == quit_command_len && !memcmp(buf1, quit_command, quit_command_len)){
            printf("Client want to quit\n");
            //aggiorno semaforo
            

            canary=1;                                   //used to exit also the second loop
            break;}


        if(DEBUG)printf("ricevuto %s",buf1);
        //printf("byte ricevuti == %d\n",recv_bytes);
        //printf("byte per exit == %ld\n",quit_command_len);


        
        //check credentials


        ////////////////------------------------------- split them (input: user msg)
        ret =split_cred(buf1,string_cred);

        // add terminator
        int i =0;
        while(string_cred[1][i]!='\0'){
            i++;
        }
        string_cred[1][i-1]='\0';                     // set the string terminator o the string received from the user
  


        if(DEBUG)printf("username = %s \n",string_cred[0]);

        if(DEBUG)printf("password = %s \n",string_cred[1]);
      
        ///////////////------------------------------------


        //controlla se l'account esiste altrimenti lo crea 
        // salva nel file anche la home directory che prende da etc/passwd 
        

        ret = check_existence(string_cred[0],uid_utente,home_directory,gid_utente); 

        //printf("user id stringa == %s\n",uid_utente);   


        if(ret == 0){
            printf("user creation\n");
            
            ret = crea_utente(string_cred[0],string_cred[1]);                    // crea utente

            if(ret==0){
            send_msg(socket_desc,buf_succ,strlen(buf_succ),0); // rimanda msg al client 
            }
        
            

            sleep(1);    //delay to update user list (debug)


            // theuse of a semaphore ensure one process at a time to access the file to write the new user

            if (sem_wait(my_named_semaphore_file)) {
                printf("Could not unlock the semaphore1 from child\n");
          
            }

            ret = popola_username_uid();           // udate file with list user info "name_uid"


            if (sem_post(my_named_semaphore_file)) {
                printf("Could not unlock the semaphore1 from child\n");
          
            }

            if(sem_close(my_named_semaphore_file)){                          // each child can trigger the user creation just once so we can close the semaphore rigth away
                printf("Could not close the semaphore1 from child \n");
          

            }

            ret = check_existence(string_cred[0],uid_utente,home_directory,gid_utente);   // used to retrive info about the user

            if(DEBUG)printf("user id stringa == %s\n",uid_utente);
            if(DEBUG)printf("user home-directory  == %s\n",home_directory);
            
            
            uid_format = (uid_t)strtoul(uid_utente, NULL, 10);
            gid_format = (gid_t)strtoul(gid_utente, NULL, 10);

            if(DEBUG)printf("Converted UID utente: %u\n", uid_format);
            if(DEBUG)printf("Converted GID utente: %u\n", gid_format);

            printf("logging in ...\n");

            ret = change_uid(uid_format,gid_format);                 // cambia user id e group id

            ret = set_new_working_dir(home_directory);              // setta la sua working dir

            break;



        } 

        else{                  

            // the user exist (already retrived info), check password

            if(DEBUG)printf("user id stringa == %s\n",uid_utente);
            if(DEBUG)printf("user home-directory  == %s\n",home_directory);      
            
            printf("checking password\n");

            ret = check_password(string_cred[0],string_cred[1]);            //check the password
            
            
            if (ret == 1){
                
                printf("password is correct\n");

                uid_format = (uid_t)strtoul(uid_utente, NULL, 10);
                gid_format = (gid_t)strtoul(gid_utente, NULL, 10);
                
                if(DEBUG)printf("Converted UID utente: %u\n", uid_format);
                if(DEBUG)printf("Converted GID utente: %u\n", gid_format);

                printf("logging in...\n");


                ret = change_uid(uid_format,gid_format);  //l'utente esiste, setta uid (già funzione)
                if (ret ==0){
                    printf("user correctly logged in \n");
                    ret = set_new_working_dir(home_directory);

                    if(DEBUG)printf("dovrei rimandare---------------------- %s\n",buf_succ);

                    send_msg(socket_desc,buf_succ,strlen(buf_succ),0); // rimanda msg al client 
                    
                    break;
                }
              
            
            
            }

        }

        /////////////////////////////////////////////////////////////////////////


        send_msg(socket_desc,buf_fail,strlen(buf_fail),0); //manda fail-msg al client 
       
    }

    if(DEBUG)printf("fuori dal loop di autenticazione\n");
    //closing semaphore
     if(sem_close(my_named_semaphore)){
            printf("Could not close the semaphore from child \n");
          

            }
 



    //  loop for command and send/receive file


    char** cmd_filename=(char**)malloc(2*sizeof(char*));
    char* comando=(char*)malloc(1024);
    char* filename_sent=(char*)malloc(1024);
    char* path_to_file=(char*)malloc(1024);
    char* path_to_file1=(char*)malloc(1024);
    memset(comando,0,1024);
    memset(path_to_file,0,1024);
    memset(filename_sent,0,1024);
    int r1;

   

    char* working_dir=(char*)malloc(1024);          
    strcpy(working_dir,home_directory);
    //strcat(working_dir,"/");

    


    if(DEBUG)printf("valore canary: %d\n",canary);
    if(canary==0){

    if(DEBUG)printf("il canary è stato settato: %d\n",canary);
    // set the new working dir and the proper uid and gid for the logged client

    set_new_working_dir(working_dir);   

    change_uid(uid_format,gid_format);    
    }

    while(1){           // problema costruzione path al file get/put

        if(canary){
            if(DEBUG)printf("uscita secondo loop\n");      // se l'utente ha già mandato QUIT devo uscire e chiudere la socket
            break;
        }                         

        memset(comando,0,strlen(comando));

        
        if(DEBUG)printf("dentro loop scambio info-----------------------------------   \n");

      

        memset(buf1, 0, buf_len);          //reset buffer to receive msg
        
        recv_bytes = 0;
        r1 = recv_msg(socket_desc,buf1,&recv_bytes);  //----------------------------------------------------   receive command

        buf1[recv_bytes] = '\0';  // Null-terminate the received command

        if(DEBUG)printf("comando ricevuto per intero: %s\n",buf1);                        

        //split string 

        r1 = split_cmd(buf1,comando,filename_sent);                         

        if(comando == NULL){
            printf("some problem in the received msg\n");
            break;
        }

        else{
        //printf("cmd == %s\n",comando);
        //printf("filename == %s\n",filename_sent);
    


            if (strcmp(comando, "get") == 0) {   //the file needs to be in the current dir of the user
                // Client wants to get a file
                printf("Client requested file: %s\n", filename_sent);

            

                // qui dovrei controllare prima se il file esiste
                int r5=check_file_existence(filename_sent);

                if(!r5){
                    printf("the requested file doesn't exist\n");
                    send_msg(socket_desc,buf_fail,strlen(buf_fail),0);
                }

                //////////////////////////////////////////
                else{

                    send_msg(socket_desc,buf_succ,strlen(buf_succ),0);

                    memset(path_to_file1,0,1024);

                    strcat(path_to_file1,working_dir);

                    if(DEBUG)printf("path con working dir: %s\n",path_to_file1);

                    strcat(path_to_file1,"/");

                    if(DEBUG)printf("path con slash: %s\n",path_to_file1);
                
                    strcat(path_to_file1,filename_sent);

                    if(DEBUG)printf("path con tot: %s\n",path_to_file1);


                    if(DEBUG)printf("dovrò mandare al client il file %s\n",path_to_file1);



                    // invio file al client 

                    sendFile(socket_desc,path_to_file1);

                }

            



        
            } else if (strcmp(comando, "put") == 0) {
                // Client wants to put a file
                printf("Client want to send file: %s\n", filename_sent);

    
                if(DEBUG)printf("my working dir: %s\n",working_dir);
                
                memset(path_to_file,0,1024);

                strcat(path_to_file,working_dir);         //crea percorso dove metterlo
                strcat(path_to_file,"/");
                strcat(path_to_file,filename_sent);

                if(DEBUG)printf("scriverò nel file = %s\n",path_to_file);

                //send_msg(socket_desc,buf_succ,strlen(buf_succ),0); // ack al client

                receiveFile(socket_desc,path_to_file);                     // riceve file del client 

                send_msg(socket_desc,buf_standard,strlen(buf_standard),0);

            


            } 
            else if (strcmp(comando, "ls") == 0) {      

            

                                                                                  

                // Client wants to put a file
                char result_ls[1024];
                char vuoto[]="empty!";
                size_t length = strlen(vuoto);
                vuoto[length-1] = '\n';
            
                memset(result_ls,0,1024);

                printf("Client want the file list\n");

                int r4=list_file(result_ls);             // based on the result of ls will send the file list or "empty"

                if(r4==1){
                    if(DEBUG)printf("send string empty\n");
                    send_msg(socket_desc,vuoto,length,0);
                }

                //printf("double check list: %s, bytes: %d\n",result_ls,strlen(result_ls));
                //printf("ritornato da fnc ls\n");
                // send result back
                else{

                send_msg(socket_desc,result_ls,strlen(result_ls),0);
                }
                if(DEBUG)printf(" end send message ls\n");
            

            }

            else if (strcmp(comando,"mkdir")== 0 || strcmp(comando,"rmdir")== 0){                                  

                    // implementa rimozione/creazione cartelle
                printf("client wants to operate on directory mkdir/rmdir");

              

                int r5=crea_delete_dir(comando,filename_sent);

                send_msg(socket_desc,buf_standard,strlen(buf_standard),0);

                



            }
            else{
                
                if (recv_bytes == quit_command_len && !memcmp(buf1, quit_command, quit_command_len)){
                printf("a client exited\n");                              
                break;
                }

                if (recv_bytes == shutdown_command_len && !memcmp(buf1, shutdown_command, shutdown_command_len)){             

                    printf("A client called the shutdown\n");

                    if (sem_post(my_named_semaphore)) {
                    printf("Could not unlock the semaphore from child\n");
                
                    }
                    else{
                        if(DEBUG)printf("agiornato valore?\n");
                        int c_v=0;
                        ret = sem_getvalue(named_semaphore, &c_v);

                        if (ret) {
                            if(DEBUG)printf("Could not access the named semaphore");
                            exit(1);
                        }

                        else{
                            if(DEBUG)printf("valore nel sem == %d\n",c_v);
                        }
                    }
                    if(sem_close(my_named_semaphore)){
                    printf("Could not close the semaphore from child \n");
                

                    }
                    break;
                    }

                    else{printf("not a well-known comman \n");}

            }
            


    }
      



    }




    if(DEBUG)printf("client socket closing\n");

    // free stuff


    free(uid_utente);
    free(gid_utente);
    free(home_directory);


    free(cmd_filename);
    free(comando);
    free(filename_sent);
    free(working_dir);


    // close socket
    ret = close(socket_desc);
    if (ret) handle_error("Cannot close socket for incoming connection");
}





void mprocServer(int server_desc) {
    int ret = 0;
    int current_value=0;
    int child_status;
 

    if(DEBUG){
        ret = sem_getvalue(named_semaphore, &current_value);

        if (ret) {
            printf("Could not access the named semaphore");
            exit(1);
        }

        else{
            if(DEBUG)printf("valore nel sem == %d\n",current_value);
        }
    }
    // we initialize client_addr to zero
    struct sockaddr_in client_addr = {0};

    // loop to manage incoming connections forking the server process
    int sockaddr_len = sizeof(struct sockaddr_in);
    while (1) {



        if(DEBUG)printf("----------------- login loop ------------------------\n");
        // accept incoming connection
        int client_desc = accept(server_desc, (struct sockaddr *) &client_addr, (socklen_t *)&sockaddr_len); //return desciptor per la connessione accettata
        // check for interruption by signals
        if (client_desc == -1 && errno == EINTR) continue; 
        if (client_desc < 0) handle_error("Cannot open socket for incoming connection");

        if (DEBUG) fprintf(stderr, "Incoming connection accepted...\n");
        total_proc=total_proc+1;


        /**           some details
         * - fork() is used to create a child process to handle the request
         * - close descriptors that are not used in the parent and the
         *   child process.
         * - connection_handler(client_desc, client_addr) is
         *   executed by the child process to handle the request
         * - memset(client_addr, 0, sizeof(struct sockaddr_in)) should
         *   be performed in order to be able to accept a new request
         * - a debug message in the parent process has been added to inform the
         *   user that a child process has been created
         **/

        pid_t pid = fork(); // pid =0 se siamo nel processo figlio, id figlio se siamo nel processo padre
        if (pid < 0) handle_error("Cannot fork server process to handle the request");

        else if (pid == 0) { // siamo nel proc figlio
            // child: close the listening socket and process the request
            ret = close(server_desc);
            if (ret) handle_error("Cannot close listening server socket in child process");
            printf("Child process to handle connection created\n");
            connection_handler(client_desc, &client_addr);                                           
            if (DEBUG) fprintf(stderr, "Process created to handle the request has completed.\n");
            _exit(EXIT_SUCCESS);
        } 
        else {
            num_figli+=1;    // update the number of child processes created
            // server: close the incoming socket and continue
            //printf("processo padre\n");
            ret = close(client_desc);
            if (ret) handle_error("Cannot close incoming socket in main process");
            if (DEBUG) fprintf(stderr, "Child process created to handle the request!\n");

            // reset fields in client_addr so it can be reused for the next accept()
            memset(&client_addr, 0, sizeof(struct sockaddr_in));

            
        }
    }

    //printf("USCITO DAL LOOP di wait for incoming connections\n");

}



int main(int argc, char* argv[]) {

    char* dir_name=(char*)malloc(1024);
    memset(dir_name,0,1024);

    // take argument from cmd line

     if (argc < 2) {
        if(DEBUG)printf("default path\n");
    }

    else{
        
        printf("Path for directories: %s\n", argv[1]);
        strcpy(dir_name,argv[1]);
        printf("but i'll ignore it cause the 'add user' command already handle this =)\n");
    }

    //set the handler to a sigchld signal
    signal(SIGCHLD, sigchld_handler);

    int ret;
    //int server_socket_desc;
    int current_value=0;

    // some fields are required to be filled with 0
    struct sockaddr_in server_addr = {0};
    // we will reuse it for accept()

    // create file with username,uid    
    ret = popola_username_uid();

    
    if(DEBUG){if(ret ==0)printf("file user written\n");}

    //---------------------------------------------- setting up semaphore


    sem_unlink(SEMAPHORE_NAME);
    sem_unlink(SEMAPHORE_FILE);
    named_semaphore = sem_open(SEMAPHORE_NAME, O_CREAT | O_EXCL, 0600, 0);

    if (named_semaphore == SEM_FAILED) {
        printf("Could not open the named semaphore");
    }

    named_sem_file= sem_open(SEMAPHORE_FILE, O_CREAT | O_EXCL, 0600, 1);
    if (named_sem_file == SEM_FAILED) {
        printf("Could not open the named semaphore1");
    }


    // initialize socket for listening
    server_socket_desc = socket(AF_INET , SOCK_STREAM , 0);                       // ho il socket descriptor
    if (server_socket_desc < 0) handle_error("Could not create socket");

    server_addr.sin_addr.s_addr = INADDR_ANY; // accept connections from any interface
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(SERVER_PORT); //  network byte order


    /* SO_REUSEADDR to quickly restart our server after a crash:*/
    int reuseaddr_opt = 1;
    ret = setsockopt(server_socket_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
    if (ret) handle_error("Cannot set SO_REUSEADDR option");

    // bind address to socket
    ret = bind(server_socket_desc, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
    if (ret) handle_error("Cannot bind address to socket");

    // start listening
    ret = listen(server_socket_desc, MAX_CONN_QUEUE);              //mark the socket dfined as passive
    if (ret) handle_error("Cannot listen on socket");

    printf("Listening on socket...\n");


    mprocServer(server_socket_desc);


   
    //cleanup();                        |  called in sigchld_handler
    //close(server_socket_desc);        |  called in sigchld_handler
    exit(EXIT_SUCCESS);
}



