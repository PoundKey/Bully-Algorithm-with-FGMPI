#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "mpi.h"

/******* FG-MPI Boilerplate begin *********/
#include "fgmpi.h"

/*forward declaration*/
int bully( int argc, char** argv );
char* get_idname(int id, char* buf);
int random_num(int min_num, int max_num);
void send_message(int source, int dest, int type, int* buffer, int buff_size, int send_failure, int mode);
void set_clock(int* buf, int* dlc);
void update_clock(int* local, int remote);
void call_election(int size, int rank, int* buffer, int buff_size, int send_failure, int mode);
void send_coord(int rank, int* buffer, int buff_size, int send_failure, int mode);

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


void usage(char * cmd, int rank) {
    if (0 ==  rank)
        printf("Usage mpirun -nfg X -n Y %s Z\n where X*Y must be >= Z\n", cmd);
}

int bully( int argc, char** argv )
{
    int rank, size, coSize, sRank;
    MPI_Status status;
    
    int DLC = 0; // logical clock
    int MODE, TIMEOUT, AYATIME, SENDFAILURE, RETURNLIFE;
    int timeout_val = 0; //timeout tracking value in second
    int aya_val = 0; //AYA message tracking value in second
    
    int electing = FALSE, probing = TRUE, isCoord = FALSE, isActive = TRUE;
    
    int coordinator; // the rank of coordinator
    
    char IDBUF[21];
    
    int buffer[2];  //buffer[0] = DLC, buffer[1] = Coordinator Number
    int buff_size = 2;
    
    
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
    //printf("World: %d, Rank: %d, Col-Group: %d, Starter: %d\n", size, rank, coSize, sRank);
    //printf("MODE: %d, TIMEOUT: %d, AYATIME: %d, SENDFAILURE: %d, RETURNLIFE: %d\n", MODE, TIMEOUT, AYATIME, SENDFAILURE, RETURNLIFE);
    
    // If rank = 0, let's set it as the clock process, send clock tick to each node every second
    if (rank == 0) {
        while(1) {
            sleep(1);
            int i;
            for (i = 1; i < size; i++) {
                MPI_Send(&buffer, buff_size, MPI_INT, i, CLOCK_ID, MPI_COMM_WORLD);
            }
        }
        
        
    } else {
        // when the program starts, the node with the highest rank declares itself as the coordinator
        // send coordination messages to all other nodes except node 0 (clock process)
        coordinator = size - 1;
        if (rank == coordinator) {
            isCoord = TRUE;
            sleep(1);
            // delcare leader before sending out the coordination message
            printf("[ DLC: %d ] [ LEADER ] [ Node: %d ] declares itself as the leader! \n", DLC, rank);
            set_clock(buffer, &DLC); //increase logical clock and store it in buffer[0]
            send_coord(rank, buffer, buff_size, SENDFAILURE, MODE); // send coordination messages
        }
        
        while (1) {
            MPI_Recv(&buffer, buff_size, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            /**
            printf("RECV_IN: %d >>> Source: %d, TAG_ID: %d [ %s ] \n", rank,
                   status.MPI_SOURCE, status.MPI_TAG, get_idname(status.MPI_TAG, IDBUF));
             */
            int MSG_ID = status.MPI_TAG;
            int remote_rank = status.MPI_SOURCE;
            
            if (MSG_ID != CLOCK_ID && isActive) {
                update_clock(&DLC, buffer[0]);
            }
            
            switch (MSG_ID) {
                case ELECTION_ID:
                    if (isActive) {
                        if (rank > remote_rank) {
                            electing = TRUE;
                            probing = FALSE;
                            timeout_val = 0; //Start timeout
                            set_clock(buffer, &DLC);
                            MPI_Send(&buffer, buff_size, MPI_INT, remote_rank, ANSWER_ID, MPI_COMM_WORLD);
                            set_clock(buffer, &DLC);
                            call_election(size, rank, buffer, buff_size, SENDFAILURE, MODE);
                        }
                    }
                    break;
                case AYA_ID:
                    if (isActive) {
                        if (isCoord) {
                            set_clock(buffer, &DLC);
                            MPI_Send(&buffer, buff_size, MPI_INT, remote_rank, IAA_ID, MPI_COMM_WORLD);
                        }
                    }
                    break;
                case IAA_ID:
                    if (isActive) {
                        if (probing && !isCoord) {
                            timeout_val = 0;
                        }
                    }

                    break;
                case COORD_ID:
                    if (isActive) {
                        if (rank < remote_rank) {
                            coordinator = remote_rank;
                            electing = FALSE;
                            probing = TRUE;
                            timeout_val = 0;
                            aya_val = 0;
                        } else {
                            set_clock(buffer, &DLC);
                            call_election(size, rank, buffer, buff_size, SENDFAILURE, MODE);
                        }
                    }
                    break;
                case ANSWER_ID:
                    if (isActive) {
                        electing = FALSE;
                        probing = TRUE;
                        timeout_val = 0;
                    }

                    break;
                case CLOCK_ID:
                    if (isActive) {
                        if (probing && !isCoord) {
                            timeout_val++;
                            aya_val++;
                        } else {
                            timeout_val++;
                        }
                    } else {
                        timeout_val++;
                    }
                    
                    /**
                    printf("RECV_IN: %d >>> Source: %d, TAG_ID: %d [ %s ], Buffer[0]: %d\n", rank,
                           status.MPI_SOURCE, status.MPI_TAG, get_idname(status.MPI_TAG, IDBUF), buffer[0]);
                     */
                    break;
                case IAM_DEAD_JIM:
                  
                    break;
                case TERMINATE:
                   
                    break;
                default:
                    break;
            }
            
            
            if (isActive) {
                // when the current node is active
                if (probing && !isCoord) {
                    // current node is in probing state, and it's not a coordinator
                    if (aya_val == AYATIME) {
                        set_clock(buffer, &DLC);
                        MPI_Send(&buffer, buff_size, MPI_INT, coordinator, AYA_ID, MPI_COMM_WORLD);
                        aya_val = 0;
                    }
                    
                    if (timeout_val == TIMEOUT) {
                        //switch to election state when timeout
                        set_clock(buffer, &DLC);
                        call_election(size, rank, buffer, buff_size, SENDFAILURE, MODE);
                        probing = FALSE;
                        electing = TRUE;
                        timeout_val = 0;
                        aya_val = 0;
                    }
                } else if (probing && isCoord) {
                    //TODO
                }
                
                
                if (electing) {
                    
                    
                }
                
                
                
            } else {
             // when the current node is in active
                
            }
            
            
        }
    }
    
    
    
    MPI_Finalize();
    return(0);
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

void send_message(int source, int dest, int type, int* buffer, int buff_size, int send_failure, int mode) {
    int chance = random_num(1, 100);
    // success in sending the message
    if(chance > send_failure){
        MPI_Send(&buffer, buff_size, MPI_INT, dest, type, MPI_COMM_WORLD);
    } else {
        //send message failure
        if (mode) {
            printf("Sending Failure >>> From: [ Node: %d] To: [ Node: %d ], Message Type: [ %d ] \n", source, dest, type);
        }
    }
}

//before send message, increase its DLC and set buffer[0] = DLC
void set_clock(int* buf, int* dlc) {
    *dlc += 1;
    buf[0] = *dlc;
}
//upon receving a message from other node, update DLC
void update_clock(int* local, int remote) {
    if (*local < remote) {
        *local = remote;
    }
}

// call for an election
void call_election(int size, int rank, int* buffer, int buff_size, int send_failure, int mode) {
    int i;
    for (i=rank+1; i < size; i++) {
        //MPI_Send(&buffer, buff_size, MPI_INT, i, ELECTION_ID, MPI_COMM_WORLD);
        send_message(rank, i, ELECTION_ID, buffer, buff_size, send_failure, mode);
    }
}

//send coordination message
void send_coord(int rank, int* buffer, int buff_size, int send_failure, int mode) {
    int i = rank - 1;
    while (i > 0) {
        //MPI_Send(&buffer, buff_size, MPI_INT, i, COORD_ID, MPI_COMM_WORLD);
        send_message(rank, i, COORD_ID, buffer, buff_size, send_failure, mode);
        i--;
    }
}
