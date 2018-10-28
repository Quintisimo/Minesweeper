#define _GNU_SOURCE
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

#if (__APPLE__)
  #define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP PTHREAD_RECURSIVE_MUTEX_INITIALIZER
#endif

#define DEFAULT_PORT 12345
#define MAXDATASIZE 100
#define BACKLOG 10
#define RANDOM_NUMBER_SEED 42
#define NUM_TILES_X 9
#define NUM_TILES_Y 9
#define NUM_MINES 10

// leaderboard
typedef struct leaderboard {
	char username[10];
  int game_time;
	int games_won;
	int games_played;
  struct leaderboard *next;
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

typedef struct request {
  int number;
  int connection;
  struct request *next;
} request_t;

pthread_mutex_t REQUEST_MUTEX = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
pthread_cond_t RECEIVED_REQUEST = PTHREAD_COND_INITIALIZER;
int NUM_REQUESTS = 0;
int NUM_CLIENTS = 0;

database_t DATABASE[BACKLOG];

char USERNAME[10];
leaderboard_t *LEADERBOARD = NULL;

pthread_mutex_t MUTEX = PTHREAD_MUTEX_INITIALIZER;
pthread_t THREAD_IDS[BACKLOG];

__thread game_state_t BOARD;
__thread int REMAINING_MINES = 0;

__thread time_t START_TIME;
__thread time_t END_TIME;

request_t *REQUESTS = NULL;
request_t *LAST_REQUEST = NULL;

void authentication();
bool check_login(int new_connection);

void place_mines();
void adjacent_mines();
void send_tiles(int new_connection, leaderboard_t *player);
int tiles_xp(int x, int y, int count);
int tiles_xn(int x, int y, int count);
int tiles_yp(int x, int y, int count);
int tiles_yn(int x, int y, int count);
void send_mines(int new_connection);

void leaderboard(int new_connection);

void start_game(int new_connection);
void reset_game();
void receive_options(int new_connection, leaderboard_t *player);


void add_request(int request_number, int new_connection, pthread_mutex_t *p_mutex, pthread_cond_t *p_cond_var);
request_t *get_request(pthread_mutex_t *p_mutex);
void *requests_loop();

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

  for (int i = 0; i < BACKLOG; i++) {
    pthread_create(&THREAD_IDS[i], NULL, requests_loop, NULL);
  }

  while (1) {
    socket_length = sizeof(struct sockaddr);

    if ((new_connection = accept(socket_id, (struct sockaddr *)&client_address, &socket_length)) == -1) {
      perror("accept");
      continue;
    }

    printf("Server received a connection from %s\n", inet_ntoa(client_address.sin_addr));

    add_request(NUM_CLIENTS, new_connection, &REQUEST_MUTEX, &RECEIVED_REQUEST);
    NUM_CLIENTS += 1;
  }
}

void start_game(int new_connection) {
  if(check_login(new_connection)) {
    leaderboard_t *next_player = malloc(sizeof(leaderboard_t));
    strcpy(next_player->username, USERNAME);
    next_player->game_time = 0;
    next_player->games_played = 0;
    next_player->games_won = 0;
    next_player->next = LEADERBOARD;
    LEADERBOARD = next_player;

    receive_options(new_connection, next_player);
  }
  close(new_connection);
}

void reset_game() {
  BOARD = (game_state_t) {0};
  REMAINING_MINES = NUM_MINES;

  place_mines();
  adjacent_mines();
  time(&START_TIME);
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

bool check_login(int new_connection) {
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
      strcpy(USERNAME, DATABASE[i].username);
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
    if (strcmp(password, DATABASE[i].password) == 0) {
      has_password = 0;
      break;
    }
  }

  if (send(new_connection, &has_password, sizeof(int), 0) == -1) {
    perror("send");
    exit(1);
  }

  if (has_username == 0 && has_password == 0) {
    return true;
  }
  return false;
}

void place_mines() {
  for (int i = 0; i <= NUM_MINES; i++) {
    int x;
    int y;

    do {
      pthread_mutex_lock(&MUTEX);
      x = rand() % NUM_TILES_X;
      y = rand() % NUM_TILES_Y;
      pthread_mutex_unlock(&MUTEX);
    } while(BOARD.tiles[x][y].is_mine);
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

      if (BOARD.tiles[i + 1][j - 1].is_mine) {
        BOARD.tiles[i][j].adjacent_mines += 1;
      }

      if (BOARD.tiles[i - 1][j + 1].is_mine) {
        BOARD.tiles[i][j].adjacent_mines += 1;
      }
    }
  }
}

