from ast import Str
from ctypes import *      
from fsm.fsm import fsm_run
from fsm.fsm import Environment
from fsm.fsm import Transition
from fsm.fsm import DefaultStates
from enum import Enum

import socket       

# Create a socket object  
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM) 

debug = False

INVALID_REQUEST = 30
INVALID_UID = 31
INVALID_TYPE = 32
INVALID_CONTEXT = 33
INVALID_PAYLOAD = 34
INVALID_SERVER = 40
INVALID_ACTION = 50
ACTION_OUT_OF_TURN = 51

CONFIRMATION = 1
INFORMATION = 2
META_ACTION = 3
GAME_ACTION = 4

RULESET = 1
MAKE_MOVE = 1
QUIT = 1

SUCCESS = 10
UPDATE = 20

START_OF_GAME = 1
MOVE_MADE = 2
END_GAME = 3
OPPONENT_DC = 4

class States(Enum):
    SETUP = DefaultStates.STATE_START
    WAIT_SERVER = 2
    GET_GAME = 3
    MOVE = 4
    WAIT = 5
    UPDATE = 6
    ERROR = 7
    END = 8


class Tile:
    def __init__(self, tile_id, tile_owner) -> None:
        self.id = tile_id
        self.owner = tile_owner

class Globals(Environment):
    def __init__(self, f_state, t_state) -> None:
        super().__init__(f_state, t_state)
        self.id = -1
        self.setup = True
        self.move = -1
        self.sender = -1
        self.server_input = []
        self.board = []
        self.turn = False
        self.uid = [0, 0, 0, 0]
        self.win = 0
        for i in range(10):
            self.board.append(Tile(-4, -4))     


def setup(env):
    if(debug):
        print("\n========== setup state ==========")
    e = env        
  
    # Define the IP address and port on which you want to connect  
    local = '127.0.0.1'
    port = 2034

    # connect to the server on local computer  
    s.connect((local, port))  

    print("loading...")
    print("Connecting to server")

    # TODO HANDSHAKE HERE
    payLen = 2
    protocolVer = 1
    gId = 1
    reqType = CONFIRMATION
    reqContext = RULESET

    packet = [e.uid[0], e.uid[1], e.uid[2], e.uid[3], reqType, reqContext, payLen, protocolVer, gId]
    s.sendall(bytearray(packet))

    return States.WAIT_SERVER

def set_uid(env):
    inp = s.recv(7)
    int_values = [x for x in inp]
    
    msg_type = int_values[0]
    int_values.pop(0)
    if(msg_type != SUCCESS):
        return States.ERROR
        
    context = int_values[0]
    int_values.pop(0)
    if(context != CONFIRMATION):
        return States.ERROR

    payload_len = int_values[0]
    int_values.pop(0)
    if(payload_len != 4):
        return States.ERROR

    for i in range(4):
        env.uid[i] = int_values[0]
        int_values.pop(0)
    
    env.server_input = int_values

    return States.GET_GAME

def get_game(env):
    if(debug):
        print("\n========== get game state ==========")
    
    inp = s.recv(4)
    int_values = [x for x in inp]

    dtype = int_values[0]
    int_values.pop(0)
    if(dtype != UPDATE):
        return States.ERROR
    
    dcontext = int_values[0]
    int_values.pop(0)
    if(dcontext != START_OF_GAME):
        return States.ERROR

    dpaylen = int_values[0]
    int_values.pop(0)
    if(dpaylen != 1):
        return States.ERROR

    dpayload = int_values[0]
    int_values.pop(0)
    if(dpayload == 1):
        env.id = dpayload
        env.turn = True
        return States.MOVE

    if(dpayload == 2):
        env.id = dpayload
        env.turn = False
        return States.WAIT

    return States.ERROR

def move(env):
    if(debug):
        print("\n========== move state ==========")

    cinput = input("Input move: ")

    payLen = 1
    reqType = GAME_ACTION
    reqContext = MAKE_MOVE
    
    packet = [env.uid[0], env.uid[1], env.uid[2], env.uid[3], reqType, reqContext, payLen, int(cinput)]
    s.sendall(bytearray(packet))

    return States.WAIT

def wait(env):
    if(debug):
        print("\n========== wait state ==========")

    inp = s.recv(5)
    int_values1 = [x for x in inp]
    if(debug):
        print(int_values1)
    
    # DISGUSTING
    if(int_values1[0] == INVALID_ACTION):
        return States.MOVE
    elif(int_values1[0] == UPDATE):
        if(int_values1[1] == MOVE_MADE):
            if(int_values1[2] == 1):
                env.move = int_values1[3]
                return States.UPDATE
            else:
                return States.ERROR
        if(int_values1[1] == END_GAME):
            if(int_values1[2] == 2):
                env.win = int_values1[3]
                env.move = int_values1[4]
                return States.END
            else:
                return States.ERROR
        else:
            States.ERROR
    else:
        States.ERROR

def update(env):
    env.board[env.move].owner = ((1,2) [env.id == 1] , env.id) [env.turn]
    if (debug):
        print("update move")

    # draw board
    for i in range(9):
        c = "?"

        if(env.board[i].owner == 1):
            c = "O"
        elif (env.board[i].owner == 2):
            c = "X"

        print(c, end='')
        if (i == 2 or i == 5 or i == 8):
            print('\n', end='')

    if(not env.turn):
        if (debug):
            print("update move")
        env.turn = not env.turn
        return States.MOVE
    else:
        if (debug):
            print("update wait")
        env.turn = not env.turn
        return States.WAIT

def end(env):
    env.board[env.move].owner = ((1,2) [env.id == 1] , env.id) [env.turn]

    # draw board
    for i in range(9):
        c = "?"

        if(env.board[i].owner == 1):
            c = "O"
        elif (env.board[i].owner == 2):
            c = "X"

        print(c, end='')
        if (i == 2 or i == 5 or i == 8):
            print('\n', end='')

    if(env.win == 1):
        print("You Win")
    elif(env.win == 2):
        print("You Lose")
    else:
        print("Draw")

    return DefaultStates.STATE_NULL

def common_error(env):
    print("error found")
    return DefaultStates.STATE_NULL

def main():
    print("[TIC TAC TOE] by Brian and Kurt")

    tran_table = [
        Transition(DefaultStates.STATE_INIT, States.SETUP, setup),
        Transition(States.SETUP, States.WAIT_SERVER, set_uid),
        Transition(States.SETUP, States.ERROR, common_error),
        Transition(States.WAIT_SERVER, States.GET_GAME, get_game),
        Transition(States.GET_GAME, States.MOVE, move),
        Transition(States.GET_GAME, States.WAIT, wait),
        Transition(States.MOVE, States.WAIT, wait),
        Transition(States.WAIT, States.MOVE, move),
        Transition(States.WAIT, States.UPDATE, update),
        Transition(States.WAIT, States.ERROR, common_error),
        Transition(States.UPDATE, States.MOVE, move),
        Transition(States.UPDATE, States.WAIT, wait),
        Transition(States.WAIT, States.END, end),
        Transition(States.END, DefaultStates.STATE_NULL, None),
        Transition(States.END, States.ERROR, common_error),
        Transition(DefaultStates.STATE_NULL, DefaultStates.STATE_NULL, None),
    ]

    glbl = Globals(DefaultStates.STATE_INIT, States.SETUP)
    start_state = DefaultStates.STATE_INIT
    next_state = States.SETUP
    fsm_run(glbl, start_state, next_state, tran_table)

main()
s.close()