#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAXDATASIZE 100

void login(int socket_id);
void play_game();
void menu();

int main(int argc, char const *argv[]) {
  int socket_id, bytes_size;
  char buffer[MAXDATASIZE];
  struct hostent *server;
  struct sockaddr_in server_address;

  if (argc != 3) {
    fprintf(stderr, "Please enter the server ip address and port");
    exit(1);
  }

  if ((server = gethostbyname(argv[1])) == NULL) {
    herror("gethostbyname");
    exit(1);
  }

  if ((socket_id = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    exit(1);
  }

  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(atoi(argv[2]));
  server_address.sin_addr = * ((struct in_addr*) server->h_addr);
  bzero(&server_address.sin_zero, 8);

  if (connect(socket_id, (struct sockaddr*) &server_address, sizeof(struct sockaddr)) == -1) {
    perror("connect");
    exit(1);
  }

  login(socket_id);
  close(socket_id);
}


void login(int socket_id) {
  char username[10];
  char password[10];
  int has_username;
  int has_password;

  printf("====================================================\n");
  printf("Welcome to the online Minesweeper gaming system\n");
  printf("====================================================\n\n");
  printf("You are required to login with your username and password\n\n");
  printf("Username: ");

  fgets(username, 10, stdin);
  username[strcspn(username, "\n")] = '\0';
  if (send(socket_id, &username, 10, 0) == -1) {
    perror("send");
    exit(1);
  }

  if (recv(socket_id, &has_username, sizeof(int), 0) == -1) {
    perror("recv");
    exit(1);
  }

  if (has_username == -1) {
    printf("Incorrect username\n");
    exit(1);
  }
  
  printf("Password: ");
  fgets(password, 10, stdin);
  password[strcspn(password, "\n")] = '\0';
  if (send(socket_id, &password, 10, 0) == -1) {
    perror("send");
    exit(1);
  }

  if (recv(socket_id, &has_password, sizeof(int), 0) == -1) {
    perror("recv");
    exit(1);
  }

  if (has_password == -1) {
    printf("Incorrect password\n");
    exit(1);
  }
}

void menu() {
  int selection;
  printf("Welcome to the Minesweeper gaming system\n");
  printf("Please enter a selection:\n");
  printf("\t<1> Play Minesweeper");
  printf("\t<2> Show Leaderboard");
  printf("\t <3> Quit");
  printf("Please select one of the options (1-3):");
  if(scanf("%i", &selection) == -1) {
    fprintf(stderr, "Error reading selection");
  }

  if (selection == 1) {
    play_game();
  }
}

void play_game() {
  char selection;
  printf("Remaining mines %i\n\n", mines);
  printf("1 2 3 4 5 6 7 8 9");
  printf("-----------------");
  printf("A |\nB |\nC |\nD |\nE |\nF |\nG |\nH |\nI |\n\n");
  printf("Choose an option\n");
  printf("<R> Reveal tile\n");
  printf("<P> Place flag");
  printf("<Q> Quit game\n");
  printf("Options (R,P,Q):");
  if (scanf("%c", &selection) == -1) {
    fprintf(stderr, "Error reading selection");
  }
}