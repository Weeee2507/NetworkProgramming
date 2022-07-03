#define MaxClient 20
#define MAX 1024

#include "account.h"

enum msg_type{
    register_,
    login_,
    ls,
    cd,
    upload,
    download,
    mkdir_,
    rm
};

int dem(char *s,char t)
{
  int dem=0;
  for(int i=0;i<=strlen(s);i++) {
    if(s[i]==t) dem=dem+1;
  }
  return dem;
}

int is_regular_file(const char *path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

void sig_chld(int signo){
	pid_t pid;
	int stat;

	/* Wait the child process terminate */
	while((pid = waitpid(-1, &stat, WNOHANG))>0)
		printf("\nChild %d terminated\n",pid);
}

// xem command str có bắt đầu bằng xâu pre (VD begin_with(command,"ls") trả về 1 nếu command bắt đầu bằng ls)
int begin_with(const char *str, const char *pre) {
  size_t lenpre = strlen(pre);
  size_t lenstr = strlen(str);

  return lenstr < lenpre ? 0 : memcmp(pre, str, lenpre) == 0;
}

// gửi thông điệp (phản hồi) lại client
int respond(int recfd, char response[]) {
  if ((send(recfd, response, strlen(response) + 1, 0)) == -1) {
    fprintf(stderr, "Can't send packet\n");
    return errno;
  }
  return 0;
}

void server_login(int recfd, node_a *list, char *username, char *pass, int *login, char *response) {
  // check username exist
  node_a *found = findNode(list,username);

  if (found != NULL) {
    if (0 == strcmp(found->pass,pass)) {
      strcpy(response,"SUCCESS");
      *login = 1;
    } else {
      strcpy(response,"FAIL|Wrong Password");
    }
 } else {
   strcpy(response,"FAIL|Can't Find Username");
 }
}

void server_register(int recfd, node_a *list, char *username, char *pass, char *filename, char *response) {
    // Create a client
    if (findNode(list, username) != NULL) {
        strcpy(response,"FAIL|Account Existed");
        return;
    }
    strcpy(response,"SUCCESS");

    int c = 0;
    do {
        errno = 0;
        int ret = mkdir(username, S_IRWXU);
        if (ret == -1) {
            switch (errno) {
                case EACCES :
                  printf("The root directory does not allow write. ");
                  break;

                case EEXIST:
                  printf("Folder %s already exists. \n%s used for client.",username, username);
                  c = 1;
                  break;

                case ENAMETOOLONG:
                  printf("Pathname is too long");
                  break;

                default:
                  printf("mkdir");
                  break;
            }
        } else {
              printf("Created: %s\n", username);
              printf("Folder %s is created",username);
              c = 1;
        }
    } while (c == 0);

    list = AddTail(list,username,pass,username);
    saveData(list,filename);

    printf("\n");
}

void account_process(int recfd, char *message, node_a *list, int *login, char *filename) {
  // Prepare
  char *delim = "|";
  char *command = strtok(message, delim);
  char *username = strtok(NULL, delim);
  char *password = strtok(NULL, delim);
  char *response = malloc(sizeof(char) * 1024);
  enum msg_type MSG_TYPE;

  // Process
  if (begin_with(command, "register")) {
    MSG_TYPE = register_;
  } else if (begin_with(command, "login")) {
    MSG_TYPE = login_;
  }

  switch(MSG_TYPE){
    case register_:
        server_register(recfd, list, username, password, filename, response);
        respond(recfd, response);
        break;
    case login_:
        server_login(recfd, list, username, password, login, response);
        respond(recfd, response);
        break;
    default:
        strcpy(response, "No such command: ");
        strcat(response, command);
        // gửi thông điệp (phản hồi) lại client
        respond(recfd, response);
        break;
  }

  // Cleanup
  free(response);
}

// xem cac tep trong thu muc (~lenh ls trong ubuntu)
void server_ls(int recfd, char *response, char **current_path) {
  memset(response, '\0', MAX);
  // Open Directory
  DIR *current_fd;
  if ((current_fd = opendir(*current_path)) == NULL) {
    // opendir return pointer to directory stream and on error NUll
    strcpy(response, "system|Can't open ");
    strcat(response, *current_path);  //send to client
    fprintf(stderr, "Can't open %s", *current_path); //remain in server
    perror("");
    return;
  }

  // Read Directory
  //d_type = d_type is the term used in Linux kernel that stands for “directory entry type”
  // struct dirent: Đây là một kiểu cấu trúc được sử dụng để trả về thông tin về các mục trong thư mục. Nó chứa các trường sau
  // https://www.gnu.org/software/libc/manual/html_node/Directory-Entries.html
  struct dirent *dir_entry;
  while ((dir_entry = readdir(current_fd)) != NULL) {
    // tra ve cac loai file/folder
    switch (dir_entry->d_type) {
    case DT_BLK:
      strcat(response, "block device\t\t");
      break;
    case DT_CHR:
      strcat(response, "character device\t\t");
      break;
    case DT_DIR:
      strcat(response, "directory\t\t");
      break;
    case DT_FIFO:
      strcat(response, "named pipe (FIFO)\t\t");
      break;
    case DT_LNK:
      strcat(response, "symbolic link\t\t");
      break;
    case DT_REG:
      strcat(response, "regular file\t\t");
      break;
    case DT_SOCK:
      strcat(response, "UNIX domain socket\t");
      break;
    default:
      strcat(response, "Unknown\t\t\t");
      break;
    }
    strcat(response, dir_entry->d_name);
    strcat(response, "\n");
  }

  // Close Directory
  if (closedir(current_fd) < 0) {
    fprintf(stderr, "Directory Close Error");
    perror("");
  }
}

// thay doi remote directory
void server_cd(int recfd, char *open_dir, char *response, char **current_path) {
  // Handle empty arg and . and ..
  // xử lý đối số trống . và ..

  // neu khong co ten file (vd: cd ) => thong bao no directory given
  if (open_dir == NULL) {
    strcpy(response, "system|no directory given");
    return;
    // neu "cd ."=> response = thu muc hien tai
  } else if (strcmp(open_dir, ".") == 0) {
    strcpy(response, *current_path);
    return;
    // "cd .."
  } else if (strcmp(open_dir, "..") == 0) {
    // Check Root
    // neu current_path = . => thong bao da den goc
    if (dem(*current_path,'/') == 1) {
      strcpy(response, "system|already reached root");
      return;
    }
    // Truncate current path - cắt bớt đường dẫn hiện tại
    // failed
    char *trunct = strrchr(*current_path, '/');
    strcpy(trunct, "\0");
    strcpy(response, *current_path);
    return;
  }

  // Open Directory
  DIR *open_dir_fd;
  if ((open_dir_fd = opendir(*current_path)) == NULL) {
    strcpy(response, "system|can't open");
    strcat(response, *current_path);
    fprintf(stderr, "(%d) Can't open %s", recfd, *current_path);
    perror("");
    return;
  }

  // Check existance
  bool exist = false;
  struct dirent *dir_entry = NULL;
  // tìm từ đầu đến cuối thư mục hiện tại (current_path)
  while ((dir_entry = readdir(open_dir_fd)) != NULL && !exist) {
    if (dir_entry->d_type == DT_DIR && strcmp(dir_entry->d_name, open_dir) == 0) {
      // Build new path name
      char *new_path = malloc(strlen(*current_path) + strlen(dir_entry->d_name) + 2);
      strcpy(new_path, *current_path);
      strcat(new_path, "/");
      strcat(new_path, dir_entry->d_name);

      // Store current path ~ đổi current_path
      free(*current_path);
      *current_path = malloc(strlen(new_path));
      strcpy(*current_path, new_path);
      strcpy(response, *current_path);
      free(new_path);
      exist = true;
    }
  }

  if (!exist) {
    strcpy(response, "system|");
    strcat(response, *current_path);
    strcat(response, "/");
    strcat(response, open_dir);
    strcat(response, " does not exist");
  }

  // Close Directory
  if (closedir(open_dir_fd) < 0) {
    fprintf(stderr, "(%d) Directory Close Error", recfd);
    perror("");
  }
}

// download file
void server_download(int recfd, char *target_file, char **current_path) {

  // Build Path
  char *full_path = malloc(strlen(*current_path) + strlen(target_file) + 2);
  strcpy(full_path, *current_path);
  strcat(full_path, "/");
  strcat(full_path, target_file);

  if (is_regular_file(full_path) == 0) {
    respond(recfd,"system|Cannot download a folder or file not existed");
    fprintf(stderr, "%s is a folder or not existed\n",full_path);
    // fclose(fd);
    return;
  }

  // Initialize File Descriptor, Buffer
  FILE *fd;
  if ((fd = fopen(full_path, "rb")) == NULL) {
    respond(recfd, "system|file open error");
    fprintf(stderr, "Can't open %s", full_path);
    perror("");
    return;
  }

  char buffer[1024];
  ssize_t chunk_size;

  // Notify File Size
  fseek(fd, 0L, SEEK_END);  //0L => 0 long means 0.000KB/Mb file, SEEK_END => end of file;
  sprintf(buffer, "%ld", ftell(fd)); // ftell(fd) current position
  ssize_t byte_sent = send(recfd, buffer, strlen(buffer) + 1, 0);
  if (byte_sent == -1) {
    fprintf(stderr, "Can't send packet");
    perror("");
    fclose(fd);
    return;
  }
  fseek(fd, 0L, SEEK_SET);

  // Wait for client to be ready
  ssize_t byte_received = recv(recfd, buffer, sizeof(buffer), 0);
  if (byte_received == -1) {
    fprintf(stderr, "Can't receive packet");
    perror("");
    fclose(fd);
    return;
  }

  // Start Transmission
  while ((chunk_size = fread(buffer, 1, sizeof(buffer), fd)) > 0) {
    ssize_t byte_sent = send(recfd, buffer, chunk_size, 0);
    if (byte_sent == -1) {
      fprintf(stderr, "Can't send packet");
      perror("");
      fclose(fd);
      return;
    }
  }

  printf("Transmited: %s\n", target_file);
  fclose(fd);
}

void server_upload(int recfd, char *target_file, char **current_path) {
  // Build Path
  // char file_name[MAX];
  char *file_name = basename(target_file);
  char *full_path = malloc(strlen(*current_path) + strlen(file_name) + 2);
  strcpy(full_path, *current_path);
  strcat(full_path, "/");
  strcat(full_path, file_name);
  if( access(full_path, F_OK ) == 0 ) {
    // file exists
    fprintf(stderr, "File already exists\n");
    respond(recfd,"system|File already exists");
    free(full_path);
    return;
  }

  // Initialize File Descriptor
  FILE *fd;
  if ((fd = fopen(full_path, "wb")) == NULL) {
    respond(recfd, "system|file open error");
    fprintf(stderr, "Can't open %s",*current_path);
    perror("");
    return;
  }

  // Retrieve File Size
  char buffer[1024];
  strcpy(buffer, "?size");
  ssize_t byte_sent = send(recfd, buffer, strlen(buffer) + 1, 0);
  if (byte_sent == -1) {
    fprintf(stderr, "Can't send packet");
    perror("");
    fclose(fd);
    return;
  }

  ssize_t byte_received = recv(recfd, buffer, sizeof(buffer), 0);
  if (byte_received == -1) {
    fprintf(stderr, "Can't receive packet");
    perror("");
    fclose(fd);
    return;
  }
  long file_size = strtol(buffer, NULL, 0);  //strol => coveert string to long int

  // Notify client to start transmission
  strcpy(buffer, "ready");
  byte_sent = send(recfd, buffer, strlen(buffer) + 1, 0);
  if (byte_sent == -1) {
    fprintf(stderr, "Can't send packet");
    perror("");
    fclose(fd);
    return;
  }

  // Start Receiving
  ssize_t chunk_size;
  long received_size = 0;
  while (received_size < file_size && (chunk_size = recv(recfd, buffer, sizeof(buffer), 0)) > 0) {
    if (chunk_size == -1) {
      fprintf(stderr, "Can't receive packet");
      perror("");
      fclose(fd);
      return;
    }

    if (received_size + chunk_size > file_size) {
      fwrite(buffer, 1, file_size - received_size, fd);
      received_size += file_size - received_size;
    } else {
      fwrite(buffer, 1, chunk_size, fd);
      received_size += chunk_size;
    }
  }

  free(full_path);
  fprintf(stderr, "Saved: %s\n", file_name);
  fclose(fd);
}

void server_rm(int recfd, char *target_file, char *response, char **current_path) {
  char *full_path = malloc(strlen(*current_path) + strlen(target_file) + 2);
  strcpy(full_path, *current_path);
  strcat(full_path, "/");
  strcat(full_path, target_file);

  int ret = remove(full_path);
  if(ret != 0) {
    strcpy(response,"system|Error: unable to delete the file/folder");
    free(full_path);
    perror("Error");
    return;
  } else {
    printf("File deleted successfully\n");
    strcpy(response,"system|File deleted successfully");
  }
  free(full_path);
}

void server_mkdir(int recfd,char *new_dir, char *response, char **current_path){
    // Handle empty arg and . and ..
  // xử lý đối số trống . và ..
  if (new_dir == NULL) {
    strcpy(response,"system|no directory name given");
    return;
  } else if (strcmp(new_dir,".") == 0 || strcmp(new_dir,"..") == 0) {
    strcpy(response,"system|Wrong name format");
    return;
  }

  char *new_path = malloc(strlen(*current_path) + strlen(new_dir) + 2);
  strcpy(new_path, *current_path);
  strcat(new_path, "/");
  strcat(new_path, new_dir);

  errno = 0;
  int ret = mkdir(new_path, S_IRWXU);
  if (ret == -1) {
      switch (errno) {
          case EACCES :
              strcpy(response,"system|The parent directory does not allow write");
              free(new_path);
              return;
          case EEXIST:
              strcpy(response,"system|pathname already exists");
              free(new_path);
              return;
          case ENAMETOOLONG:
              strcpy(response,"system|pathname is too long");
              free(new_path);
              return;
          default:
              strcpy(response,"system|mkdir");
              free(new_path);
              return;
      }
  } else {
    fprintf(stderr, "Created: %s\n", new_dir);
    strcpy(response,"system|Folder is created");
  }
  free(new_path);
}

void server_process(int recfd, char *full_command, char **current_path) {
  // Prepare
  char *delim = " "; //dấu cách phân định command
  // strtok: Chia 1 chuỗi dài thành các chuỗi nhỏ được phân biệt riêng rẽ bởi các ký tự đặc biệt được chọn.
  char *command = strtok(full_command, delim); //lấy phần đầu của command (ls,cd,...)
  char *context = strtok(NULL, delim); //phần sau của command
  char *response = malloc(sizeof(char) * 1024);
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
    MSG_TYPE = mkdir_;
  } else if (begin_with(command,"rm")){
    MSG_TYPE = rm;
  }

  switch(MSG_TYPE){
    case ls:
        server_ls(recfd, response, current_path);
        respond(recfd, response);
        break;
    case cd:
        server_cd(recfd, context, response, current_path);
        respond(recfd, response);
        break;
    case download:
        server_download(recfd, context, current_path);
        break;
    case upload:
        server_upload(recfd, context, current_path);
        break;
    case mkdir_:
        server_mkdir(recfd, context, response, current_path);
        respond(recfd,response);
        break;
    case rm:
        server_rm(recfd,context,response,current_path);
        respond(recfd,response);
        break;
    default:
        strcpy(response, "No such command: ");
        strcat(response, command);
        // gửi thông điệp (phản hồi) lại client
        respond(recfd, response);
        break;
  }
  // Cleanup
  free(response);
}
