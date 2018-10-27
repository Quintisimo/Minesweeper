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
#define NUM_MINES 9

int MINES = 0;
char TILE_VALUES[9][9];
int SOCKET_ID;
bool REVEAL_MINE = false;

bool login();
void play_game(bool is_mine);
void reset_game();
int game_options();
void menu_option();
void quit();
int letter_to_number(char letter);
bool send_location(int x, int y, char user_selection);
void leaderboard();

int main(int argc, char const *argv[]) {
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
    printf("\nWelcome to the Minesweeper gaming system\n");
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
  exit(0);
}

void menu_option() {
  int selection = game_options();

  if (send(SOCKET_ID, &selection, sizeof(int), 0) == -1) {
    perror("send");
    exit(1);
  }

  if (selection == 1) {
    play_game(true);
  } else if (selection == 2) {
    leaderboard();
  } else {
    quit();
  }
}

int game_options() {
  char selection;

  printf("\nPlease enter a selection:\n");
  printf("\t<1> Play Minesweeper\n");
  printf("\t<2> Show Leaderboard\n");
  printf("\t<3> Quit\n\n");

  while(1) {
    printf("Please select one of the options (1-3): ");
    
    if (scanf("%s", &selection) != -1) {
      if (selection == '1' || selection == '2' || selection == '3') {
        break;
      } else {
        printf("Invaid Option %c. Please try again\n", selection);
      }
    } else {
      printf("Error reading value. Please try again\n");
    }
  }

  return strtol(&selection, NULL, 10);
}

void play_game(bool is_mine) {
  char user_selection = '\0';
  char tile_location[2];
  bool mine = true;
  int x = 0;
  int y = 0;

  if (!REVEAL_MINE) {
    if (recv(SOCKET_ID, &MINES, sizeof(int), 0) == -1) {
      perror("recv");
      exit(1);
    }
  }

  printf("\nRemaining mines: %i\n\n", MINES);
  printf("    ");

  for(int i = 1; i < 10; i++) {
    printf("%d ", i);
  }
  printf("\n");

  for(int i = 0; i < 22; i++) {
    printf("-");
  }

  printf("\n");
  for (int i = 0; i < NUM_MINES; i++) {
    int num = i + 'A';
    printf("%c |", (char)num);

    for (int j = 0; j < NUM_MINES; j++) {
      if (strcmp(&TILE_VALUES[j][i], "") == 0) {
        printf("  ");
      } else {
        printf(" %c", TILE_VALUES[j][i]);
      }
    }
    printf("\n");
  }

  if (!REVEAL_MINE && MINES != 0) {
    printf("\n\n");

    if (!is_mine) {
      printf("Flagged tile was not a mine\n\n");
    }
    printf("Choose an option\n");
    printf("<R> Reveal tile\n");
    printf("<P> Place flag\n");
    printf("<Q> Quit game\n");

    while(1) {
      printf("\nOptions (R,P,Q): ");

      if (scanf("%s", &user_selection) != -1) {
        if (!(user_selection == 'R' || user_selection == 'P' || user_selection == 'Q')) {
          printf("Invaid Option %c. Please try again.\n", user_selection);
        } else {
          break;
        }
      } else {
        printf("Error reading value. Please try again\n");
      }
    }
  } else {
    if (REVEAL_MINE) {
      printf("\nGame Over! You hit a mine\n");
      reset_game();
      menu_option();
    } else if (MINES == 0) {
      int game_duration;

      if (recv(SOCKET_ID, &game_duration, sizeof(int), 0) == -1) {
        perror("recv");
        exit(1);
      }

      printf("\nCongratulations! You have located all the mines.\n");
      printf("You won in %d seconds\n", game_duration);
      reset_game();
      menu_option();
    }
  }

  if (user_selection == 'R') {
    while(1) {
      printf("Reveal tile: ");

      if (scanf("%s", tile_location) != -1) {
        x = strtol(&tile_location[1], NULL, 10) - 1;
        y = letter_to_number(tile_location[0]);

        if (x > 8 || y == -1) {
          fprintf(stderr, "Invalid Tile Location\n");
        } else {
          break;
        }
      } else {
        printf("Error reading value. Please try again\n");
      }
    }

    mine = send_location(x, y, 'R');
    play_game(mine);

  } else if (user_selection == 'P') {
    while(1) {
      printf("Place flag: ");

      if (scanf("%s", tile_location) != -1) {
        x = atoi(&tile_location[1]) - 1;
        y = letter_to_number(tile_location[0]);

        if (x > 8 || y == -1) {
          fprintf(stderr, "Invalid Tile Location\n");
        } else {
          break;
        }
      } else {
        printf("Error reading value. Please try again\n");
      }
    }
    mine = send_location(x, y, 'P');
    play_game(mine);

  } else if (user_selection == 'Q') {
    if (send(SOCKET_ID, &user_selection, sizeof(int), 0) == -1) {
      perror("send");
      exit(1);
    }
    reset_game();
    menu_option();
  }
}

