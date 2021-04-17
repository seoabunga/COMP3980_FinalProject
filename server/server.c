#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

//
#define MAX_PLAYER_COUNT 1024
#define MAX_GAMES 10
#define BUFF_SIZE 1024
#define DEBUG 1
#define PORT 2034
#define NUM_GAME_TYPES 2
#define PROTO_VER 1
// Structs

typedef enum RequestType
{
    CONFIRMATION = 1,
    INFORMATION = 2,
    META_ACTION = 3,
    GAME_ACTION = 4,
} RequestType;

typedef enum RequestContext
{
    RULESET = 1,
    MAKE_MOVE = 1,
    QUIT = 1,
} RequestContext;

typedef enum Status
{
    SUCCESS = 10,
    UPDATE = 20,
    INVALID_REQUEST = 30,
    INVALID_UID = 31,
    INVALID_TYPE = 32,
    INVALID_CONTEXT = 33,
    INVALID_PAYLOAD = 34,
    SERVER_ERROR = 40,
    INVALID_ACTION = 50,
    ACTION_OUT_OF_TURN = 51,
} Status;

typedef enum ResponseContext
{
    START_GAME = 1,
    MOVE_MADE = 2,
    END_OF_GAME = 3,
    DISCONNECT = 4,
} ResponseContext;

typedef enum GameId
{
    TTT = 1,
    RPS = 2,
} GameId;

typedef enum RPSMove
{
    ROCK = 1,
    PAPER,
    SCISSORS
} RPSMove;

typedef enum TTTMove
{
    X = 1,
    O
} TTTMove;

typedef struct WaitList
{
    int player;
    int gameType;
} WaitList;

typedef struct Game
{
    int type;

    uint8_t p1uid[4];
    uint8_t p2uid[4];
    int fdp1;
    int fdp2;

    int num_bytes;
    uint8_t buff[BUFF_SIZE];

    int rps_p1_move;
    int rps_p2_move;

    int ttt_board[9];
    int ttt_turn;
} Game;

// Function Proto

//      general function proto
void send_to_client(int client, int nbytes_recvd, uint8_t *recv_buf, fd_set *master);
void send_recv(int client, fd_set *master, WaitList **waiting, Game **games, int *numrooms);
void send_uid(int client);
int init_verification(uint8_t reqtype, uint8_t context, uint8_t paylen, uint8_t *payload);
//      struct functions
void init_waiting(WaitList **waiting);
void add_to_wait(WaitList **waiting, int client, int game);
void remove_from_wait(WaitList **waiting, int client);
int find_pair(WaitList **waiting, GameId game_id);
void init_games(Game **games);
void init_game(Game *g, int type, int fdp1, int fdp2);
int get_game_pair(Game **game, int play_fd);
int get_player_game(Game **game, int play_fd);
void create_game(Game **games, int fdp1, int fdp2, int game);
void end_game(Game **games, int game_index);
void not_your_turn(int client);
int handle_ttt(Game *game, int client, uint8_t *buff);
int handle_rps(Game *game, int client, uint8_t *buff);
static int checkWinner(int board[3][3]);
static int equals3(int a, int b, int c);
static int rps_compare(int move1, int move2);
// Function Definitions

void send_to_client(int client, int nbytes_recvd, uint8_t *recv_buf, fd_set *master)
{
    if (FD_ISSET(client, master))
    {
        if (write(client, recv_buf, nbytes_recvd) == -1)
        {
            perror("write");
        }
    }
}

void send_uid(int client)
{
    int uid = client;
    uint8_t send_uid[] = {
        (uid >> 24) & 0xFF,
        (uid >> 16) & 0xFF,
        (uid >> 8) & 0xFF,
        uid & 0xFF,
    };

    uint8_t res[] =
        {
            SUCCESS,
            CONFIRMATION,
            4,
            send_uid[0],
            send_uid[1],
            send_uid[2],
            send_uid[3],
        };

    write(client, res, 7);
}

