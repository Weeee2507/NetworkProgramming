#include <arpa/inet.h>  // htons(), inet_addr()
#include <dirent.h>     // opendir(), readdir(), closedir()
#include <errno.h>      // errno
#include <netinet/in.h> // inet_addr(), bind()
#include <signal.h>     // signal()
#include <stdbool.h>    // bool
#include <stdio.h>
#include <stdlib.h>     // strtol()
#include <string.h>     // bzero(), strlen(), strcmp(), strcpy(), strtok(), strrchr(), memcmp()
#include <sys/socket.h> // socket(), inet_addr(), bind(), listen(), accept(), recv(), send()
#include <sys/types.h>  // socket()
#include <unistd.h>     // close()
#include <ctype.h>
#include <sys/stat.h>   //mkdir()
#include <sys/wait.h>   //waitpid();
#include <libgen.h>

#include "account.h"
#include "server.h"

#define MaxClient 20
#define MAX 1024

int main(int argc, const char *argv[]) {
  // check argument
 	if(argc != 2){
 		printf("Invalid argument\n\n");
 		return 0;
 	}

  char username[MAX],pass[MAX],folder[MAX], *reply;
  int bytes_sent, bytes_received;
  char filename[] = "account.txt";
  char str[MAX];

  // Initialize Addresses
  int sock;
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_addr_length = sizeof(client_addr);
  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(atoi(argv[1]));
  server_addr.sin_addr.s_addr = INADDR_ANY;
  int pid;

  node_a *found;
  node_a *account_list = loadData(filename);

  // menu
  int chon = 0;
  do {
      // system("clear");
      printf("\n============MENU===========\n");
      printf("|1. Show all clients       |\n");
      printf("|2. Start server           |\n");
      printf("|3. Exit                   |\n");
      printf("===========================\n");
      printf("Enter your choice: ");
      scanf("%d%*c", &chon);

      switch(chon) {
        case 1:
          // show all clients
          printf("\n");
          printf("%-17s%-25s%-17s\n", "Username", "Pass", "Folder");

          for(node_a *p = account_list; p != NULL; p = p->next) {
            printf("%-17s%-25s%-17s\n", p->username, p->pass, p->folder);
          }
          printf("\n");
          break;

        case 2:
          // start server
          // Create Socket
          if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
            // perror("Create socket error");
            // exit(errno);
            perror("\nError: ");
            return 0;
          }

          // Bind Socket to Address
          if ((bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr))) == -1) {
            perror("Binding error");
            // close(sock);
            // exit(errno);
            return 0;
          }

          // Start Listening
          if ((listen(sock, MaxClient)) == -1) {
            perror("\nError:");
            return 0;
          }
          signal(SIGCHLD, sig_chld);

          // Start Accepting
          printf("Start Listening\n");

          while (1) {
            int recfd;
            recfd = accept(sock, (struct sockaddr *)&client_addr, &client_addr_length);
            printf("Who's that pokemon???\n");

            if (recfd == -1) {
              perror("Accept error");
              continue;
            }

            pid = fork();
            if (pid < 0) {
              perror("Error: ");
              return 1;
            }

            if (pid == 0) {
              // Initialize Buffer, Response, FDs
              int buffer_size = 1024;
              char *buffer = malloc(sizeof(char) * buffer_size);
              char *current_path = malloc(sizeof(char) * 2);

              int login = 0,path = 0;
              strcpy(current_path,".");


              while (login == 0) {
                if (0 >= (bytes_received = recv(recfd,buffer,MAX-1,0))){
                  printf("Connection closed\n");
                  break;
                }
                buffer[bytes_received] = '\0';

                char *temp = malloc(sizeof(char) * buffer_size);
                strcpy(temp,buffer);

                account_process(recfd, buffer, account_list, &login, filename);

                temp = strtok(temp,"|");
                temp = strtok(NULL,"|");

                strcpy(username,temp);

                memset(buffer, '\0', MAX);
              }

              while (path == 0) {

              	printf ("%s\n",username);
                node_a *found = findNode(account_list, username);
                strcat(current_path,"/");
                strcat(current_path,found->folder);
                sleep(1);
                printf("%s in %s\n",username,current_path);
                respond((int)recfd,current_path);
                printf("%s\n", current_path);
                path = 1;
              }

              // Read-Evaluate-Print Loop
              while (true) {
                // Recieve message
                // in ra VD: (4)terminated
                if ((recv(recfd, buffer, buffer_size, 0)) < 1) {
                  fprintf(stderr, "(%s) Terminated\n", username);
                  // perror("");
                  close(recfd);
                  break;
                }

                // Evaluate
                // in ra command đã nhận
                printf("(%s) Command: %s\n", username, buffer);
                // thực thi command
                server_process(recfd, buffer, &current_path);
              }

              // Clean Up
              free(buffer);
              free(current_path);
              // close(recfd);
            } else {
              close(recfd);
            }
          }

          close(sock);
          break;

        case 3:
          break;

        default:
          printf("Please choose options only in MENU!\n");
          break;
      }
  } while(chon!=3);
  free(account_list);
  return 0;
}
