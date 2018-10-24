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
#include <signal.h>
#include <pthread.h>
#include <time.h>

#define DEFAULT_PORT 12345
#define MAXDATASIZE 100
#define BACKLOG 10
#define RANDOM_NUMBER_SEED 42
#define NUM_TILES_X 9
#define NUM_TILES_Y 9
#define NUM_MINES 10

// leaderboard
typedef struct {
	char username[10];
  int game_time;
	int games_won;
	int games_played;
} leaderboard_t;

// authorisation
typedef struct {
  char username[10];
  char password[10];
} database_t;

// tile state
typedef struct {
  int adjacent_mines;
  bool revealed;
  bool is_mine;
} tile_t;

// grid
typedef struct {
  tile_t tiles[NUM_TILES_X][NUM_TILES_Y];
} game_state_t;

// typedef struct {
//   int number;
//   int socket_id;
//   int new_connection;
// } connections_t;

// int CONNECTED_CLIENTS = 0;
// connections_t CLIENTS[BACKLOG];
database_t DATABASE[BACKLOG];
leaderboard_t LEADERBOARD[BACKLOG];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

__thread time_t START_TIME;
__thread time_t END_TIME;
__thread int REMAINING_MINES = 1;
__thread game_state_t BOARD = {};

// struct request *requests = NULL;
// struct request *last_request = NULL;

void authentication();
void check_login(int new_connection);
void place_mines();
void adjacent_mines();
int send_tiles(int new_connection);
void send_mines(int new_connection);
void leaderboard(int new_connection);
void receive_options(int new_connection);
void start_game(int new_connection);

int main(int argc, char const *argv[]) {
  int socket_id, new_connection;
  int port = DEFAULT_PORT;
  struct sockaddr_in server_address;
  struct sockaddr_in client_address;
  socklen_t socket_length;

  srand(RANDOM_NUMBER_SEED);

  if (argc == 2) {
    port = atoi(argv[1]);
  }

  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(port);
  server_address.sin_addr.s_addr = INADDR_ANY;
  authentication();

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

  printf("Server is listening on %d\n", port);

  while (1) {
    pthread_t thread_id;
    pthread_attr_t thread_attributes;
    socket_length = sizeof(struct sockaddr);

    if ((new_connection = accept(socket_id, (struct sockaddr *)&client_address, &socket_length)) == -1) {
      perror("accept");
      continue;
    }

    printf("Server received a connection from %s\n", inet_ntoa(client_address.sin_addr));

    pthread_attr_init(&thread_attributes);
    pthread_create(&thread_id, &thread_attributes, (void * (*) (void *)) start_game, (void * __restrict__)(uintptr_t) new_connection);
    // CONNECTED_CLIENTS += 1;
    pthread_join(thread_id, NULL);

    // if (!fork()) {
    //   start_game(new_connection);
    //   close(new_connection);
    //   exit(0);
    // }
    // close(new_connection);

    // while (waitpid(-1, NULL, WNOHANG) > 0);
  }
}

void start_game(int new_connection) {
  check_login(new_connection);
  place_mines();
  adjacent_mines();
  time(&START_TIME);
  receive_options(new_connection);
  close(new_connection);
  pthread_exit(NULL);
}

void authentication() {
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
        strcpy(DATABASE[array_index].username, username);
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
        strcpy(DATABASE[array_index].password, password);
      }
    } else {
      fprintf(stderr, "Error reading password from Authentication.txt\n");
      exit(1);
    }

    if (username != NULL && (strcmp(username, "Username") != 0) && password != NULL && (strcmp(password, "Password") != 0)) {
      array_index++;
    }
  }

  fclose(authentication);
}