int init_verification(uint8_t reqtype, uint8_t context, uint8_t paylen, uint8_t *payload)
{
    for (int j = 0; j < 9; j++)
        if (DEBUG)
            printf("data %d %d\n", j, payload[j]);

    if ((uint32_t)reqtype != CONFIRMATION)
        return INVALID_TYPE;
    if ((uint32_t)context != RULESET)
        return INVALID_CONTEXT;

    if ((uint32_t)paylen < 2)
        return INVALID_PAYLOAD;

    int i = 7;
    if ((uint32_t)payload[i++] != PROTO_VER)
        return INVALID_PAYLOAD;

    if ((uint32_t)payload[i] == TTT || (uint32_t)payload[i] == RPS)
        return SUCCESS;

    return INVALID_PAYLOAD;
}

void connection_accept(fd_set *master, int *fdmax, int sockfd, struct sockaddr_in *client_addr, WaitList **waiting, Game **games)
{
    socklen_t addrlen;
    int newsockfd;

    addrlen = sizeof(struct sockaddr_in);
    if ((newsockfd = accept(sockfd, (struct sockaddr *)client_addr, &addrlen)) == -1)
    {
        perror("accept");
        exit(1);
    }
    else
    {
        uint8_t buff[BUFF_SIZE];
        int retval = 0;
        retval = read(newsockfd, buff, BUFF_SIZE);
        if (retval == 0)
        {
            perror("read");
            exit(1);
        }
        else
        {
            retval = init_verification(buff[4], buff[5], buff[6], buff);
            if (retval == SUCCESS)
            {
                // return UID
                send_uid(newsockfd);
                FD_SET(newsockfd, master);
                if (newsockfd > *fdmax)
                {
                    *fdmax = newsockfd;
                }

                int pair = 0;
                if (DEBUG)
                    printf("pair %d\n", pair);
                int gameType = buff[8];
                if (DEBUG)
                    printf("gametype %d\n", gameType);
                if ((pair = find_pair(waiting, gameType)) == 0)
                {
                    add_to_wait(waiting, newsockfd, gameType);
                }
                else
                {
                    if (DEBUG)
                        printf("new game created between player %d and player %d\n", newsockfd, pair);
                    create_game(games, newsockfd, pair, gameType);
                    remove_from_wait(waiting, pair);
                }

                if (DEBUG)
                    printf("new connection from %s on port %d \n", inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
            }
            else
            {
                // return handshake error
                printf("return error %d\n", retval);
                uint8_t res[] = {
                    retval,
                    CONFIRMATION,
                    0};
                write(newsockfd, res, 3);
            }
        }
    }
}

void connect_request(int *sockfd, struct sockaddr_in *my_addr)
{
    int yes = 1;

    if ((*sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Socket TCP error");
        exit(1);
    }

    my_addr->sin_family = AF_INET;
    my_addr->sin_port = htons(PORT);
    my_addr->sin_addr.s_addr = INADDR_ANY;
    memset(my_addr->sin_zero, 0, sizeof my_addr->sin_zero);

    if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
        perror("setsockopt");
        exit(1);
    }

    if (bind(*sockfd, (struct sockaddr *)my_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("Unable to bind");
        exit(1);
    }
    if (listen(*sockfd, 10) == -1)
    {
        perror("listen");
        exit(1);
    }

    bind(*sockfd, (struct sockaddr *)my_addr, sizeof(struct sockaddr));

    printf("\nServer: Waiting for client on port %d\n", PORT);
    fflush(stdout);
}

int handle_ttt(Game *game, int client, uint8_t *buff)
{
    if (DEBUG)
        printf("handling ttt\n");
    // type
    int reqType = buff[4];

    if (reqType != GAME_ACTION)
        return INVALID_TYPE;

    // context
    int context = buff[5];
    if (context != MAKE_MOVE)
        return INVALID_CONTEXT;

    // len
    int paylen = buff[6];
    if (paylen != 1)
        return INVALID_PAYLOAD;

    // move
    int move = buff[7];
    if (DEBUG)
        printf("move %d\n", move);
    if (move > 8 || move < 0)
        return INVALID_ACTION;

    if (DEBUG)
        printf("board move %d\n", game->ttt_board[move]);
    if (game->ttt_board[move] != -1)
        return INVALID_ACTION;

    (*game).ttt_board[move] = (*game).ttt_turn;

    // change turn
    if (DEBUG)
        printf("change turn\n");
    (*game).ttt_turn = (*game).ttt_turn == O ? X : O;

    for (int i = 0; i < 10; i++)
    {
        if (i == 9)
        {
            // DRAW
            int payload_len = 2;
            uint8_t drawres[] = {
                UPDATE,
                END_OF_GAME,
                payload_len,
                3,
                move};

            write((*game).fdp1, drawres, 5);
            write((*game).fdp2, drawres, 5);
        }

        if ((*game).ttt_board[i] == -1)
        {
            break;
        }
    }

    int board[3][3];

    int k = 0;
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            board[i][j] = (*game).ttt_board[k++];
        }
    }

    if (DEBUG)
        printf("check winner\n");
    // Check Win
    int winner;
    if ((winner = checkWinner(board)) != -1)
    {
        int payload_len = 2;
        uint8_t winres[] = {
            UPDATE,
            END_OF_GAME,
            payload_len,
            1,
            move};

        uint8_t loseres[] = {
            UPDATE,
            END_OF_GAME,
            payload_len,
            2,
            move};
        if (winner == 1)
        {
            write((*game).fdp2, winres, 5);
            write((*game).fdp1, loseres, 5);
        }
        else
        {
            write((*game).fdp1, winres, 5);
            write((*game).fdp2, loseres, 5);
        }
        return UPDATE;
    }

    int payload_len = 1;
    uint8_t moveres[] = {
        UPDATE,
        MOVE_MADE,
        payload_len,
        move};

    write((*game).fdp1, moveres, 5);
    write((*game).fdp2, moveres, 5);

    return UPDATE;
}

