#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <netinet/in.h>

#include "mpi.h"

/******* FG-MPI Boilerplate begin *********/
#include "fgmpi.h"

// enum to keep tracking a node's internal state
typedef enum {
    PROBING = 1000,
    ELECTING,
    COORDINATING //The node currently is coordinator
} State;

/*forward declaration*/
int bully( int argc, char** argv );
void clear_val(int* timeout_val, int* aya_val);
char* get_idname(int id, char* buf);
int random_num(int min_num, int max_num);
void set_clock(int* buf, int* dlc);
void update_clock(int* local, int remote);
void send_message(int source, int dest, int type, int* buffer, int buff_size, int mode, int* DLC);
void call_election(int size, int rank, int* buffer, int buff_size, int mode, int* DLC);
void send_coord(int rank, int* buffer, int buff_size, int mode, int* DLC);

/*forwdrd declaration*/


FG_ProcessPtr_t binding_func(int argc __attribute__ ((unused)),
                             char** argv __attribute__ ((unused)),
                             int rank __attribute__ ((unused))){
    return (&bully);
}

FG_MapPtr_t map_lookup(int argc __attribute__ ((unused)),
                       char** argv __attribute__ ((unused)),
                       char* str __attribute__ ((unused))){
    return (&binding_func);
}

int main( int argc, char *argv[] )
{
    FGmpiexec(&argc, &argv, &map_lookup);
    return (0);
}
/******* FG-MPI Boilerplate end *********/


#define  ELECTION_ID   20
#define  AYA_ID        21
#define  IAA_ID        22
#define  COORD_ID      23
#define  ANSWER_ID     24
#define  CLOCK_ID      25
#define  IAM_DEAD_JIM  998
#define  TERMINATE     999

#define FALSE 0
#define TRUE !FALSE

char IDBUF[21]; //just a buffer to show Message ID => Message name

void usage(char * cmd, int rank) {
    if (0 ==  rank)
        printf("Usage mpirun -nfg X -n Y %s Z\n where X*Y must be >= Z\n", cmd);
}

