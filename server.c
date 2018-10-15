#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>

#define MAXDATASIZE 100
#define BACKLOG 10
#define RANDOM_NUMBER_SEED 42
#define NUM_TILES_X 9
#define NUM_TILES_Y 9
#define NUM_MINES 10
#define DEFAULT_PORT 12345 /* default port used by the server */

typedef struct {
  char username[10];
  char password[10];
} Auth;

typedef struct {
  int adjacent_mines;
  bool revealed;
  bool is_mine;
} Tile;

typedef struct {
  Tile tiles[NUM_TILES_X][NUM_TILES_Y];
} GameState;

typedef struct request request_t;
struct request {
  int number;
  int sockfd;
  request_t* next;
};

struct request *requests = NULL;
struct request *last_request = NULL;

void authentication(Auth database[]);
void check_login(int new_connection, Auth database[]);
void place_mines();

int main(int argc, char const *argv[]) {
  // uint16_t portNum = DEFAULT_PORT;
  int socket_id, new_connection;
  struct sockaddr_in server_address;
  struct sockaddr_in client_address;
  socklen_t socket_length;

  Auth Database[BACKLOG];
  GameState Board[NUM_MINES];
  srand(RANDOM_NUMBER_SEED);

  if (argc != 2) {
    fprintf(stderr, "Port number has been set to default: 12345\n");

      server_address.sin_family = AF_INET;
      server_address.sin_port = htons(DEFAULT_PORT);
      server_address.sin_addr.s_addr = INADDR_ANY;
  } else {
      server_address.sin_family = AF_INET;
      server_address.sin_port = htons(atoi(argv[1]));
      server_address.sin_addr.s_addr = INADDR_ANY;    
  }

  // if (argc > 1) {
  //   portNum = atoi(argv[1]);
  // } else {
  //   portNum = DEFAULT_PORT;
  // }

  authentication(Database);

  if ((socket_id = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    exit(1);
  }

  if (bind(socket_id, (struct sockaddr *)&server_address, sizeof(struct sockaddr)) == -1) {
    perror("bind");
    exit(1);
  }

  if (listen(socket_id, BACKLOG) == -1) {
    perror("listen");
    exit(1);
  }

  printf("Server is listening on %d\n", atoi(argv[1]));

  while (1) {
    socket_length = sizeof(struct sockaddr);

    if ((new_connection = accept(socket_id, (struct sockaddr *)&client_address, &socket_length)) == -1) {
      perror("accept");
      continue;
    }

    printf("Server received a connection from %s\n", inet_ntoa(client_address.sin_addr));

    if (!fork()) {
      check_login(new_connection, Database);
      place_mines(Board);
      int mines = NUM_MINES;
      if (send(new_connection, &mines, sizeof(int), 0) == -1) {
        perror("send");
        exit(1);
      }
      close(new_connection);
      exit(0);
    }
    close(new_connection);

    while (waitpid(-1, NULL, WNOHANG) > 0);
  }
}

void authentication(Auth database[]) {
  FILE *authentication;
  int array_index = 0;
  char buffer[1000];

  if (!(authentication = fopen("Authentication.txt", "r"))) {
    fprintf(stderr, "Failed to read Authentication.txt\n");
    exit(1);
  }

  while (fgets(buffer, 1000, authentication) != NULL) {
    char *username = strtok(buffer, "\t");
    char *password = strtok(NULL, "\t\n");

    if (username != NULL) {
      if (strcmp(username, "Username") != 0) {
        if (isspace(username[strlen(username) - 1])) {
          username[strlen(username) - 1] = '\0';
        }
        strcpy(database[array_index].username, username);
      }
    } else {
      fprintf(stderr, "Error reading usernames from Authentication.txt\n");
      exit(1);
    }

    if (password != NULL) {
      if (strcmp(password, "Password") != 0) {
        if (isspace(password[strlen(password) - 1])) {
          password[strlen(password) - 1] = '\0';
        }
        strcpy(database[array_index].password, password);
      }
    } else {
      fprintf(stderr, "Error reading password from Authentication.txt\n");
      exit(1);
    }

    if (username != NULL && (strcmp(password, "Password") != 0) && password != NULL && (strcmp(password, "Password") != 0)) {
      array_index++;
    }
  }

  fclose(authentication);
}

void check_login(int new_connection, Auth database[]) {
  char username[10];
  char password[10];
  int has_username = -1;
  int has_password = -1;

  if (recv(new_connection, &username, 10, 0) == -1) {
    perror("recv");
    exit(1);
  }

  for (int i = 0; i < BACKLOG; i++) {
    if (strcmp(username, database[i].username) == 0) {
      has_username = 0;
      break;
    }
  }

  if (send(new_connection, &has_username, sizeof(int), 0) == -1) {
    perror("send");
    exit(1);
  }

  if (recv(new_connection, &password, 10, 0) == -1) {
    perror("recv");
    exit(1);
  }

  for (int i = 0; i < BACKLOG; i++) {
    if (strcmp(password, database[i].password) == 0) {
      has_password = 0;
      break;
    }
  }

  if (send(new_connection, &has_password, sizeof(int), 0) == -1) {
    perror("send");
    exit(1);
  }
}

bool tile_contains_mine(int x, int y) {
  bool contains_mine;

  for (int i = 0; i < NUM_TILES_X; i++) {
    for (int j = 0; j < NUM_TILES_Y; j++) {
      if ((x == i) && (y == j)) { 
        contains_mine = true;
      } else {
        contains_mine = false;
      }
    }
  }
  return contains_mine;
}

void place_mines() {
  for (int i = 0; i < NUM_MINES; i++) {
    int x;
    int y;

    do {
      x = rand() % NUM_TILES_X;
      y = rand() % NUM_TILES_Y;
    } while (tile_contains_mine(x,y));
      // place mine at (x, y) 
      for (int i = 0; i < NUM_TILES_X; i++) {
        for (int j = 0; j < NUM_TILES_Y; j++) {
          if (tile_contains_mine(x,y)) {
            printf("*");
          }
        }
      }
    }
}


void minesweeperLoop(int new_connection) {
  char buf[MAXDATASIZE];

  place_mines();

    // Send the placed mines to the client
    if (send(new_connection, buf, sizeof(buf), 0) == -1) {
      close(new_connection);
      perror("send");
    }

  }