void receive_options(int new_connection, leaderboard_t *player) {
  int selection = 0;

  while(1) {
    if (recv(new_connection, &selection, sizeof(int), 0) == -1) {
      perror("recv");
      exit(1);
    }

    if (selection == 1) {
      reset_game();
      send_tiles(new_connection, player);
    } else if (selection == 2) {
      leaderboard(new_connection);
    }
  }
}

int tiles_xp(int x, int y, int count) {
  int xl = x + 1;

  if (xl < 9 && y >= 0 && y < 9) {
    count += 1;
    if (BOARD.tiles[xl][y].adjacent_mines == 0) {
      return tiles_xp(xl, y, count);
    }
  }
  return count;
}

int tiles_xn(int x, int y, int count) {
  int xl = x - 1;

  if (xl > 0 && y >= 0 && y < 9) {
    count += 1;
    if (BOARD.tiles[xl][y].adjacent_mines == 0) {
      return tiles_xn(xl, y, count);
    }
  }
  return count;
}

int tiles_yn(int x, int y, int count) {
  int yl = y - 1;

  if (yl > 0 && x >= 0 && x < 9) {
    count += 1;
    if (BOARD.tiles[x][yl].adjacent_mines == 0) {
      return tiles_yn(x, yl, count);
    }
  }
  return count;
}

int tiles_yp(int x, int y, int count) {
  int yl = y + 1;

  if (yl < 9 && x >= 0 && x < 9) {
    count += 1;
    if (BOARD.tiles[x][yl].adjacent_mines == 0) {
      return tiles_yp(x, yl, count);
    }
  }
  return count;
}

void send_tiles(int new_connection, leaderboard_t *player) {
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

    if (user_selection != 'Q') {
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
      } else if (BOARD.tiles[x][y].revealed) {
        tile_value = -2;
      } else {
        tile_value = BOARD.tiles[x][y].adjacent_mines;
      }
      BOARD.tiles[x][y].revealed = true;

      if (send(new_connection, &tile_value, sizeof(int), 0) == -1) {
        perror("send");
        exit(1);
      }
        
      if (tile_value == 0) {
        int count_xp = tiles_xp(x, y, 0);
        int count_xn = tiles_xn(x, y, 0);
        int count_yp = tiles_yp(x, y, 0);
        int count_yn = tiles_yn(x, y, 0);

        int count_dlp = tiles_yp(x - 1, y, 0);
        int count_dln = tiles_yn(x - 1, y, 0);
        int count_drp = tiles_yp(x + 1, y, 0);
        int count_drn = tiles_yn(x + 1, y, 0);

        if (send(new_connection, &count_xp, sizeof(int), 0) == -1) {
          perror("send");
          exit(1);
        }

        if (count_xp > 0) {
          for (int i = 0; i <= count_xp; i++) {
            BOARD.tiles[x + i][y].revealed = true;
            if (send(new_connection, &BOARD.tiles[x + i][y].adjacent_mines, sizeof(int), 0) == -1) {
              perror("send");
              exit(1);
            }
          }
        }

        if (send(new_connection, &count_xn, sizeof(int), 0) == -1) {
          perror("send");
          exit(1);
        }

        if (count_xn > 0) {
          for (int i = 0; i <= count_xn; i++) {
            BOARD.tiles[x - i][y].revealed = true;
            if (send(new_connection, &BOARD.tiles[x - i][y].adjacent_mines, sizeof(int), 0) == -1) {
              perror("send");
              exit(1);
            }
          }
        }

        if (send(new_connection, &count_yp, sizeof(int), 0) == -1) {
          perror("send");
          exit(1);
        }

        if (count_yp > 0) {
          for (int i = 0; i <= count_yp; i++) {
            BOARD.tiles[x][y + i].revealed = true;
            if (send(new_connection, &BOARD.tiles[x][y + i].adjacent_mines, sizeof(int), 0) == -1) {
              perror("send");
              exit(1);
            }
          }
        }

        if (send(new_connection, &count_yn, sizeof(int), 0) == -1) {
          perror("send");
          exit(1);
        }
        
        if (count_yn > 0) {
          for (int i = 0; i <= count_yn; i++) {
            BOARD.tiles[x][y - i].revealed = true;
            if (send(new_connection, &BOARD.tiles[x][y - i].adjacent_mines, sizeof(int), 0) == -1) {
              perror("send");
              exit(1);
            }
          }
        }

        if (send(new_connection, &count_dlp, sizeof(int), 0) == -1) {
          perror("send");
          exit(1);
        }

        if (count_dlp > 0) {
          for (int i = 0; i <= count_dlp; i++) {
            BOARD.tiles[x - 1][y + i].revealed = true;
            if (send(new_connection, &BOARD.tiles[x - 1][y + i].adjacent_mines, sizeof(int), 0) == -1) {
              perror("send");
              exit(1);
            }
          }
        }

        if (send(new_connection, &count_dln, sizeof(int), 0) == -1) {
          perror("send");
          exit(1);
        }

        if (count_dln > 0) {
          for (int i = 0; i <= count_dln; i++) {
            BOARD.tiles[x - 1][y - i].revealed = true;
            if (send(new_connection, &BOARD.tiles[x - 1][y - i].adjacent_mines, sizeof(int), 0) == -1) {
              perror("send");
              exit(1);
            }
          }
        }

        if (send(new_connection, &count_drp, sizeof(int), 0) == -1) {
          perror("send");
          exit(1);
        }

        if (count_drp > 0) {
          for (int i = 0; i <= count_drp; i++) {
            BOARD.tiles[x + 1][y + i].revealed = true;
            if (send(new_connection, &BOARD.tiles[x + 1][y + i].adjacent_mines, sizeof(int), 0) == -1) {
              perror("send");
              exit(1);
            }
          }
        }

        if (send(new_connection, &count_drn, sizeof(int), 0) == -1) {
          perror("send");
          exit(1);
        }

        if (count_drn > 0) {
          for (int i = 0; i <= count_drn; i++) {
            BOARD.tiles[x + 1][y - i].revealed = true;
            if (send(new_connection, &BOARD.tiles[x + 1][y - i].adjacent_mines, sizeof(int), 0) == -1) {
              perror("send");
              exit(1);
            }
          }
        }
      } else if (tile_value == -1) {
        if (user_selection == 'R') {
          send_mines(new_connection);
          pthread_mutex_lock(&MUTEX);
          player->games_played += 1;
          pthread_mutex_unlock(&MUTEX);
          receive_options(new_connection, player);
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

          pthread_mutex_lock(&MUTEX);
          player->games_played += 1;
          player->games_won += 1;
          player->game_time = game_duration;
          pthread_mutex_unlock(&MUTEX);
          receive_options(new_connection, player);
        }
      }

    } else {
      receive_options(new_connection, player);
    }
  }
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
  int num_players = 0;

  for (leaderboard_t *player = LEADERBOARD; player != NULL; player = player->next) {
    if (player->games_won > 0) {
      num_players += 1;
    }
  }

  if (send(new_connection, &num_players, sizeof(int), 0) == -1) {
    perror("send");
    exit(1);
  }

  for (leaderboard_t *player = LEADERBOARD; player != NULL; player = player->next) {
    if (player->games_won > 0) {
      if (send(new_connection, &player->username, 10, 0) == -1) {
        perror("send");
        exit(1);
      }

      if (send(new_connection, &player->game_time, sizeof(int), 0) == -1) {
        perror("send");
        exit(1);
      }

      if (send(new_connection, &player->games_won, sizeof(int), 0) == -1) {
        perror("send");
        exit(1);
      }

      if (send(new_connection, &player->games_played, sizeof(int), 0) == -1) {
        perror("send");
        exit(1);
      }
    }
  }
}