void send_recv(int client, fd_set *master, WaitList **waiting, Game **games, int *numrooms)
{
    if (DEBUG)
        printf("client %d read\n", client);
    int num_bytes;
    uint8_t buff[BUFF_SIZE];
    if ((num_bytes = read(client, buff, BUFF_SIZE)) <= 0)
    {
        if (DEBUG)
            printf("numbytes %d\n", num_bytes);
        int game_pair;
        int game;
        if (num_bytes == 0)
        {
            game_pair = get_game_pair(games, client);
            game = get_player_game(games, client);
            if (game_pair == -1)
            {
                remove_from_wait(waiting, client);
            }
            else
            {
                end_game(games, game);
                int quitMessageLength = 3;
                uint8_t quitPayloadLength = 0;

                uint8_t quit_message[] = {
                    UPDATE,
                    DISCONNECT,
                    quitPayloadLength};

                write(game_pair, quit_message, 3);
            }
        }
        printf("client %d disconnected\n", client);
        close(client);
        FD_CLR(client, master);
    }
    else
    {
        uint32_t converted_uid = (buff[1] << 24) | (buff[1] << 16) | (buff[2] << 8) | buff[3];
        printf("converted uid %d\n", converted_uid);
        // Read client move
        int game = get_player_game(games, client);
        if ((*games)[game].type == TTT)
        {
            if (DEBUG)
            {
                printf("converted uid %d\n", converted_uid);
                printf("client %d send data\n", client);
            }
            printf("client %d send data\n", client);
            int rettype = -1;
            if ((*games)[game].ttt_turn == 1)
            {
                if (DEBUG)
                    printf("handle TTT 1\n");
                if (converted_uid == client)
                {
                    int paylen = buff[6];
                    if (paylen > 0)
                        rettype = handle_ttt(&((*games)[game]), client, buff);

                    if (rettype != UPDATE)
                    {
                        int payload_length = 2;
                        uint8_t payload[] = {
                            rettype,
                            0};
                        write(client, payload, payload_length);
                    }
                }
                else
                {
                    if (DEBUG)
                        printf("not your turn 1\n");
                    not_your_turn(client);
                }
            }
            else
            {
                if (DEBUG)
                    printf("handle TTT 2\n");
                if (converted_uid == client)
                {
                    int paylen = buff[6];
                    if (paylen > 0)
                        rettype = handle_ttt(&((*games)[game]), client, buff);

                    if (rettype != UPDATE)
                        not_your_turn(client);
                }
                else
                {
                    if (DEBUG)
                        printf("not your turn 2\n");
                    not_your_turn(client);
                }
            }

            for (int i = 0; i < num_bytes; i++)
            {
                if (DEBUG)
                    printf("client %d data[%d] : %d\n", client, i, (uint32_t)buff[i]);
            }
        }
        else if ((*games)[game].type == RPS)
        {
            if (DEBUG)
                printf("handling rps\n");

            if (converted_uid == client)
            {
                if ((*games)[game].fdp1 == client)
                {
                    if (buff[6] != 1)
                    {
                        uint8_t payload[] = {
                            INVALID_PAYLOAD,
                            0};
                        write(client, payload, 2);
                        return;
                    }
                    if (buff[7] == ROCK || buff[7] == PAPER || buff[7] == SCISSORS)
                    {
                        (*games)[game].rps_p1_move = buff[7];
                        uint8_t payload[] = {
                            SUCCESS,
                            GAME_ACTION,
                            0};
                        write(client, payload, 3);
                    }
                    else
                    {
                        uint8_t payload[] = {
                            INVALID_PAYLOAD,
                            0};
                        write(client, payload, 2);
                        return;
                    }
                }
                else
                {
                    if (buff[6] != 1)
                    {
                        uint8_t payload[] = {
                            INVALID_PAYLOAD,
                            0};
                        write(client, payload, 2);
                        return;
                    }

                    if (buff[7] == ROCK || buff[7] == PAPER || buff[7] == SCISSORS)
                    {
                        (*games)[game].rps_p2_move = buff[7];
                        uint8_t payload[] = {
                            SUCCESS,
                            GAME_ACTION,
                            0};
                        write(client, payload, 3);
                    }
                    else
                    {
                        uint8_t payload[] = {
                            INVALID_PAYLOAD,
                            0};
                        write(client, payload, 2);
                        return;
                    }
                }

                if ((*games)[game].rps_p2_move != 0 && (*games)[game].rps_p1_move != 0)
                {
                    int winner = rps_compare((*games)[game].rps_p1_move, (*games)[game].rps_p2_move);

                    if (winner == 3)
                    {
                        int payload_len = 1;
                        uint8_t drawres[] = {
                            UPDATE,
                            END_OF_GAME,
                            payload_len,
                            3};
                        write((*games)[game].fdp1, drawres, 4);
                        write((*games)[game].fdp2, drawres, 4);
                        printf("draw rps");
                    }
                    else
                    {
                        int payload_len = 2;

                        if (winner == 1)
                        {
                            uint8_t winres[] = {
                                UPDATE,
                                END_OF_GAME,
                                payload_len,
                                1,
                                (*games)[game].rps_p2_move
                            };

                            uint8_t loseres[] = {
                                UPDATE,
                                END_OF_GAME,
                                payload_len,
                                2,
                                (*games)[game].rps_p1_move
                                };
                            printf("win p1");
                            write((*games)[game].fdp1, winres, 4);
                            write((*games)[game].fdp2, loseres, 4);
                        }
                        else
                        {
                            uint8_t winres[] = {
                                UPDATE,
                                END_OF_GAME,
                                payload_len,
                                1,
                                (*games)[game].rps_p1_move
                            };

                            uint8_t loseres[] = {
                                UPDATE,
                                END_OF_GAME,
                                payload_len,
                                2,
                                (*games)[game].rps_p2_move
                                };
                            printf("win p2");
                            write((*games)[game].fdp1, loseres, 4);
                            write((*games)[game].fdp2, winres, 4);
                        }
                    }
                }
            }

            for (int i = 0; i < num_bytes; i++)
            {
                if (DEBUG)
                    printf("client %d data[%d] : %d\n", client, i, (uint32_t)buff[i]);
            }
        }
    }
}

