#include <arpa/inet.h>  // htons(), inet_addr()
#include <errno.h>      // errno
#include <netinet/in.h> // inet_addr()
#include <stdbool.h>    // bool
#include <stdio.h>
#include <stdlib.h> // strtol()
#include <string.h> // bzero(), strlen(), strcmp(), strcpy(), strtok(), memcmp()
#include <sys/socket.h> // socket(), inet_addr(), connect(), recv(), send()
#include <sys/types.h>  // socket()
#include <unistd.h>     // close()
#include <sys/stat.h>

#include "client.h"

#define MAX 1024

int main(int argc, const char *argv[]) {
  // check argument
 	if(argc != 3){
 		printf("Invalid argument\n\n");
 		return 0;
 	}

  // Create Socket
  int sock; //clientfd for client;
  if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Can't create socket");
    exit(errno);
  }

  // Initialize Address, Buffer, Path
  struct sockaddr_in server_addr;
  char buffer[MAX];
  char buff[MAX];
  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(atoi(argv[2]));
  server_addr.sin_addr.s_addr = inet_addr(argv[1]);
  char username[MAX], password[MAX];
  int bytes_sent, bytes_received;
  char *path = malloc(sizeof(char)*1024);
  strcpy(path, ".");
  int LOGIN = 0, PATH = 0;

  // Connecting to Server
  if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
    perror("Connect Failed");
    close(sock);
    exit(errno);
  }

  int choice = 0;
  do {
      // system("clear");
      printf("\n============MENU===========\n");
      printf("|1. Register              |\n");
      printf("|2. Login                 |\n");
      printf("===========================\n");
      printf("Enter your choice: ");
      scanf("%d%*c", &choice);

      switch(choice) {
      	case 1:
            // get username
            printf("Username: ");
            fgets(username,MAX,stdin);
            username[strcspn(username,"\n")] = '\0';

            // get pass
            printf("Password: ");
            fgets(password,MAX,stdin);
            password[strcspn(password, "\n")] = '\0';
            
            sprintf(password,"%ld",hash(password));
            
            char *msg = malloc(strlen(username)+strlen(password)*1024);
            strcpy(msg,"register|");
            strcat(msg,username);
            strcat(msg,"|");
            strcat(msg,password);
            
            // send username to server
            if (0 >= (bytes_sent = send(sock,msg,strlen(msg),0))) {
                perror("\nCan't send to server");
                return 0;
            }

            // receive server reply
            if (0 >= (bytes_received = recv(sock,buff,MAX-1,0))) {
                perror("\nCan't receive result from server");
                return 0;
            }
            buff[bytes_received] = '\0';

            if (begin_with(buff,"SUCCESS")){
                printf("Register Successfully!\n");
                printf("Please login from the main menu to verify\n");
            } else if (begin_with(buff,"FAIL")){
                char *temp = strtok(buff, "|");
                temp = strtok(NULL,"|");
                printf("%s\n",temp);
                printf("Or may be you want to Log in instead?\n");
            } else {
                printf("Messange has been tampered!\n");
                return 0;
            }

            memset(buff, '\0', MAX);
            free(msg);
      		break;

      	case 2:
      		while (LOGIN == 0) {
    			printf("Username: ");
    			fgets(username,MAX,stdin);
    			username[strcspn(username,"\n")] = '\0';

    			// get pass
    			printf("Password: ");
    			fgets(password,MAX,stdin);
    			password[strcspn(password, "\n")] = '\0';
    			
    			sprintf(password,"%ld",hash(password));

    			char *msg = malloc(strlen(username)+strlen(password)*1024);
    			strcpy(msg,"login|");
    			strcat(msg,username);
    			strcat(msg,"|");
    			strcat(msg,password);

    			// send username to serv
    			if (0 >= (bytes_sent = send(sock,msg,strlen(msg),0))) {
      				perror("\nCan't send to server");
      				return 0;
    			}

    			// receive server reply
    			if (0 >= (bytes_received = recv(sock,buff,MAX-1,0))) {
      				perror("\nCan't receive result from server");
      				return 0;
    			}
    			buff[bytes_received] = '\0';

                if (begin_with(buff,"SUCCESS")){
                    printf("Login Successfully!\n");
                    LOGIN = 1;
                } else if (begin_with(buff,"FAIL")){
                    char *temp = strtok(buff, "|");
                    temp = strtok(NULL,"|");
                    printf("%s\n",temp);
                } else {
                    printf("Messange has been tampered!\n");
                    return 0;
                }

                memset(buff, '\0', MAX);
                    free(msg);
            }
      		break;
      	default:
      		printf("No such choice, please enter 1-3!\n");
      		break;
      }
  } while (choice!=2);

  sleep(1);
  if (0 >= (bytes_received = recv(sock,path,MAX-1,0))) {
    printf("\nFailed to receive folder!\n");
    return 0;
  }
  path[bytes_received] = '\0';
  printf("If needed then type \"help\" for list of commands\n");

  // Read-Evaluate-Print Loop
  while (true) {
    // Read
    printf("(%s) $ ", path);
    fgets(buffer, 1024, stdin);

    if (strcmp(buffer, "\n") == 0) {
      continue; // Empty Input, reset
    }

    if ((strlen(buffer) > 0) && (buffer[strlen(buffer) - 1] == '\n')) {
      buffer[strlen(buffer) - 1] = '\0'; // Standard Input, fix EOL
    }

    if (strcmp(buffer, "exit") == 0) {
      break; // Exit, break loop
    }

    // Processing
    client_process(sock, buffer, &path);
  }

  printf("Session Terminated\n");
  close(sock);
  free(path);
}
