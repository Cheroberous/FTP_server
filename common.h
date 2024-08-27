

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
#define FILE_PWD "name_uid"
#define MAX_LINE_LENGTH 100
#define BUFFER_SIZE 1024
#define SEMAPHORE_NAME      "/pleaseshutdown"
#define SEMAPHORE_FILE      "/forfile1"
#include <pwd.h>
#include <shadow.h>




// rem user :    sudo userdel -r username  (usefull to clean after testing)

//check if the requested file exists in the account directory

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


//used to handle the mkdir/rmdir commands, work using execvp

int crea_delete_dir(char* comando,char* nome_dir){

    char* args[] = {comando, nome_dir, NULL};


    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    } else if (pid == 0) {

        if (execvp(comando, args) == -1) {
            perror("Error executing command");
            exit(EXIT_FAILURE);
        }

    }





    return 0;

}


//the popen function is used to create a pipe and establish a connection between the calling program and the ls command.
//It allows the calling program to either read from or write to the input or output stream of the executed command.


int list_file(char* buf_res){   

   

    FILE* commandOutput = popen("ls", "r");
    if (commandOutput == NULL) {
        perror("Error opening pipe");
        exit(EXIT_FAILURE);
    }


    //char* buffer = malloc(1024);
    char* buffer = NULL;
    //size_t buffer_size = 1024;
    size_t buffer_size = 0;
    char line[1024];
    int r=0;
    int canary=0;



    // Read the output of the command into the dynamically allocated buffer
    while (fgets(line, sizeof(line), commandOutput) != NULL) {
        size_t line_length = strlen(line);

        if(DEBUG)printf("trovata roba di %ld bytes\n",line_length);
        if(canary!=1)canary=1;
        // Resize the buffer to accommodate the new line
        buffer = realloc(buffer, buffer_size + line_length + 1);
        if (buffer == NULL) {
            perror("Error allocating memory");
            exit(EXIT_FAILURE);
        }

        // Append the line to the buffer
        strcpy(buffer + buffer_size, line);
        buffer_size += line_length;
    }

 
    if(DEBUG)printf("canary == %d\n",canary);

    // Close the pipe
    if (pclose(commandOutput) == -1) {
        perror("Error closing pipe");
        exit(EXIT_FAILURE);
    }

    // if the canary has not beeing set the list is empty
    if(canary==0){
        if(DEBUG)printf("no elementi\n");
        return 1;
    }

    else{

        //clean the result from every \n

        //printf("risultato tot %s\n",buffer);
        for(int i=0;i<strlen(buffer);i++){

            if(buffer[i]=='\n'){
                buffer[i]=' ';
            }
        }
        buffer[strlen(buffer)-1]='\n';      // set the las tcharacter as \n (used to send the string)

        if(DEBUG)printf("risultato updated %s\n",buffer);

        strcpy(buf_res,buffer);
    }


    return 0;





}


// send the message to the client, handle possible interrupt checking what the send return

int send_msg(int socket_fd,char* buf,int m_len,int sent){
    int mandati=sent;
    int ret=0;
    while ( mandati < m_len) {
        ret = send(socket_fd, buf + mandati, m_len - mandati, 0);
        if (ret == -1 && errno == EINTR) continue;
        if (ret == -1) handle_error("Cannot write to the socket");
        mandati += ret;
    }
    if(DEBUG)printf("bytes mandati == %d\n",mandati);
    //printf("fine send message\n");
    return 0;
}

//receive the msg from the client reading until it finds a \n, handle possible interrupt signals

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

    if(DEBUG)printf("ricevuti %d\n",ricevuti);

    //buf[ricevuti]='\0';


    return  ret;

}



//send file to the client

void sendFile(int clientSocket, char* fileName) {


    if(DEBUG)printf("sending file %s\n",fileName);


    FILE* file = fopen(fileName, "rb");
    if (file == NULL) {
        perror("Error opening file for reading");
        return;
    }
    else{if(DEBUG)printf("file aperto correttamente\n");}

    char buffer[1024];
    size_t bytesRead=0;


    //non legge correttamente da "file_invio.txt"

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        int r=send(clientSocket, buffer, bytesRead, 0);
        if(DEBUG)printf("inviati %d bytes \n",r);
        if(DEBUG)printf("letti %ld bytes dal file\n",bytesRead);
    }

    if(DEBUG)printf("letti %ld bytes dal file\n",bytesRead);
    //printf("fine invio file \n");

    fclose(file);
    return ;
}


//receive the file from the client