int letter_to_number(char letter) {
  int y_location = letter - 'A';

  if (y_location < 9) {
    return y_location;
  }
  return -1;
}

bool send_location(int x, int y, char user_selection) {
  int tile_value;

  if (send(SOCKET_ID, &user_selection, sizeof(int), 0) == -1) {
    perror("send");
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

  if (user_selection == 'P') {
    TILE_VALUES[x][y] = '+';

    if (tile_value == -1) {
      return true;
    } else {
      return false;
    }
  } else if (user_selection == 'R') {
    if (tile_value == -1) {
      for(int i = 0; i < MINES; i++) {
        int x = 0;
        int y = 0;

        if (recv(SOCKET_ID, &x, sizeof(int), 0) == -1) {
          perror("recv");
          exit(1);
        }

        if (recv(SOCKET_ID, &y, sizeof(int), 0) == -1) {
          perror("recv");
          exit(1);
        }

        TILE_VALUES[x][y] = '*';
      }
      REVEAL_MINE = true;
    } else {
      TILE_VALUES[x][y] = '0' + tile_value;
    }
  }
  return true;
}

void reset_game() {
  for (int i = 0; i < NUM_MINES; i++) {
    for (int j = 0; j < NUM_MINES; j++) {
      TILE_VALUES[i][j] = 0;
    }
  }

  REVEAL_MINE = false;
  MINES = 0;
}

void leaderboard() {
  int players = 0;
  bool no_wins = true;

  if (recv(SOCKET_ID, &players, sizeof(int), 0) == -1) {
    perror("recv");
    exit(1);
  }

  char username[10][players];
  int game_time[players];
  int games_won[players];
  int games_played[players];

  for (int i = 0; i < players; i++) {
    if (recv(SOCKET_ID, &username[i], 10, 0) == -1) {
      perror("recv");
      exit(1);
    }

    if (recv(SOCKET_ID, &game_time[i], sizeof(int), 0) == -1) {
        perror("recv");
        exit(1);
    }

    if (recv(SOCKET_ID, &games_won[i], sizeof(int), 0) == -1) {
        perror("recv");
        exit(1);
    }

    if (games_won[i] != 0) {
      no_wins = false;
    }

    if (recv(SOCKET_ID, &games_played[i], sizeof(int), 0) == -1) {
        perror("recv");
        exit(1);
    }
  }

  if (no_wins) {
    puts("----------------------------------------------------------------------------");
    puts("There is no information currently stored in the leaderbaord. Try again later");
    puts("----------------------------------------------------------------------------");
  } else {
      puts("\nLeaderboard:");
      puts("------------------------------------------------");
      printf("| %-20s| ", "Name");
      printf("%-8s| ", "Time");
      printf("%-6s| ", "Wins");
      printf("%-5s|\n", "Plays");
      puts("------------------------------------------------");

    for (int i = 0; i < players; i++) {
      printf("| %-20s| ", username[i]); // name
      printf("%-8d| ", game_time[i]); // time
      printf("%-6d| ", games_won[i]); // wins
      printf("%-5d|\n", games_played[i]); // plays

      puts("------------------------------------------------");
    }
  }

	menu_option();
}