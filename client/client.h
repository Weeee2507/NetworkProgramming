#define MAX 1024

enum msg_type{
    ls,
    cd,
    upload,
    download,
    mkdir,
    rm
};

int is_regular_file(const char *path) {
    struct stat path_stat;

    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

unsigned long hash(char *str){
    unsigned long hash = 5381;
    int c;

    while (c=*str++){
         hash = ((hash << 5) + hash) + c;
    }

    return hash;
}

int begin_with(const char *str, const char *pre) {
  size_t lenpre = strlen(pre);
  size_t lenstr = strlen(str);

  return lenstr < lenpre ? 0 : memcmp(pre, str, lenpre) == 0;
}

void display_help(){
  printf("\nList of commands: \n");
  printf("\"ls\": list files and folders in the current directory\n");
  printf("\"cd [path]\": change current directory\n");
  printf("\"upload [file path]\": upload file to server\n");
  printf("\"download [file name]\": download file from server\n");
  printf("\"mkdir [folder name]\": create a new folder in the current directory\n");
  printf("\"rm [file/folder name]\": remove a file or directory\n");
  printf("\"exit\": exit the program\n\n");
}

void client_ls(int sock, char *buffer) {
  char response[MAX];

  // Send & Evaluate
  if (send(sock, buffer, strlen(buffer) + 1, 0) == -1) {
    fprintf(stderr, "can't send packet");
    perror("");
    return;
  }

  // Recieve
  if (recv(sock, response, sizeof(response), 0) == -1) {
    fprintf(stderr, "can't receive packet");
    perror("");
    return;
  }

  // Print
  if (begin_with(response, "system")) {
    printf("Server Error: %s\n", &response[7]);
  } else {
    printf("%s", response);
  }
}

void client_cd(int sock, char *buffer, char **path) {
  char response[MAX];

  // Send & Evaluate
  if (send(sock, buffer, strlen(buffer) + 1, 0) == -1) {
    fprintf(stderr, "can't send packet");
    perror("");
    return;
  }

  // Recieve
  if (recv(sock, response, sizeof(response), 0) == -1) {
    fprintf(stderr, "can't receive packet");
    perror("");
    return;
  }

  // Print
  if (begin_with(response, "system")) {
    printf("%s\n", &response[7]);
  } else {
    free(*path);
    *path = malloc(strlen(response) + 1);
    strcpy(*path, response);
  }
}

void client_download(int sock, char *buffer, char *target_file) {
  ssize_t chunk_size;
  long received_size = 0;
  char response[MAX];

  if( access(target_file, F_OK ) == 0 ) {
    // file exists
    fprintf(stderr, "File already exists\n");
    return;
  }

  // Send Download Command
  if (send(sock, buffer, strlen(buffer) + 1, 0) == -1) {
    fprintf(stderr, "can't send packet");
    perror("");
    // fclose(fd);
    return;
  }

  // Retrieve File Size
  if (recv(sock, response, sizeof(response), 0) == -1) {
    fprintf(stderr, "can't receive packet");
    perror("");
    // fclose(fd);
    return;
  }

  if (begin_with(response, "system")) {
    printf("Server Error: %s\n", &response[7]);
    // fclose(fd);
    return;
  }

  // Initialize File Descriptor
  FILE *fd = fopen(target_file, "wb");
  if (fd == NULL) {
    fprintf(stderr, "can't create file");
    perror("");
    return;
  }
  long file_size = strtol(response, NULL, 0);

  // Notify server to start transmission
  strcpy(buffer, "ready");
  if (send(sock, buffer, strlen(buffer) + 1, 0) == -1) {
    fprintf(stderr, "can't send packet");
    perror("");
    fclose(fd);
    return;
  }

  // Start Receiving
  while (received_size < file_size && (chunk_size = recv(sock, response, sizeof(response), 0)) > 0) {
    if (received_size + chunk_size > file_size) {
      fwrite(response, 1, file_size - received_size, fd);
      received_size += file_size - received_size;
    } else {
      fwrite(response, 1, chunk_size, fd);
      received_size += chunk_size;
    }
  }

  // Clean Up
  printf("%s Downloaded\n", target_file);
  fclose(fd);
}

void client_upload(int sock, char *buffer, char *target_file) {
  ssize_t chunk_size;
  char response[MAX];

  if (is_regular_file(target_file) == 0) {
    fprintf(stderr, "%s is a folder or not existed\n",target_file);
    // fclose(fd);
    return;
  }

  // Initialize File Descriptor
  FILE *fd = fopen(target_file, "rb");
  if (fd == NULL) {
    fprintf(stderr, "Error ");
    perror("Error");
    return;
  }

  // Send Upload Command
  if (send(sock, buffer, strlen(buffer) + 1, 0) == -1) {
    fprintf(stderr, "can't send packet");
    perror("");
    return;
  }

  // Wait for server to be ready
  if (recv(sock, response, sizeof(response), 0) == -1) {
    fprintf(stderr, "can't receive packet");
    perror("");
    fclose(fd);
    return;
  }

  if (begin_with(response, "system")) {
    printf("Server Error: %s\n", &response[7]);
    fclose(fd);
    return;
  }

  // Notify File Size
  fseek(fd, 0L, SEEK_END);
  sprintf(buffer, "%ld", ftell(fd));
  if (send(sock, buffer, strlen(buffer) + 1, 0) == -1) {
    fprintf(stderr, "can't send packet");
    perror("");
    fclose(fd);
    return;
  }
  fseek(fd, 0L, SEEK_SET);

  // Wait for server to be ready
  if (recv(sock, response, sizeof(response), 0) == -1) {
    fprintf(stderr, "can't receive packet");
    perror("");
    fclose(fd);
    return;
  }

  // Start Transmission
  while ((chunk_size = fread(buffer, 1, sizeof(buffer), fd)) > 0) {
    if (send(sock, buffer, chunk_size, 0) == -1) {
      fprintf(stderr, "can't send packet");
      perror("");
      fclose(fd);
      return;
    }
  }

  // Clean Up
  printf("%s Uploaded\n", target_file);
  fclose(fd);
}

void client_mkdir(int sock, char *buffer, char *new_dir) {
  char response[MAX];

  // Send & Evaluate
  if (send(sock, buffer, strlen(buffer) + 1, 0) == -1) {
    fprintf(stderr, "can't send packet");
    perror("");
    return;
  }

  // Recieve
  if (recv(sock, response, sizeof(response), 0) == -1) {
    fprintf(stderr, "can't receive packet");
    perror("");
    return;
  }

  // Print
  if (begin_with(response, "system")) {
    printf("%s\n", &response[7]);
  }
}

void client_rm(int sock, char *buffer, char *target_file){
  char response[MAX];

  if (send(sock, buffer, strlen(buffer) + 1, 0) == -1) {
    fprintf(stderr, "can't send packet");
    perror("");
    return;
  }

  if (recv(sock, response, sizeof(response), 0) == -1) {
    fprintf(stderr, "can't receive packet");
    perror("");
    return;
  }

  if (begin_with(response, "system")) {
    printf("%s\n", &response[7]);
  }
}

void client_process(int sock, char *buffer, char **path) {
  // Prepare
  char *full_command = malloc(strlen(buffer) + 1);
  strcpy(full_command, buffer);
  char *delim = " ";
  char *command = strtok(full_command, delim);
  char *context = strtok(NULL, delim);
  enum msg_type MSG_TYPE;

  // Process
  if (begin_with(command, "ls")) {
    MSG_TYPE = ls;
  } else if (begin_with(command, "cd")) {
    MSG_TYPE = cd;
  } else if (begin_with(command, "download")) {
    MSG_TYPE = download;
  } else if (begin_with(command, "upload")) {
    MSG_TYPE = upload;
  } else if (begin_with(command,"mkdir")) {
    MSG_TYPE = mkdir;
  } else if (begin_with(command,"rm")){
    MSG_TYPE = rm;
  } else if (begin_with(command, "help")) {
    display_help();
  }

  switch(MSG_TYPE){
    case ls:
        client_ls(sock, buffer);
        break;
    case cd:
        client_cd(sock, buffer, path);
        break;
    case download:
        client_download(sock, buffer, context);
        break;
    case upload:
        client_upload(sock, buffer, context);
        break;
    case mkdir:
        client_mkdir(sock,buffer,context);
        break;
    case rm:
        client_rm(sock,buffer,context);
        break;
    default:
        printf("No such command: %s\nType \"help\" for list of commands\n", buffer);
        break;
  }

  // Cleanup
  free(full_command);
}