void receiveFile(int clientSocket,char* path_fin) {

    if(DEBUG)printf("receiving the file\n");


    char* path_filename=malloc(1024);
    memset(path_filename,0,1024);

    //strcat(path_filename,w_d);
    //strcat(path_filename,filename);


    if(DEBUG)printf("the file will be put in %s\n",path_fin);

    FILE* file = fopen(path_fin, "wb");
    if (file == NULL) {
        perror("Error opening file for writing");
        return;
    }

    char buffer[1000];
    ssize_t bytesRead;

    //read content from the socket

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

        if(bytesRead<1000){                           // if the recv hasn't fill the buffer we exit
            break;
        }

        //printf("uscita forzata\n");
        //break;                                     //forzando l'uscita lo scrive ma dovrebbe farlo quando non legge più nulla dalla socket
        memset(buffer,0,1000);
       
    }
    

    fclose(file);
    free(path_filename);

    if(DEBUG)printf("fine ricezione file\n");

  
}






//change the current dir calling chdir (getcwd is used to debug purpose)

int set_new_working_dir(char* new_working_dir){

    char currentDirectory[1024];

    if (getcwd(currentDirectory, sizeof(currentDirectory)) != NULL) {
        //printf("Current working directory: %s\n", currentDirectory);
    } else {
        perror("getcwd() error");
        return 1;  // Exit with an error code
    }
    
    if (chdir(new_working_dir) == 0) {
        if(DEBUG)printf("Changed working directory to: %s\n", new_working_dir);
    } 
    else{
        perror("chdir error");
        return 1;  // Exit with an error code
    }

    // Get the new current working directory
    if (getcwd(currentDirectory, sizeof(currentDirectory)) != NULL) {
        //printf("Current working directory: %s\n", currentDirectory);
    } else {
        perror("getcwd() error");
        return 1;  // Exit with an error code
    }

    return 0;
}

//just a split function for the username,passwor string

int split_cred(char* string,char** list_cred){

    char *token = strtok(string, ",");

    // Iterate through the tokens
    int i =0;
    while (token != NULL) {
        // Print or process each token
        //printf("Token: %s\n", token);
        list_cred[i] = token;
        
        

        // Get the next token
        token = strtok(NULL, ",");
        i++;
    }
  
    return 0;
}


// split the comman-filename pair

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

// check if an account with that username exists

int check_existence(char* u,char* uid_ret,char* h_dir,char* gid_ret){


    int ritorna = 0;
    FILE *file = fopen(FILE_PWD, "r");

    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LENGTH];

    while (fgets(line, sizeof(line), file) != NULL) {
        // Remove newline character from the end of the line
        line[strcspn(line, "\n")] = '\0';

        // Use strtok to tokenize the line based on ","
        char *username = strtok(line, ",");
        char *uid = strtok(NULL, ",");
        char *gid = strtok(NULL, ",");
        char *h = strtok(NULL, ";");

        int result = strcmp(u, username);
        if (result==0){
            strcpy(uid_ret, uid);
            strcpy(h_dir, h);
            strcpy(gid_ret, gid);
            uid_ret = uid;
            if(DEBUG)printf("trovato info per utente == %s\n",u);
            //ritorna = 2;
            //int ris = strcmp(p,password);
            //if (ris ==0){printf("esiste\n");ritorna = 1;break;}
            //printf("esiste \n");
            return 1;
            

            }
    }

    
    

    fclose(file);
    return ritorna;
}


// return 1 if the password is correct
int check_password(char* username,char* clear_password){


    struct passwd *pwd;

    pwd = getpwnam(username);


    // if pwd == null --> user don't exist

    if (pwd != NULL) {

        // left for debug purpose

        //printf("User: %s\n", pwd->pw_name);
        //printf("User ID: %d\n", pwd->pw_uid);   //comodo
        //uid = pwd->pw_uid;
        //printf("Group ID: %d\n", pwd->pw_gid);
        //printf("Home directory: %s\n", pwd->pw_dir);
        //char *hashed_password = pwd->pw_passwd;
        //char *salt = strstr(hashed_password, "$");

        struct spwd* shadowEntry = getspnam(username);           // populate the structure with user related info
        if (!shadowEntry)
        {
            printf( "Failed to read shadow entry for user '%s'\n", username );
            return 1;
        }

        int ris=strcmp( shadowEntry->sp_pwdp, crypt( clear_password, shadowEntry->sp_pwdp ) );  // check the provided password against the correct one
        if(ris ==0){
            //printf("ris == %d\n",ris); printf("password is correct\n");
            return 1;
        }
        else{if(DEBUG)printf("password incorrect\n");return 0;}
    }

    
         
    else {
        printf("User not found.\n");
        return 1;
    }

}


int crea_utente(char* nome,char*password){

    // create new user and set password   


    char inizio[100]="useradd -m ";
    strcat(inizio,nome);
    strcat(inizio," && ");



    // build the string to be used as argument of /bin/sh

    char string_for_p[100]="echo '";

    strcat(inizio,string_for_p);

    char*end_for_p="' | sudo chpasswd";
    strcat(inizio,nome);
    strcat(inizio,":");
    strcat(inizio,password);
    strcat(inizio,end_for_p);

    if(DEBUG)printf(" stringa comd %s\n",inizio);


    char *args[] = { "/bin/sh", "-c", inizio, NULL };

    // Use fork to create a child process
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    } else if (pid == 0) {
        // Child process: execute useradd
       

    // Execute the command using execvp
        if (execvp(args[0], args) == -1) {
            perror("execvp");
            return 1;
        }

        else{if(DEBUG)printf("utente creato con successo");return 0;}
       
    }
    return 0;
  
}