//      struct function definition
void init_waiting(WaitList **waiting)
{
    *waiting = malloc(sizeof(WaitList) * NUM_GAME_TYPES);
    if (*waiting == 0)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < NUM_GAME_TYPES; i++)
    {
        (*waiting)[i].player = 0;
        (*waiting)[i].gameType = 0;
    }
}

int find_pair(WaitList **waiting, GameId game_id)
{
    for (int i = 0; i < NUM_GAME_TYPES; i++)
    {
        if ((*waiting)[i].gameType == game_id)
        {
            return (*waiting)[i].player;
        }
    }
    return 0;
}

void add_to_wait(WaitList **waiting, int client, int game)
{
    for (int i = 0; i < NUM_GAME_TYPES; i++)
    {
        if ((*waiting)[i].player == 0)
        {
            if (DEBUG)
                printf("client %d added to waiting %d for game %d\n", client, i, game);
            (*waiting)[i].player = client;
            (*waiting)[i].gameType = game;
            break;
        }
    }
}

void remove_from_wait(WaitList **waiting, int client)
{
    for (int i = 0; i < NUM_GAME_TYPES; i++)
    {
        if ((*waiting)[i].player == client)
        {
            if (DEBUG)
                printf("client %d remove to waiting %d\n", client, i);
            (*waiting)[i].player = 0;
            (*waiting)[i].gameType = 0;
            break;
        }
    }
}

