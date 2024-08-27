#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>  // htons() and inet_addr()
#include <netinet/in.h> // struct sockaddr_in
#include <sys/socket.h>
#include <ctype.h>
#include <signal.h>
#include "common_c.h"



int socket_desc;                        // needed here to be available also in the handle function


void handleCtrlC(int signum) {
    printf("\nTrying to escape from me?...please next time use QUIT!!\n");

    //send to the server that we want to quit

    char* exit_c="QUIT\n";

    send_msg(socket_desc,exit_c,strlen(exit_c),0);

  
    exit(EXIT_SUCCESS);
}


int main(int argc, char* argv[]) {


    if (signal(SIGINT, handleCtrlC) == SIG_ERR) {
        perror("Error setting up signal handler");
        return EXIT_FAILURE;
    }


    // take server name as input from cmd

    char* server_name=(char*)malloc(1024);
    char* server_name_b=(char*)malloc(1024);
    memset(server_name,0,1024);

    if (argc < 2) {
        printf("So..i'll assume localhost?\n");
        server_name="127.0.0.100";
    }

    else{
        
        if(DEBUG)printf("Command-line argument: %s\n", argv[1]);
        strcpy(server_name,argv[1]);
        if (strncmp(server_name, "127.0.0.1", 9) != 0) {
        
            if (isdigit(server_name[0])) {
                printf("private ip addresses\n");
                // do nothing
            } 
            else{
                printf("custom dns\n");
                server_name_b=dns_resol(server_name);   //resolve the domain name
         
                if(server_name == NULL){
                    printf("error retriving dns address\n");
                    exit(0);
                }
                printf("ip address solved %s", server_name_b);
            }
        }
        else{
            printf("no-where is like home\n");
            server_name="127.0.0.100";

        }

    }

    int ret,bytes_sent,recv_bytes;

    // variables for handling a socket
    //int socket_desc;
    struct sockaddr_in server_addr = {0}; // some fields are required to be filled with 0

    // create a socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_desc < 0) handle_error("Could not create socket");

    // set up parameters for the connection
    server_addr.sin_addr.s_addr = inet_addr(server_name);   //SERVER_ADDRESS
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(SERVER_PORT); // don't forget about network byte order!

    // initiate a connection on the socket
    ret = connect(socket_desc, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
    if(ret) handle_error("Could not create connection");

    if (DEBUG) fprintf(stderr, "Connection established!\n");

    char buf[1024];
    char buf_tmp[1024];
    size_t buf_len = sizeof(buf);
    int msg_len;
    memset(buf, 0, buf_len);
    int canary=0;
    char* quit_command = SERVER_COMMAND;
    size_t quit_command_len = strlen(quit_command);
    char* shutdown_command = SERVER_SHUTDOWN;
    size_t shutdown_command_len = strlen(shutdown_command);



    // display welcome message from server  (here just to display the logic, receive and send functions are in the common_c.h file)
    recv_bytes = 0;
    do {
        ret = recv(socket_desc, buf + recv_bytes, buf_len - recv_bytes, 0);
        if (ret == -1 && errno == EINTR) continue;
        if (ret == -1) handle_error("Cannot read from the socket");
        if (ret == 0) break;
	   recv_bytes += ret;
    } while ( buf[recv_bytes-1] != '\n' );
    printf("%s", buf);

    // main loop


    // while loop per autenticazione


    char credenziali[1024];
    int r3=0;


    while(1){
        if(DEBUG)printf("authentication loop\n");
        memset(credenziali,0,1024);
        memset(buf,0,1024);
        memset(buf_tmp,0,1024);

        printf("\nYour credentials: ");


        if (fgets(buf, sizeof(buf), stdin) != (char*)buf) {
            fprintf(stderr, "Error while reading from stdin, exiting...\n");
            exit(EXIT_FAILURE);
        }


        msg_len = strlen(buf);
        strcpy(buf_tmp,buf);
        buf[strlen(buf) - 1] = '\n'; // place '\n' from the end of the message




        if (msg_len == quit_command_len && !memcmp(buf, quit_command, quit_command_len)){
            printf("the user want to quit\n");
            send_msg(socket_desc,buf,strlen(buf),0);
            canary=1;
            break;
        } 

        if (msg_len == shutdown_command_len && !memcmp(buf, shutdown_command, shutdown_command_len)){
            printf("server shutdown sent, goodbye\n");
            send_msg(socket_desc,buf,strlen(buf),0);
            canary=1;
            break;
        } 


        if(strncmp(buf_tmp,"root",4)==0){
            printf("no root access allowed, sorry\n");  // anche lato server
        }

        else{

            //printf("dovrei mandare messaggio\n");

            send_msg(socket_desc,buf,strlen(buf),0);

            memset(buf, 0, buf_len);

            int received2=0;
            
            memset(buf,0,1024);
            r3=recv_msg(socket_desc,buf,&received2);
            //printf("bytes ricevuti == %d\n",received);

            
        

            if (strncmp(buf,"ok", 2) == 0) {
                //printf("ricevuto: %s\n",buf);
                printf("authentication success\n");
                break;
            }

            else{
                //printf("ricevuto: %s \n",buf);
                printf("authentication error, retry\n");
                }

        }


    }



    char comando[4];
    char filename[1024];
    char curr_dir[2048];
    char curr_dir1[2048];
    char* received_string=malloc(1024);

    printf("you can use the following commands: put/get filename, ls, mkdir/rmdir dir_name \n");
    while (1) {

        if(canary)break;
        if(DEBUG)printf("in attesa di nuovo comando --------------------------------------\n");
        memset(buf,0,1024);
        memset(curr_dir,0,2048);
        memset(filename,0,1024);
        memset(buf_tmp,0,1024);
        memset(received_string,0,1024);

     
        int received=0;

        printf("Your message: ");

        /* Read a line from stdin
         *
         * fgets() reads up to sizeof(buf)-1 bytes and on success
         * returns the first argument passed to it. */
        if (fgets(buf, sizeof(buf), stdin) != (char*)buf) {
            fprintf(stderr, "Error while reading from stdin, exiting...\n");
            exit(EXIT_FAILURE);
        }

        msg_len = strlen(buf);
        strcpy(buf_tmp,buf);
        buf[strlen(buf) - 1] = '\n'; // place '\n' from the end of the message

        



       
        //printf("il client ha mandato %s\n",buf);   
        
        // send message to server                                                        

        //send_msg(socket_desc,buf,strlen(buf),0);                       // di base lui fa sempre la send msg


        

        // dato contenuto massaggio mandato si decide il da farsi     // check message type

        //printf("il client ha mandato %s\n",buf_tmp);   

        if (strncmp(buf_tmp, "put", 3) == 0) {
            if(DEBUG)printf("The first three characters are 'put'\n");


            // get file path and command

            r3=split_cmd(buf_tmp,comando,filename);

            if(DEBUG)printf("comando == %s\n",comando);
            if(DEBUG)printf("file da trasferire == %s\n",filename);

            r3=check_file_existence(filename);                       // verify that the file we want to sent exist (check for typing errors)

            if(r3){

                send_msg(socket_desc,buf,strlen(buf),0);                       //send client command to the server


                if(DEBUG)printf("the file exist, we can send it\n");
            

                //////////////////////////////////////////////////////   concatenate file with current directory

                r3= get_dir(curr_dir);

                //printf("current dir == %s\n",curr_dir);

                strcat(curr_dir,"/");
                strcat(curr_dir,filename);

                if(DEBUG)printf("path to file == %s\n",curr_dir);       
                //////////////////////////////////////////////////////

                //qui devo mandare il file
                sendFile(socket_desc,curr_dir);   // curr_dir ora contiene path to file

                if(DEBUG)printf("ritornato da funzione send file \n");

                // qui riceve
                received=0;
                memset(buf, 0, buf_len);
                r3=recv_msg(socket_desc,buf,&received);
                //printf("bytes ricevuti == %d\n",received);

                printf("response from server: %s\n",buf);
            }

            else{printf("please select an existent file\n");}
        } 
        
        
        else if (strncmp(buf_tmp, "get", 3) == 0) {

            send_msg(socket_desc,buf,strlen(buf),0);                       //send client command to the server

            // qui ricevo ack dal server

            memset(buf, 0, buf_len);
            r3=recv_msg(socket_desc,buf,&received);
            //printf("response from server == %s\n",buf);

            if(strncmp(buf,"ok",2)!=0){
                printf("the requested file doesn't exists\n");
            }

            else{

                if(DEBUG)printf("the requested file exists on the server\n");
            
                if(DEBUG)printf("The first three characters are 'get'\n");

                r3=split_cmd(buf_tmp,comando,filename);

                if(DEBUG)printf("comando == %s\n",comando);
                if(DEBUG)printf("file da trasferire == %s\n",filename);

                r3= get_dir(curr_dir1);

                if(DEBUG)printf("current dir == %s\n",curr_dir1);

                strcat(curr_dir1,"/");
                strcat(curr_dir1,filename);

                if(DEBUG)printf("path dove ricevere file == %s\n",curr_dir1);     


                // devo ricevere il file che ho richiesto (il nome è nel mio comando)

                receiveFile(socket_desc,curr_dir1);

            }



        }

        else if (strncmp(buf_tmp, "ls", 2) == 0) {

  
            send_msg(socket_desc,buf,strlen(buf),0);                       //send client command to the server
            
            int bytes_r=0;
            //printf("sono in 'ls' ricezione \n");
            
            recv_msg(socket_desc,received_string,&bytes_r);

            printf("msg from server: %s\n",received_string);

        }

        else if(strncmp(buf_tmp, "mkdir",5)==0){

            send_msg(socket_desc,buf,strlen(buf),0);                       //send client command to the server

            printf("entrato in mkdir\n");
            
            int bytes_r1=0;

            //receive ack
            recv_msg(socket_desc,received_string,&bytes_r1);
            printf("msg from server: %s\n",received_string);


        }

        else if (strncmp(buf_tmp,"rmdir",5)==0){

            send_msg(socket_desc,buf,strlen(buf),0);                       //send client command to the server

        
            int bytes_r2=0;

            //receive ack
            recv_msg(socket_desc,received_string,&bytes_r2);
            printf("msg from server: %s\n",received_string);

        }
        
        
        
        else {

            send_msg(socket_desc,buf,strlen(buf),0);                       //send client command to the server

            //printf("analizando messaggio %s",buf_tmp);
   
            //qui devo controllare se ho mandato QUIT

            //check for quit
            /* After a quit command we won't receive any more data from
            * the server, thus we must exit the main loop. */
            if (msg_len == quit_command_len && !memcmp(buf, quit_command, quit_command_len)){printf("client want to quit, bye\n");break;}
            if (msg_len == shutdown_command_len && !memcmp(buf, shutdown_command, shutdown_command_len)){
                printf("server shutdown sent, goodbye\n");
                break;
            } 

            else{printf("you sent an invalid command\n");}

     
        }



    }



    // close the socket
    ret = close(socket_desc);
    if(ret) handle_error("Cannot close socket");

    if (DEBUG) fprintf(stderr, "Socked has been closed,Exiting...\n");

    exit(EXIT_SUCCESS);
}
