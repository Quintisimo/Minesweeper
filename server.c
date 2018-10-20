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

#define MAXDATASIZE 100
#define BACKLOG 10
#define RANDOM_NUMBER_SEED 42
#define NUM_TILES_X 9
#define NUM_TILES_Y 9

struct User {
	char *username;
	char *password;
} *users;

struct LeaderBoard {
	char *username;
	int games_won;
	int games_played;
} *leaderBoard = NULL;

// LEADER BOARD 
int user_count = 0;
int entry_count = 0;
char buf[MAXDATASIZE];
int add_leaderboard_entry(char *name);
int add_win_for(char *name);
int add_game_played(char *name);

typedef struct {
  char username[10];
  char password[10];
} Auth;

// tile state
typedef struct {
  int adjacent_mines;
  bool revealed;
  bool is_mine;
} Tile;

// grid
typedef struct {
  Tile tiles[NUM_TILES_X][NUM_TILES_Y];
} GameState;

// typedef struct request request_t;
// struct request {
//   int number;
//   int sockfd;
//   request_t* next;
// };

const int NUM_MINES = 10;
Auth DATABASE[BACKLOG];
GameState BOARD = {};

// struct request *requests = NULL;
// struct request *last_request = NULL;

void authentication();
void check_login(int new_connection);
void place_mines();
void adjacent_mines();
void send_tiles(int socket_id);
int leader_board(int new_connection);

int main(int argc, char const *argv[]) {
  int socket_id, new_connection;
  struct sockaddr_in server_address;
  struct sockaddr_in client_address;
  socklen_t socket_length;

  srand(RANDOM_NUMBER_SEED);

  if (argc != 2) {
    fprintf(stderr, "Please enter a port number\n");
    exit(1);
  }
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(atoi(argv[1]));
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

  printf("Server is listening on %d\n", atoi(argv[1]));

  while (1) {
    socket_length = sizeof(struct sockaddr);

    if ((new_connection = accept(socket_id, (struct sockaddr *)&client_address, &socket_length)) == -1) {
      perror("accept");
      continue;
    }

    printf("Server received a connection from %s\n", inet_ntoa(client_address.sin_addr));

    if (!fork()) {
      check_login(new_connection);
      place_mines();
      adjacent_mines();
      send_tiles(new_connection);
      // add_leaderboard_entry(username);
      // add_win_for(username);
      // add_game_played(username);
      close(new_connection);
      exit(0);
    }
    close(new_connection);

    while (waitpid(-1, NULL, WNOHANG) > 0);
  }
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
      // add_leaderboard_entry(username);
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

bool tile_contains_mine(int x, int y) {
  // bool contains_mine;

  // for (int i = 0; i < NUM_TILES_X; i++) {
  //   for (int j = 0; j < NUM_TILES_Y; j++) {
  //     if ((x == i) && (y == j)) { 
  //       contains_mine = true;
  //     } else {
  //       contains_mine = false;
  //     }
  //   }
  // }
  // return contains_mine;
  return BOARD.tiles[x][y].is_mine;
}

void place_mines() {
  for (int i = 0; i < NUM_MINES; i++) {
    int x;
    int y;

    do {
      x = rand() % NUM_TILES_X;
      y = rand() % NUM_TILES_Y;
    // } while (tile_contains_mine(x,y));
    } while(BOARD.tiles[x][y].is_mine);
    // place mine at (x, y)
    BOARD.tiles[x][y].is_mine = true;
    // for (int i = 0; i < NUM_TILES_X; i++) {
    //   for (int j = 0; j < NUM_TILES_Y; j++) {
    //     if (tile_contains_mine(x,y)) {
    //       printf("*");
    //     }
    //   }
    // }
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

// void minesweeperLoop(int new_connection) {
//   char buf[MAXDATASIZE];

//   // place_mines();

//   // Send the placed mines to the client
//   if (send(new_connection, buf, sizeof(buf), 0) == -1) {
//     close(new_connection);
//     perror("send");
//   }
// }

void send_tiles(int new_connection) {
  int x = 0;
  int y = 0;
  int tile_value = 0;

  while(1) {
    if (recv(new_connection, &x, sizeof(int), 0) == -1) {
      perror("recv");
      exit(1);
    }

    if (recv(new_connection, &y, sizeof(int), 0) == -1) {
      perror("recv");
      exit(1);
    }
    
    // printf("x:%d, y:%d\n", x, y);
    // printf("%d", board.tiles[x][y].is_mine);
    if (BOARD.tiles[x][y].is_mine) {
      tile_value = -1;
    } else {
      tile_value = BOARD.tiles[x][y].adjacent_mines;
      BOARD.tiles[x][y].revealed = true;
    }
    printf("%d", tile_value);

    if (send(new_connection, &tile_value, sizeof(int), 0) == -1) {
      perror("send");
      exit(1);
    }
  }
}

// void leaderboard(int new_connection) {
//   int played = games_played;
//   int won = games_won;
//   int timer = time_taken;
  
//   if (send(new_connection, &played, sizeof(int), 0) == -1) {
//     perror("send");
//     exit(1);
//   } 

//   if (send(new_connection, &won, sizeof(int), 0) == -1) {
//     perror("send");
//     exit(1);
//   } 

//   if (send(new_connection, &timer, sizeof(int), 0) == -1) {
//     perror("send");
//     exit(1);
//   } 
// }

int addWinFor(char *name){

	for (int i = 0; i < user_count; i++){
		if(strcmp(leaderBoard[i].username, name) == 0){
			leaderBoard[i].games_won++;
			leaderBoard[i].games_played++;

			return 1;
		}
	}
	return 0;
}

int add_game_played(char *name){

	for (int i = 0; i < user_count; i++){
		if(strcmp(leaderBoard[i].username, name) == 0){
			leaderBoard[i].games_played++;

			return 1;
		}
	}

	return 0;
}

int add_leaderboard_entry(char *name){

	// Check to see if the user already exists in the data store
	for (int i = 0; i < user_count; i++){
		if(strcmp(leaderBoard[i].username, name) == 0){
		// they're the same

		return -1;
		}
	}

	user_count++;
	leaderBoard = realloc(leaderBoard, user_count * sizeof(struct LeaderBoard));
	leaderBoard[user_count - 1].username = malloc(strlen(name) + 1);
	strcpy(leaderBoard[user_count - 1].username, name);
	leaderBoard[user_count - 1].games_won = 0;
	leaderBoard[user_count - 1].games_played = 0;

	return 1; // return success
}

int leader_board(int new_connection){

	for (int i = 0; i < user_count; i++){
		sprintf(buf, "%s&%d&%d", leaderBoard[i].username, leaderBoard[i].games_played, leaderBoard[i].games_won);

		if (send(new_connection, buf, sizeof buf, 0) == -1) { 
			perror("recv");
      exit(1);
		}
	}

	if (send(new_connection, "lb-end", sizeof("lb-end"), 0) == -1) { 
		perror("recv");
    exit(1);
	}

	return 1;
}