void init_games(Game **games)
{
    *games = malloc(sizeof(Game) * MAX_GAMES);
    if (*games == 0)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < MAX_GAMES; i++)
    {
        init_game(&(*games)[i], 0, 0, 0);
    }
}

void init_game(Game *g, int type, int fdp1, int fdp2)
{
    g->type = type;
    g->fdp1 = fdp1;
    g->fdp2 = fdp2;

    if (type == TTT)
    {
        for (int i = 0; i < 9; i++)
        {
            (*g).ttt_board[i] = -1;
            if (DEBUG)
                printf("board %d\n", g->ttt_board[i]);
        }
        g->ttt_turn = O;
    }
    else if (type == RPS)
    {
        g->rps_p1_move = 0;
        g->rps_p2_move = 0;
    }

    int uid1 = fdp1;
    uint8_t client_uid1[] = {
        (uid1 >> 24) & 0xFF,
        (uid1 >> 16) & 0xFF,
        (uid1 >> 8) & 0xFF,
        uid1 & 0xFF,
    };

    int uid2 = fdp2;
    uint8_t client_uid2[] = {
        (uid2 >> 24) & 0xFF,
        (uid2 >> 16) & 0xFF,
        (uid2 >> 8) & 0xFF,
        uid2 & 0xFF,
    };

    *g->p1uid = client_uid1;
    *g->p2uid = client_uid2;
}

int get_game_pair(Game **game, int play_fd)
{
    for (int i = 0; i < MAX_GAMES; i++)
    {
        if ((*game)[i].fdp1 == play_fd)
        {
            return (*game)[i].fdp2;
        }
        else if ((*game)[i].fdp2 == play_fd)
        {
            return (*game)[i].fdp1;
        }
    }

    return -1;
}

int get_player_game(Game **game, int play_fd)
{
    for (int i = 0; i < MAX_GAMES; i++)
    {
        if ((*game)[i].fdp1 == play_fd || (*game)[i].fdp2 == play_fd)
        {
            return i;
        }
    }

    return -1;
}

