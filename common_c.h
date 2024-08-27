

// macros for handling errors
#define handle_error_en(en, msg)    do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)
#define handle_error(msg)           do { perror(msg); exit(EXIT_FAILURE); } while (0)

/* Configuration parameters */
#define DEBUG           0   // display debug messages
#define MAX_CONN_QUEUE  3   // max number of connections the server can queue
#define SERVER_ADDRESS  "127.0.0.1"
#define SERVER_COMMAND  "QUIT\n"
#define SERVER_SHUTDOWN  "SD\n"
#define SERVER_PORT     2017
#define Quit_len        5
#define MAX_LINE_LENGTH 100




#include <netdb.h>


int check_file_existence(char* filename){

  

    if (access(filename, F_OK) == 0) {
        if(DEBUG)printf("File '%s' exists in the current directory.\n", filename);
        return 1;
    } else {
        if(DEBUG)printf("File '%s' does not exist in the current directory.\n", filename);
        return 0;
    }

    return 1;
}

// used to perform dns resolution (from domain name to ipv4 address)
char* dns_resol(char* domainname){       


    printf("requested domain %s\n",domainname);
    //char* d="example.com";

    struct hostent *host_info;

    host_info = gethostbyname(domainname);
    if (host_info == NULL) {
        herror("gethostbyname");
        exit(EXIT_FAILURE);
    }
    if (host_info->h_addrtype == AF_INET) {
        struct in_addr **addr_list = (struct in_addr **)host_info->h_addr_list;
        for (int i = 0; addr_list[i] != NULL; i++) {
            //printf("Resolved IP address: %s\n", inet_ntoa(*addr_list[i]));
            char* resolved=inet_ntoa(*addr_list[i]);

            return resolved;
            //strcpy(domainname,inet_ntoa(*addr_list[i]));
            printf("Resolved IP address: %s\n", resolved);

        }
    } else if (host_info->h_addrtype == AF_INET6) {
        // Handle IPv6 addresses if needed
        // Note: This example assumes IPv4; additional logic is required for IPv6
        printf("IPv6 address not handled, SORRY!!\n");
    }
    return NULL;

   
}


//described in the server common.h file

void receiveFile(int clientSocket,char* path_fin) {

    char buffer[2048];

    if(DEBUG)printf("sto ricevendo il file\n");


    char* path_filename=malloc(1024);
    memset(path_filename,0,1024);

    //strcat(path_filename,w_d);
    //strcat(path_filename,filename);


    if(DEBUG)printf("il file verrà messo in %s\n",path_fin);

    FILE* file = fopen(path_fin, "wb");
    if (file == NULL) {
        perror("Error opening file for writing");
        return;
    }
    else{
        if(DEBUG)printf("file descriptor buono\n");
    }

 
    ssize_t bytesRead=0;

  

    while ((bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
        int ris= fwrite(buffer, 1, bytesRead, file);
        if(DEBUG)printf("bytes ricevuti == %ld\n",bytesRead);
        if (ris != bytesRead) {
            perror("Error writing to file");
            fclose(file);
            return;
        }
        if (bytesRead < 0) {
            perror("Error receiving data");
        }

        if(DEBUG)printf("scritti %d bytes\n",ris);
        if(bytesRead<2048){
            //printf("finito di ricevere\n");
            break;
        }

                          
        memset(buffer,0,2048);
       
    }
    

    fclose(file);
    free(path_filename);

    if(DEBUG)printf("fine ricezione file\n");

  
}



//described in the server common.h file

void sendFile(int clientSocket, const char* fileName) {


    if(DEBUG)printf("sto mandando file\n");
    FILE* file = fopen(fileName, "rb");
    if (file == NULL) {
        perror("Error opening file for reading");
        return;
    }

    char buffer[1024];
    size_t bytesRead;

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        int r=send(clientSocket, buffer, bytesRead, 0);
        if(DEBUG)printf("iniviati %d bytes \n",r);
    }

    if(DEBUG)printf("fine invio file \n");

    fclose(file);
    return;
}



//described in the server common.h file


int send_msg(int socket_fd,char* buf,int m_len,int sent){
    //printf("sto mandando un messaggio\n");
    int mandati=sent;
    //printf("messaggio: %s\n",buf);
    int ret=0;
    while ( mandati < m_len) {
        ret = send(socket_fd, buf + mandati, m_len - mandati, 0);
        if (ret == -1 && errno == EINTR) continue;
        if (ret == -1) handle_error("Cannot write to the socket");
        mandati += ret;
    }
    //printf("inviati %d bytes \n",mandati);
    return 0;
}


//described in the server common.h file

int recv_msg(int socket_fd,char* buf,int* received){
    int ricevuti=0;
    int ret=0;
     do {
        ret = recv(socket_fd, buf + ricevuti, 1, 0);
        if (ret == -1 && errno == EINTR) continue;
        if (ret == -1) handle_error("Cannot read from the socket");
        if (ret == 0) break;
	} while ( buf[ricevuti++] != '\n' );

    *received = ricevuti;

    if(DEBUG)printf("bytes ricevuti == %d\n",ricevuti);

    //buf[ricevuti]='\0';


    return  ret;

}


//split the "command filename" pair

int split_cmd(char* input,char* command_sent,char* file){
    
    
    // Using sscanf to split the string
    char command[6], filename[1024];
    sscanf(input, "%s %s", command, filename);

    //printf("Command: %s\n", command);
    //printf("FileName: %s\n", filename);

    strcpy(command_sent,command);
    strcpy(file,filename);


    return 0;


}

//retrve current working directory (debug purpose)

int get_dir(char* current_dir) {
 
    char tmp[2048];

    // Get the current working directory
    if (getcwd(tmp, sizeof(tmp)) != NULL) {
        if(DEBUG)printf("Current working directory: %s\n", tmp);
        strcpy(current_dir,tmp);
    } else {
        perror("getcwd() error");
        return 1;  // Exit with an error code
    }

    return 0;
}






   