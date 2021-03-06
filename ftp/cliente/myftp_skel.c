#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <err.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 512

/**
 * function: si hay error, se imprime mensaje y se corta el programa
 **/
void exitwithmsg(char * msg){
    printf("%s\n", msg);
    exit(-1);
}
/**
 * function: receive and analize the answer from the server
 * sd: socket descriptor
 * code: three leter numerical code to check if received
 * text: normally NULL but if a pointer if received as parameter
 *       then a copy of the optional message from the response
 *       is copied
 * return: result of code checking
 **/
bool recv_msg(int sd, int code, char *text) {
    char buffer[BUFSIZE], message[BUFSIZE];
    int recv_s, recv_code;

    // receive the answer
    recv_s = recv(sd, buffer, BUFSIZE, 0);

    // error checking
    if (recv_s < 0) warn("error receiving data");
    if (recv_s == 0) errx(1, "connection closed by host");

    // parsing the code and message receive from the answer
    sscanf(buffer, "%d %[^\r\n]\r\n", &recv_code, message);
    printf("%d %s\n", recv_code, message);
    // optional copy of parameters
    if(text) strcpy(text, message);
    // boolean test for the code
    return (code == recv_code) ? true : false;
}

/**
 * function: send command formated to the server
 * sd: socket descriptor
 * operation: four letters command
 * param: command parameters
 **/
void send_msg(int sd, char *operation, char *param) {
    char buffer[BUFSIZE] = "";

    // command formating
    if (param != NULL)
        sprintf(buffer, "%s %s\r\n", operation, param);
    else
        sprintf(buffer, "%s\r\n", operation);

    // send command and check for errors
    if((send(sd, buffer, BUFSIZE, 0)) < 0){
        exitwithmsg("Error en el envio de mensaje");
    }

}

/**
 * function: simple input from keyboard
 * return: input without ENTER key
 **/
char * read_input() {
    char *input = malloc(BUFSIZE);
    if (fgets(input, BUFSIZE, stdin)) {
        return strtok(input, "\n");
    }
    return NULL;
}

/**
 * function: login process from the client side
 * sd: socket descriptor
 **/
void authenticate(int sd) {
    char *input, desc[100];
    int code;

    sleep(1);

    // ask for user
    printf("Usuario: ");
    input = read_input();

    // send the command to the server
    send_msg(sd, "USER", input);

    sleep(1);
    
    // relese memory
    free(input);

    // wait to receive password requirement and check for errors
    recv_msg(sd, 331, desc);

    // ask for password
    printf("Contraseña: ");
    input = read_input();

    // send the command to the server
    send_msg(sd, "PASS", input);

    // release memory
    free(input);

    // wait for answer and process it and check for errors
    if(!recv_msg(sd, 230, desc)){
       exitwithmsg("Error en Autenticacion!");
    }
}

/**
 * function: operation get
 * sd: socket descriptor
 * file_name: file name to get from the server
 **/
void get(int sd, char *file_name) {
    char desc[BUFSIZE], buffer[BUFSIZE];
    int f_size, recv_s, r_size = BUFSIZE;
    FILE *file;

    // send the RETR command to the server
    send_msg(sd, "RETR", file_name);

    // check for the response
    if(recv_msg(sd, 550, NULL) == true) return;

    // parsing the file size from the answer received
    // "File %s size %ld bytes"
    sscanf(buffer, "File %*s size %d bytes", &f_size);

    // open the file to write
    file = fopen(file_name, "wb");

    //receive the file
    while(1){
        recv_s = recv(sd, buffer, r_size, 0);
                
        fwrite(buffer, 1, recv_s, file);

        if (recv_s < r_size){
            // close the file
            fflush(file);
            fclose(file);
            break;
        }
    }

    // receive the OK from the server
    recv_msg(sd, 226, NULL);
}

/**
 * function: operation quit
 * sd: socket descriptor
 **/
void quit(int sd) {
    // send command QUIT to the client
    send_msg(sd, "QUIT", NULL);

    // receive the answer from the server
    recv_msg(sd, 221, NULL);
}

/**
 * function: make all operations (get|quit)
 * sd: socket descriptor
 **/
void operate(int sd) {
    char *input, *op, *param;

    while (true) {
        printf("Operation: ");
        input = read_input();
        if (input == NULL)
            continue; // avoid empty input
        op = strtok(input, " ");
        if (strcmp(op, "get") == 0) {
            param = strtok(NULL, " ");
            get(sd, param);
        }
        else if (strcmp(op, "quit") == 0) {
            quit(sd);
            break;
        }
        else {
            // new operations in the future
            printf("TODO: unexpected command\n");
        }
        free(input);
    }
    free(input);
}

/**
 * Run with
 *         ./myftp <SERVER_IP> <SERVER_PORT>
 **/
int main (int argc, char *argv[]) {
    if(argc<2){
        printf("<host> <puerto>\n");
        return 1;
    }
    int sd,resp_size,valor,puerto;
    struct sockaddr_in addr;
    char *ptr;
    char pedido[BUFSIZE],respuesta[BUFSIZE];
    char buffer[BUFSIZE], usuario[BUFSIZE], pass[BUFSIZE];
    char user [5] = "USER";
    char contr [5] = "PASS";
    
    puerto = (atoi(argv[2]));
        
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1)
    {
        exitwithmsg("No se puede crear el socket\n");
    }

    addr.sin_addr.s_addr = inet_addr(argv[1]);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(puerto);

    // Connect to server
    if (connect(sd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(sd);
        exitwithmsg("Connection failed\n"); 
    }else{
        if((resp_size = recv(sd, buffer, BUFSIZE, 0))> 0) {
            printf( "Me llego del servidor: %s \n", buffer);
            authenticate(sd);
            operate(sd);
        }else{
            exitwithmsg("Servidor desconectado!");
        }
    }
    return 0;
}