int bully( int argc, char** argv )
{
    int rank, size, coSize, sRank;
    MPI_Status status;
    
    srand(time(NULL)); //used for generate survival rate
    int DLC = 0; // logical clock
    int MODE, TIMEOUT, AYATIME, SENDFAILURE, RETURNLIFE;
    int timeout_val = 0; //timeout tracking value in second
    int aya_val = 0; //AYA message tracking value in second
    
    State state = PROBING; // node's internal state: probing, electing or coordinating
    int isActive = TRUE, isAnswer = FALSE;
    
    
    int coordinator; // the rank of coordinator
    
    int buffer[1];  //buffer[0] = DLC
    int buff_size = 1;
    
    
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPIX_Get_collocated_size(&coSize);
    MPIX_Get_collocated_startrank(&sRank);
    
    // Initialize the input paramenter, for now assume proper user input
    char *endPtr;
    MODE = (int) strtol(argv[1], &endPtr, 10);
    TIMEOUT = (int) strtol(argv[2], &endPtr, 10);
    AYATIME = (int) strtol(argv[3], &endPtr, 10);
    SENDFAILURE = (int) strtol(argv[4], &endPtr, 10);
    RETURNLIFE = (int) strtol(argv[5], &endPtr, 10);
    
    // If rank = 0, let's set it as the clock process, send clock tick to each node every second
    if (rank == 0) {
        while(1) {
            sleep(1);
            int i;
            for (i = 1; i < size; i++) {
                MPI_Send(buffer, buff_size, MPI_INT, i, CLOCK_ID, MPI_COMM_WORLD);
            }
        }
        
        
    } else {
        // when the program starts, the node with the highest rank declares itself as the coordinator
        // send coordination messages to all other nodes except node 0 (clock process)
        coordinator = size - 1;
        if (rank == coordinator) {
            state = COORDINATING;
            sleep(1);
            // delcare leader before sending out the coordination message
            printf("[ DLC: %d ] [ LEADER ] [ Node: %d ] declares itself as the coordinator! \n", DLC, rank);
            send_coord(rank, buffer, buff_size, MODE, &DLC); // send coordination messages
        }
        
        while (1) {
            MPI_Recv(buffer, buff_size, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            
            int MSG_ID = status.MPI_TAG;
            int remote_rank = status.MPI_SOURCE;
            if (MODE && MSG_ID != CLOCK_ID) {
                
                printf("[ DLC: %d ] Message Recv >>> Home: [ Node: %d] Sr: [ Node: %d ], Message Type: %d [ %s ] \n", DLC, rank, remote_rank, MSG_ID, get_idname(MSG_ID, IDBUF));
            }
            
            if (MSG_ID != CLOCK_ID) {
                update_clock(&DLC, ntohl(buffer[0]));
            }
            
            
            if (isActive) {
                // Start Handling incoming messages for nodes that are active
                switch (MSG_ID) {
                    case ELECTION_ID:
                        if (rank > remote_rank) {
                            switch (state) {
                                case PROBING:
                                    //TODO
                                    break;
                                case ELECTING:
                                    //TODO
                                    break;
                                case COORDINATING:
                                    //TODO
                                    break;
                            }
                        } else {
                            switch (state) {
                                case PROBING:
                                    //TODO
                                    break;
                                case ELECTING:
                                    //TODO
                                    break;
                                case COORDINATING:
                                    //TODO
                                    break;
                            }
                            break;
                        case AYA_ID:
                            if (state == COORDINATING) {
                                // Generate a random number between 1-100, to determine if the coordinator should die
                                int survivalRate = rand()%100 + 1;
                                if (survivalRate > SENDFAILURE) {
                                    // Oh yes, the node survived, send back a IAA message
                                    send_message(rank, remote_rank, IAA_ID, buffer, buff_size, MODE, &DLC);
                                } else {
                                    // Coordinator enters a sudo death state
                                    isActive = FALSE;
                                    state = PROBING;
                                    isAnswer = FALSE;
                                    clear_val(&timeout_val, &aya_val);
                                }
                                
                            }
                            break;
                        case IAA_ID:
                            if (state == PROBING)
                                timeout_val = 0;
                            break;
                        case COORD_ID:
                            if (rank > remote_rank) {
                                //call an election if local node rank is greater than the coordinator's
                                state = ELECTING;
                                isAnswer = FALSE;
                                clear_val(&timeout_val, &aya_val);
                                printf("[ DLC: %d ]  [ ELECTION ]  [ Node: %d ] calls an election! \n", DLC, rank);
                                call_election(size, rank, buffer, buff_size, MODE, &DLC);
                            } else {
                                //set the coordinator
                                coordinator = remote_rank;
                                state = PROBING;
                                isAnswer = FALSE;
                                clear_val(&timeout_val, &aya_val);
                            }
                            break;
                        case ANSWER_ID:
                            state = ELECTING;
                            isAnswer = TRUE;
                            clear_val(&timeout_val, &aya_val);
                            break;
                        case CLOCK_ID:
                            if (state == PROBING) {
                                aya_val++;
                            }
                            timeout_val++;
                            break;
                        case IAM_DEAD_JIM:
                            // RESERVED
                            break;
                        case TERMINATE:
                            // RESERVERD
                            break;
                        default:
                            break;
                        } // end switch(MSG_ID)
                        
                        
                        switch (state) {
                            case PROBING:
                                //send aya message to the coordinator if AYATIME reached
                                if (aya_val == AYATIME) {
                                    send_message(rank, coordinator, AYA_ID, buffer, buff_size, MODE, &DLC);
                                    aya_val = 0; //reset aya_val for next round of probing
                                }
                                
                                //Oops, timeout in probing state, coordinator is dead, we need to call an election
                                if (timeout_val == TIMEOUT) {
                                    printf("[ DLC: %d ]  [ LEADERDEAD ] [ Node: %d ] detects coordinator failure! \n", DLC, rank);
                                    printf("[ DLC: %d ]  [ ELECTION ]   [ Node: %d ] calls an election! \n", DLC, rank);
                                    state = ELECTING;
                                    isAnswer = FALSE;
                                    clear_val(&timeout_val, &aya_val);
                                    call_election(size, rank, buffer, buff_size, MODE, &DLC);
                                }
                                break;
                            case ELECTING:
                                // current node has involved in a electing
                                // Oops, timeout reached in electing state,
                                // declare itself as the coordinator and send coordination message
                                if ((timeout_val == TIMEOUT) && !isAnswer) {
                                    coordinator = rank;
                                    state = COORDINATING;
                                    isAnswer = FALSE;
                                    clear_val(&timeout_val, &aya_val);
                                    printf("[ DLC: %d ]  [ LEADER ] [ Node: %d ] declares itself as the coordinator! \n", DLC, rank);
                                    send_coord(rank, buffer, buff_size, MODE, &DLC);
                                } else if ((timeout_val == TIMEOUT) && isAnswer) {
                                    // call election again, because the only answer received, but not the coordination message
                                    state = ELECTING;
                                    clear_val(&timeout_val, &aya_val);
                                    printf("[ DLC: %d ]  [ ELECTION ]  [ Node: %d ] calls an election! \n", DLC, rank);
                                    call_election(size, rank, buffer, buff_size, MODE, &DLC);
                                }
                                break;
                            case COORDINATING:
                                //TODO: may not be necessary, save this for later
                                break;
                            default:
                                break;
                        }
                        
                }// end of active state message handling
                else {
                    // Start Handling incoming messages for nodes that are not active
                    switch (MSG_ID) {
                            
                        case CLOCK_ID:
                            timeout_val++;
                            break;
                        default:
                            break;
                    }
                    
                    //when return to life timeout reached, switch state to become active
                    if (timeout_val == RETURNLIFE) {
                        isActive = TRUE;
                        isAnswer = FALSE;
                        state = ELECTING;
                        clear_val(&timeout_val, &aya_val);
                        printf("[ DLC: %d ]  [ ALIVE ]     [ Node: %d ] ex-coordinator declares its return to alive! \n", DLC, rank);
                        printf("[ DLC: %d ]  [ ELECTION ]  [ Node: %d ] calls an election! \n", DLC, rank);
                        call_election(size, rank, buffer, buff_size, MODE, &DLC);
                    }
                } // end of inactive state message handling
            }
            
        }
        
        MPI_Finalize();
        return(0);
    }
    
    
    void clear_val(int* timeout_val, int* aya_val) {
        *timeout_val = 0;
        *aya_val = 0;
    }
    
    char* get_idname(int id, char* buf) {
        memset(buf, 0, 21);
        switch (id) {
            case ELECTION_ID:
                strncpy(buf, "ELECTION_ID", 20);
                break;
            case AYA_ID:
                strncpy(buf, "AYA_ID", 20);
                break;
            case IAA_ID:
                strncpy(buf, "IAA_ID", 20);
                break;
            case COORD_ID:
                strncpy(buf, "COORD_ID", 20);
                break;
            case ANSWER_ID:
                strncpy(buf, "ANSWER_ID", 20);
                break;
            case CLOCK_ID:
                strncpy(buf, "CLOCK_ID", 20);
                break;
            case IAM_DEAD_JIM:
                strncpy(buf, "IAM_DEAD_JIM", 20);
                break;
            case TERMINATE:
                strncpy(buf, "TERMINATE", 20);
                break;
            default:
                break;
        }
        return buf;
    }
    
    int random_num(int min_num, int max_num) {
        int result = 0, low_num = 0, hi_num = 0;
        if(min_num < max_num)
        {
            low_num = min_num;
            hi_num = max_num + 1;
        } else {
            low_num = max_num + 1;
            hi_num = min_num;
        }
        srand(time(NULL));
        result = (rand()%(hi_num - low_num)) + low_num;
        return result;
    }
    
    //before send message, increase its DLC and set buffer[0] = DLC
    void set_clock(int* buf, int* dlc) {
        *dlc += 1;
        buf[0] = htonl(*dlc);
    }
    //upon receving a message from other node, update DLC
    void update_clock(int* local, int remote) {
        if (*local < remote) {
            *local = remote;
        }
    }
    
    
    void send_message(int source, int dest, int type, int* buffer, int buff_size, int mode, int* DLC) {
        set_clock(buffer, DLC);
        if (mode) {
            printf("[ DLC: %d ] Message Sent >>> From: [ Node: %d] To: [ Node: %d ], Message Type: %d [ %s ] \n", *DLC, source, dest, type, get_idname(type,IDBUF));
        }
        MPI_Send(buffer, buff_size, MPI_INT, dest, type, MPI_COMM_WORLD);
    }
    
    
    // call for an election
    void call_election(int size, int rank, int* buffer, int buff_size, int mode, int* DLC) {
        int i;
        for (i=rank+1; i < size; i++) {
            send_message(rank, i, ELECTION_ID, buffer, buff_size, mode, DLC);
        }
    }
    
    //send coordination message
    void send_coord(int rank, int* buffer, int buff_size, int mode, int* DLC) {
        int i = rank - 1;
        while (i > 0) {
            send_message(rank, i, COORD_ID, buffer, buff_size, mode, DLC);
            i--;
        }
    }
    