void check_login(int new_connection) {
  char username[10];
  char password[10];
  int has_username = -1;
  int has_password = -1;

  if (recv(new_connection, &username, 10, 0) == -1) {
    perror("recv");
    exit(1);
  }

  for (int i = 0; i < BACKLOG; i++) {
    if (strcmp(username, DATABASE[i].username) == 0) {
      has_username = 0;
      strcpy(LEADERBOARD[0].username, username);
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
    if (strcmp(password, DATABASE[i].password) == 0) {
      has_password = 0;
    }
  }

  if (send(new_connection, &has_password, sizeof(int), 0) == -1) {
    perror("send");
    exit(1);
  }
}

void place_mines() {
  for (int i = 0; i < NUM_MINES; i++) {
    int x;
    int y;

    do {
      pthread_mutex_lock(&mutex);
      x = rand() % NUM_TILES_X;
      y = rand() % NUM_TILES_Y;
      pthread_mutex_unlock(&mutex);
    } while(BOARD.tiles[x][y].is_mine);
    // place mine at (x, y)
    BOARD.tiles[x][y].is_mine = true;
  }
}

void adjacent_mines() {
  for (int i = 0; i < NUM_TILES_X; i++) {
    for (int j = 0; j < NUM_TILES_Y; j++) {
      if (BOARD.tiles[i + 1][j].is_mine) {
        BOARD.tiles[i][j].adjacent_mines += 1;
      }
      
      if (BOARD.tiles[i][j + 1].is_mine) {
        BOARD.tiles[i][j].adjacent_mines += 1;
      }
      
      if (BOARD.tiles[i + 1][j + 1].is_mine) {
        BOARD.tiles[i][j].adjacent_mines += 1;
      }

      if (BOARD.tiles[i - 1][j].is_mine) {
        BOARD.tiles[i][j].adjacent_mines += 1;
      }

      if (BOARD.tiles[i][j - 1].is_mine) {
        BOARD.tiles[i][j].adjacent_mines += 1;
      }

      if (BOARD.tiles[i - 1][j - 1].is_mine) {
        BOARD.tiles[i][j].adjacent_mines += 1;
      }
    }
  }
}

void receive_options(int new_connection) {
  int selection = 0;

  while(1) {
    if (recv(new_connection, &selection, sizeof(int), 0) == -1) {
      perror("recv");
      exit(1);
    }

    if (selection == 1) {
      if(send_tiles(new_connection) == -1) {
        break;
      }
    } else if (selection == 2) {
      leaderboard(new_connection);
    }
  }
}

int send_tiles(int new_connection) {
  int x = 0;
  int y = 0;
  int tile_value = 0;
  char user_selection;

  while(1) {
    if (send(new_connection, &REMAINING_MINES, sizeof(int), 0) == -1) {
      perror("send");
      exit(1);
    }

    if (recv(new_connection, &user_selection, sizeof(int), 0) == -1) {
      perror("recv");
      exit(1);
    }

    if (recv(new_connection, &x, sizeof(int), 0) == -1) {
      perror("recv");
      exit(1);
    }

    if (recv(new_connection, &y, sizeof(int), 0) == -1) {
      perror("recv");
      exit(1);
    }
    
    if (BOARD.tiles[x][y].is_mine) {
      tile_value = -1;
    } else {
      tile_value = BOARD.tiles[x][y].adjacent_mines;
      BOARD.tiles[x][y].revealed = true;
    }

    if (send(new_connection, &tile_value, sizeof(int), 0) == -1) {
      perror("send");
      exit(1);
    }

    if (tile_value == -1) {
      if (user_selection == 'R') {
        send_mines(new_connection);
        pthread_mutex_lock(&mutex);
        LEADERBOARD[0].games_played += 1;
        pthread_mutex_unlock(&mutex);
        return -1;
      } else if (user_selection == 'P') {
        REMAINING_MINES -= 1;
      }

      if (REMAINING_MINES == 0) {
        time(&END_TIME);
        int game_duration = difftime(END_TIME, START_TIME);

        if (send(new_connection, &REMAINING_MINES, sizeof(int), 0) == -1) {
          perror("send");
          exit(1);
        }

        if (send(new_connection, &game_duration, sizeof(int), 0) == -1) {
          perror("send");
          exit(1);
        }

        pthread_mutex_lock(&mutex);
        LEADERBOARD[0].games_played += 1;
        LEADERBOARD[0].games_won += 1;
        LEADERBOARD[0].game_time = game_duration;
        pthread_mutex_unlock(&mutex);
        return -1;
      }
    }
  }
  return 0;
}

void send_mines(int new_connection) {
  for(int i = 0; i < NUM_TILES_X; i++) {
    for(int j = 0; j < NUM_TILES_Y; j++) {
      if (BOARD.tiles[i][j].is_mine) {
        if (send(new_connection, &i, sizeof(int), 0) == -1) {
          perror("send");
          exit(1);
        }

        if (send(new_connection, &j, sizeof(int), 0) == -1) {
          perror("send");
          exit(1);
        }
      }
    }
  }
}

void leaderboard(int new_connection) {
	if (send(new_connection, &LEADERBOARD[0].username, 10, 0) == -1) {
    perror("send");
    exit(1);
  }

  if (send(new_connection, &LEADERBOARD[0].game_time, sizeof(int), 0) == -1) {
    perror("send");
    exit(1);
  }

  if (send(new_connection, &LEADERBOARD[0].games_won, sizeof(int), 0) == -1) {
    perror("send");
    exit(1);
  }

  if (send(new_connection, &LEADERBOARD[0].games_played, sizeof(int), 0) == -1) {
    perror("send");
    exit(1);
  }
}