void create_game(Game **games, int fdp1, int fdp2, int game)
{
    for (int i = 0; i < MAX_GAMES; i++)
    {
        if ((*games)[i].type == 0)
        {
            init_game(&(*games)[i], game, fdp1, fdp2);
            break;
        }
    }

    if (game == TTT)
    {
        uint8_t team_meslen = 4;
        uint8_t team_paylen = 1;
        uint8_t player1 = 1;
        uint8_t player2 = 2;
        uint8_t team1_message[] = {
            UPDATE,
            START_GAME,
            team_paylen,
            player1};

        uint8_t team2_message[] = {
            UPDATE,
            START_GAME,
            team_paylen,
            player2};

        write(fdp1, team1_message, team_meslen);
        write(fdp2, team2_message, team_meslen);
    }
    else if (game == RPS)
    {
        uint8_t team1_message[] = {
            UPDATE,
            START_GAME,
            0};

        uint8_t team2_message[] = {
            UPDATE,
            START_GAME,
            0};

        printf("created RPS game\n");

        write(fdp1, team1_message, 3);
        write(fdp2, team2_message, 3);
    }
}

void end_game(Game **games, int game_index)
{
    for (int i = 0; i < MAX_GAMES; i++)
    {
        if (i == game_index)
        {
            init_game(&(*games)[i], 0, 0, 0);
            break;
        }
    }
}

void not_your_turn(int client)
{
    int payload_length = 2;
    uint8_t payload[] = {
        INVALID_ACTION,
        0};
    write(client, payload, payload_length);
}

static int checkWinner(int board[3][3])
{
    int winner = -1;

    // vertical
    for (int i = 0; i < 3; i++)
    {
        if (equals3(board[i][0], board[i][1], board[i][2]))
            winner = board[i][0];
    }

    // horizontal
    for (int i = 0; i < 3; i++)
    {
        if (equals3(board[0][i], board[1][i], board[2][i]))
            winner = board[i][0];
    }

    // diagonal
    for (int i = 0; i < 3; i++)
    {
        if (equals3(board[0][0], board[1][1], board[2][2]))
            winner = board[0][0];
    }

    for (int i = 0; i < 3; i++)
    {
        if (equals3(board[2][0], board[1][1], board[0][2]))
            winner = board[2][0];
    }

    return winner;
}

static int equals3(int a, int b, int c)
{
    return (a == b && b == c && a != -1);
}

static int rps_compare(int move1, int move2)
{
    if (move1 == ROCK && move2 == PAPER)
    {
        return 2;
    }
    else if (move1 == PAPER && move2 == ROCK)
    {
        return 1;
    }

    if (move1 == PAPER && move2 == SCISSORS)
    {
        return 2;
    }
    else if (move1 == SCISSORS && move2 == PAPER)
    {
        return 1;
    }

    if (move1 == SCISSORS && move2 == ROCK)
    {
        return 2;
    }
    else if (move1 == ROCK && move2 == SCISSORS)
    {
        return 1;
    }

    return 3;
}
// Main Function

int main()
{
    fd_set master;
    fd_set read_fds;
    int fd_max, i;
    int server_fd = 0;
    struct sockaddr_in my_addr, client_addr;

    int retval;
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    connect_request(&server_fd, &my_addr);
    FD_SET(server_fd, &master);
    fd_max = server_fd;

    int numrooms = 0;

    // holds the game environments
    WaitList *waiting;
    init_waiting(&waiting);

    Game *games;
    init_games(&games);

    while (1)
    {
        read_fds = master;

        if ((retval = select(fd_max + 1, &read_fds, NULL, NULL, NULL)) == -1)
        {
            perror("select");
            exit(EXIT_FAILURE);
        }

        for (i = 0; i <= fd_max; i++)
        {
            if (FD_ISSET(i, &read_fds))
            {
                if (i == server_fd)
                {
                    connection_accept(&master, &fd_max, server_fd, &client_addr, &waiting, &games);
                }
                else
                {
                    send_recv(i, &master, &waiting, &games, &numrooms);
                }
            }
        }
    }
}