void add_request(int request_number, int new_connection, pthread_mutex_t *p_mutex, pthread_cond_t *p_cond_var) {
  request_t *new_request;

  new_request = (request_t *) malloc(sizeof(request_t));
  if (!new_request) {
    printf("Request: Out of memory\n");
    exit(1);
  }
  new_request->number = request_number;
  new_request->connection = new_connection;
  new_request->next = NULL;

  pthread_mutex_lock(p_mutex);

  if (NUM_REQUESTS == 0) {
    REQUESTS = new_request;
    LAST_REQUEST = new_request; 
  } else {
    LAST_REQUEST->next = new_request;
    LAST_REQUEST = new_request;
  }
  NUM_REQUESTS += 1;

  pthread_mutex_unlock(p_mutex);
  pthread_cond_signal(p_cond_var);
}

request_t *get_request(pthread_mutex_t *p_mutex) {
  request_t *new_request;

  pthread_mutex_lock(p_mutex);

  if (NUM_REQUESTS > 0) {
    new_request = REQUESTS;
    REQUESTS = new_request->next;

    if (REQUESTS == NULL) {
      LAST_REQUEST = NULL;
    }
    NUM_REQUESTS--;
  } else {
    new_request = NULL;
  }
  pthread_mutex_unlock(p_mutex);
  return new_request;
}

void *requests_loop() {
  request_t *new_request;

  pthread_mutex_lock(&REQUEST_MUTEX);

  while(1) {
    if (NUM_REQUESTS > 0) {
      if ((new_request = get_request(&REQUEST_MUTEX))) {
        pthread_mutex_unlock(&REQUEST_MUTEX);
        start_game(new_request->connection);
        free(new_request);
        pthread_mutex_lock(&REQUEST_MUTEX);
      }
    } else {
      pthread_cond_wait(&RECEIVED_REQUEST, &REQUEST_MUTEX);
    }
  }
}