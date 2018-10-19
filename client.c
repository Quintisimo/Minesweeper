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
int MINES = 10;
char USERNAME[10];
char TILE_VALUES[9][9];
int SOCKET_ID;

bool login();
void play_game();
int game_options();
void menu_option();
void quit();
int letter_to_number(char letter);
void send_location(char tile_location[2], char user_selection);
void leader_board();

int main(int argc, char const *argv[]) {
  
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

  if ((SOCKET_ID = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    exit(1);
  }

  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(atoi(argv[2]));
  server_address.sin_addr = * ((struct in_addr*) server->h_addr);
  bzero(&server_address.sin_zero, 8);

  if (connect(SOCKET_ID, (struct sockaddr*) &server_address, sizeof(struct sockaddr)) == -1) {
    perror("connect");
    exit(1);
  }

  if(login()) {
    printf("Welcome to the Minesweeper gaming system\n");
    menu_option();
  }

  close(SOCKET_ID);
}


bool login() {
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
  if (send(SOCKET_ID, &username, 10, 0) == -1) {
    perror("send");
    exit(1);
  }

  if (recv(SOCKET_ID, &has_username, sizeof(int), 0) == -1) {
    perror("recv");
    exit(1);
  }

  printf("Password: ");
  fgets(password, 10, stdin);
  password[strcspn(password, "\n")] = '\0';
  if (send(SOCKET_ID, &password, 10, 0) == -1) {
    perror("send");
    exit(1);
  }

  if (recv(SOCKET_ID, &has_password, sizeof(int), 0) == -1) {
    perror("recv");
    exit(1);
  }

  if (has_username == -1 || has_password == -1) {
    printf("You either entered an incorrect username or password. Disconnecting.\n");
    quit(SOCKET_ID);
    return false;
  } else {
    return true;
  }
}

void quit() {
  close(SOCKET_ID);
}

void menu_option() {
  int selection = game_options();

  if (selection == 1) {
    play_game();
  } else if (selection == 2) {
    leader_board();
  } else {
    quit();
  }
}

int game_options() {
  int selection = 0;

  printf("Please enter a selection:\n");
  printf("\t<1> Play Minesweeper\n");
  printf("\t<2> Show Leaderboard\n");
  printf("\t<3> Quit\n\n");

  while(1) {
    if (selection >= 1 && selection <= 3) {
      break;
    } else {
      printf("Please select one of the options (1-3): ");
      scanf("%i", &selection);
    }
  }

  return selection;
}

void play_game() {
  // bool finished = false;
  char user_selection;
  char tile_location[2];
  int tile_value = 0;
  
  printf("\nRemaining mines: %i\n\n", MINES);
  printf("    ");

  for(int i = 1; i < 10; i++) {
    printf("%d ", i);
  }
  printf("\n");

  for(int i = 1; i < 23; i++) {
    printf("-");
  }

  printf("\n");
  for (int i = 0; i < 10; i++) {
    int num = i + 'A';
    printf("%c | ", (char)num);

    for (int j = 0; j < 9; j++) {
        printf("%c ", TILE_VALUES[i][j]);
    }
    printf("\n");
  }

  printf("\n\n");
  printf("Choose an option\n");
  printf("<R> Reveal tile\n");
  printf("<P> Place flag\n");
  printf("<Q> Quit game\n");
  printf("\nOptions (R,P,Q): ");

  scanf("%s", &user_selection);

  if (user_selection == 'R') {
    printf("Reveal tile: ");
    scanf("%s", tile_location);
    send_location(tile_location, user_selection);
    play_game();
    if (tile_value == -1) {
      printf("Game Over! You hit a mine\n");
      quit();
    }

  } else if (user_selection == 'P') {
    printf("Place flag: ");
    scanf("%s", tile_location);
    send_location(tile_location, user_selection);

  } else {
    quit();
  }
}

int letter_to_number(char letter) {
  int y_location = letter - 'A';

  if (y_location < 9) {
    return y_location;
  }
  return -1;
}

void send_location(char tile_location[2], char user_selection) {
  int x = atoi(&tile_location[1]) - 1;
  int y = letter_to_number(tile_location[0]);
  int tile_value;

  if (x > 8 || y == -1) {
    fprintf(stderr, "Outside grid");
    exit(1);
  }

  if (send(SOCKET_ID, &x, sizeof(int), 0) == -1) {
    perror("send");
    exit(1);
  }

  if (send(SOCKET_ID, &y, sizeof(int), 0) == -1) {
    perror("send");
    exit(1);
  }

  if (recv(SOCKET_ID, &tile_value, sizeof(int), 0) == -1) {
    perror("recv");
    exit(1);
  }

  if (tile_value == -1) {
    TILE_VALUES[x][y] = '*';
  } else if (tile_value != -1 && TILE_VALUES[x][y] != '+') {
    TILE_VALUES[x][y] = '0' + tile_value;
  } else if (user_selection == 'P') {
    TILE_VALUES[x][y] = '+';
  }
}

void leader_board() {
  int time_taken = 0;
  int games_won = 0;
  int games_played = 0;
  
  printf("\n===========================================================");
  printf("\n|      Name      ");
  printf("|  Time (sec)  ");
  printf("|  Wins  ");
  printf("|  Games Played  |\n");
  printf("===========================================================\n");

  printf("\n|     %s     ", USERNAME);
  printf("|    %d    ", time_taken);
  printf("|   %d   ", games_won);
  printf("|   %d   |\n", games_played);
  printf("===========================================================\n\n");

  menu_option();
}