//create a file "name_uid" that populates with user information taken from /etc/passwd

int popola_username_uid(){         

    //list user-uid in /etc/passwd

    char line[256];
    int c=0; 
    char tmp[1024];

    /*char* nome;
    char* uid_str;
    char* gid;
    char* home_dir;*/


    // open etc/passwd in reading mode

    FILE *passwdfile = fopen("/etc/passwd", "r");   //-----------------  non toccare

    if (passwdfile == NULL) {
        perror("fopen");
        return 1;
    }


    char *filename = "name_uid";


    FILE *file = fopen(filename, "w");          // create a new file named "name_uid" 

    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }


                                     

    struct passwd *pw_entry;

    while ((pw_entry = fgetpwent(passwdfile)) != NULL) {

        memset(tmp,0,1024);
    
        snprintf(tmp, sizeof(tmp), "%s,%d,%d,%s;",
                 pw_entry->pw_name, pw_entry->pw_uid, pw_entry->pw_gid, pw_entry->pw_dir);



        int r=fprintf(file, "%s\n", tmp);
       


    }

    // Close files descriptor
    fclose(file);
    fclose(passwdfile);
    //printf("fatto ?\n");
    return 0;


}


// NON USED (alternative to the previous one, works if using the first username to correctly parse the /etc/passwd file)

int popola_u_id_alternative(){

    // funziona definendo il primo user effettivo del server


    char line[256];
    int c=0;
    char* tmp=malloc(40);   
    char* qualcosa;
    char* uid_str;
    char* gid;
    char* home_dir;


    FILE *passwdfile = fopen("/etc/passwd", "r");   // non toccare

    if (passwdfile == NULL) {
        perror("fopen");
        return 1;
    }

    // Open the file in append mode (no)
    const char *filename = "name_uid";


    FILE *file = fopen(filename, "w");

    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }




    char *username = strtok(line, ":");
    memset(tmp,0,40);



 
    if(strcmp(username,"chero")==0){  
        printf("da chero in giu \n");                
        c=1;
    }

        
    qualcosa = strtok(NULL, ":");
    uid_str = strtok(NULL, ":");
    gid = strtok(NULL, ":");
    home_dir = strtok(NULL, ":");
           


    if(c==1){
        printf("username: %s, uid: %s\n",username,uid_str);
        printf("gid: %s \n",gid);
        printf("home dir: %s\n",home_dir);
    }


            if(strncmp(home_dir,"/home",5)==0){

            //char *q3 = strtok(NULL, ":");
            //char *q4 = strtok(NULL, ":");
 


            // write into file

       
            // Concatenate str2 to the end of str1
            strcat(tmp, username);
            strcat(tmp,",");
            strcat(tmp, uid_str);
            strcat(tmp,",");
            strcat(tmp, home_dir);
            strcat(tmp,",");
            strcat(tmp, gid);
            strcat(tmp,";");

            printf("stringa : %s\n",tmp);

           

            
            // Write the string to the file
            fprintf(file, "%s\n", tmp);

            }

            



            //conversione uid (richieta per setuid)
            //uid_t uid = (uid_t)strtoul(uid_str, NULL, 10);
            //printf("Converted UID: %d\n", uid);
        
    

    // Close the file (handle error)
    fclose(file);
    fclose(passwdfile);
    return 0;
}



//change the child process uid and gid with the one of the autenticated user (proper sanity check are performed)

int change_uid(uid_t new_uid,gid_t new_gid){
    FILE *output_file;


    // Buffer to store the output
    char buffer[1024];


    output_file = popen("whoami", "r");
    if (output_file == NULL) {
        perror("popen");
        return 1;
    }

    // Read the output into the buffer
    fgets(buffer, sizeof(buffer), output_file);

    // Close the file
    pclose(output_file);


    // Print or process the captured output
    //printf("Output of whoami: %s", buffer);



    memset(buffer,0,1024);


    //printf("UID : %d\n", getuid());

     if (setgid(new_gid) == 0) {
        //printf("Group ID set successfully.\n");
    } else {
        perror("Error setting group ID");
    }

    
  

    // Attempt to change the UID
    if (setuid(new_uid) == -1) {
        perror("setuid");
        return 1;
    }

    // Check if the UID was successfully changed
    if(DEBUG)printf("Changed UID to: %d\n", getuid());

   




    // Use popen to run the "whoami" command and read the output into the buffer   (debug purpose)
    output_file = popen("whoami", "r");
    if (output_file == NULL) {
        perror("popen");
        return 1;
    }

    // Read the output into the buffer
    fgets(buffer, sizeof(buffer), output_file);

    // Close the file
    pclose(output_file);

    // Print or process the captured output
    if(DEBUG)printf("Output of whoami: %s", buffer);
    return 0;
}




