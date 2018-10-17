#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>

#define MAXDATASIZE 100

int socket_id;

bool login(int socket_id);
void play_game(int socket_id);
int game_options();
int user_options();
void game_function(int socket_id);
void menu_option(int socket_id);
void quit();
int letter_to_number(char letter);

int main(int argc, char const *argv[]) {
  int socket_id;
  // int bytes_size;
  // char buffer[MAXDATASIZE];
  struct hostent *server;
  struct sockaddr_in server_address;

  if (argc != 3) {
    fprintf(stderr, "Please enter the server ip address and port\n");
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

  if (login(socket_id) == true) {
    menu_option(socket_id);
  }

  close(socket_id);
}


bool login(int socket_id) {
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
    return false;
    exit(1);
  } else {
    return true;
  }
}

void quit() {
  close(socket_id);
}

void menu_option(int socket_id) {
  int selection = game_options();

  if (selection == 1) {
    play_game(socket_id);
  } else if (selection == 2) {
    printf("Leaderboard\n");
  } else {
    quit();
  }
}

int game_options() {
  int selection = 0;

  printf("Welcome to the Minesweeper gaming system\n");
  printf("Please enter a selection:\n");
  printf("\t<1> Play Minesweeper\n");
  printf("\t<2> Show Leaderboard\n");
  printf("\t<3> Quit\n\n");

  // if(scanf("%i", &selection) == -1) {
  //   fprintf(stderr, "Error reading selection");
  // }

  // if (scanf("%i", &selection) < 3) {
  //   play_game();
  // }

  while(1) {
    if (selection >= 1 && selection <= 3) {
      break;
    } else {
      printf("Please select one of the user_options (1-3): ");
      scanf("%i", &selection);
    }
  }

  return selection;
}

void play_game(int socket_id) {
  // bool finished = false;
  // char selection;
  int mines = 0;

  // receive number of mines
  if (recv(socket_id, &mines, sizeof(int), 0) == -1) {
    perror("recv");
    exit(1);
  }
  
  printf("\nRemaining mines %i\n\n", mines);
  printf("   1 2 3 4 5 6 7 8 9\n");
  printf("---------------------\n");
  printf("A |\nB |\nC |\nD |\nE |\nF |\nG |\nH |\nI |\n\n");
  printf("Choose an option\n");
  printf("<R> Reveal tile\n");
  printf("<P> Place flag\n");
  printf("<Q> Quit game\n");

  // if (scanf("%c", &selection) == -1) {
  //   fprintf(stderr, "Error reading selection");
  game_function(socket_id);

}

int user_options() {
  char userSelection;
  printf("\nOptions (R,P,Q): ");
  scanf("%s", &userSelection);

  if (userSelection == 'R') {
    return 1;
  } else if (userSelection == 'P') {
    return 2;
  } else {
    return 3;
  }
}

void game_function(int socket_id) {
  char tile_location[2];

  int selection = user_options();
  if (selection == 1) {
    printf("Reveal tile: ");
    scanf("%s", tile_location);
    int x = atoi(&tile_location[1]) - 1;
    int y = letter_to_number(tile_location[0]);

    printf("%d\n", socket_id); //! socket id is changing to 0
    if (send(socket_id, &x, sizeof(int), 0) == -1) {
      perror("send");
      exit(1);
    }

    if (send(socket_id, &y, sizeof(int), 0) == -1) {
      perror("send");
      exit(1);
    }
    
  } else if (selection == 2) {
    printf("Place flag: \n");
    scanf("%s", tile_location);
  } else {
    quit();
  }
}

int letter_to_number(char letter) {
  switch (letter) {
    case 'A':
      return 0;
    case 'B':
      return 1;
    case 'C':
      return 2;
    case 'D':
      return 3;
    case 'E':
      return 4;
    case 'F':
      return 5;
    case 'G':
      return 6;
    case 'H':
      return 7;
    case 'I':
      return 8;
  }
  return -1;
}