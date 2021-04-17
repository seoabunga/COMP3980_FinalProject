from enum import Enum

class DefaultStates(Enum):
    STATE_NULL = -1
    STATE_INIT = 0
    STATE_EXIT = 1
    STATE_START = 2

class Environment:
    def __init__(self, f_state, t_state) -> None:
        self.from_state = f_state
        self.to_state = t_state

class Transition:
    def __init__(self, f_state, t_state, act) -> None:
        """Transition Class

        Args:
            f_state (State): from state
            t_state (State): to state
            act (def): action of the transition
        """
        self.from_state = f_state
        self.to_state = t_state
        self.action = act

def fsm_run(env, curr_state, next_state, tran_table):
    c_state = curr_state
    n_state = next_state

    while(True):
        perform = fsm_transition(c_state, n_state, tran_table)

        if(perform == None):
            exit()

        env.from_state = c_state
        env.to_state = n_state
        c_state = n_state
        n_state = perform(env)

def fsm_transition(from_state, to_state, tran_table):
    i = 0
    transition = tran_table[i]
    while(transition.action != None):
        if(transition.from_state == from_state and transition.to_state == to_state):
            # print("transition valid")
            return transition.action
        i+= 1
        transition = tran_table[i]

    return None


    