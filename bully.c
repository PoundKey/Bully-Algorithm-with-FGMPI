#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"

/******* FG-MPI Boilerplate begin *********/
#include "fgmpi.h"
int bully( int argc, char** argv ); /*forward declaration*/
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

// This is where you should start, add or change any code in here as you like
// Note that bully code is simply meant to show you how to use MPI and you 
// probably want to gut most of it. In this example, the defines below are used as 
// TAGs, but you will probably want to put them into the buffer.  

#define  ELECTION_ID   20
#define  AYA_ID        21
#define  IAA_ID        22
#define  COORD_ID      23
#define  ANSWER_ID     24
#define  IAM_DEAD_JIM  998
#define  TERMINATE     999


void usage(char * cmd, int rank) {
  if (0 ==  rank) 
    printf("Usage mpirun -nfg X -n Y %s Z\n where X*Y must be >= Z\n", cmd);
}

int bully( int argc, char** argv )
{
  int rank,            // This is analogous to a process id/port number 
                       // The values start a 0 and go to size - 1
    size,              // The total number of process across all processors
    coSize,            // The number of processes co-located in your address space
    sRank;             // The ID of the first process in your address space
 

  MPI_Status status;
  
  /* From the file mpi.h this is the definition for MPI_status, with my comments on the 
     fields


     typedef struct MPI_Status {
     int count;          // Seems to be the count, in bytes of the data received
     int cancelled;      // Indication of whether of not the operation was cancelled.
                         // this probably only applies if you are doing non-blocking 
                         // operations
     int MPI_SOURCE;     // ID of the source
     int MPI_TAG;        // The actual message tag
     int MPI_ERROR;      /  The error code if there was one
     
     } MPI_Status;

  */     


  int msgCount = 0;

  // Calls to initialize the system and get the data you will need 
  // You probably don't need to change any of this. 
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPIX_Get_collocated_size(&coSize);
  MPIX_Get_collocated_startrank(&sRank);


  // For demo purposes let's get all the command line arguments, and if we are 
  // rank 0 print them. 
  
  if (0 == rank) {
    printf("argv[0] = %s\n", argv[0]);
    // Assuming remaing args are all integers if you want to 
    // remember them assign them to a variable

    int i ;
    for (i = 1; i < argc; i++) {
      char *endPtr;
      int val;
	
      val = strtol(argv[i], &endPtr, 10);
      
      printf("argv[%d] = ", i); 
      if (*endPtr == '\0') { // valid conversion
	printf("%d\n", val);
      } else {
	printf(" Not a valid number\n");
      }
    }
  }
  printf("Hello! I am rank %d of %d co-located size is %d, start rank %d \n", 
	 rank, size, coSize, sRank);
  
  // If I am node 0 I'll wait for everyone to send a message to me. 
  // If I am not node 0 then I'll send a message to node 0
  
  if (rank == 0) { // Wait for messages
    int i;
    int buffer[2]; 
    int buff_size = 2;

    // First let's collect a bunch of registration tags from individual nodes
    // and ACK the tags
    for (i = size - 1 ; i > 0;  i--) {
      MPI_Recv(&buffer,    // buffer to receive into
	       buff_size,  // the size of the buffer in buffer units
	       MPI_INT,    // What the units of the buffer are, in this 
	                   // case they are ints, so if buff_size were
	                   // 2 that would mean there is enough space for 2 ints
	       i,          // Proccess to receive from. If this is MPI_ANY_SOURCE
	                   // it means receive from any one
	       ELECTION_ID,// Message have tags (i.e. types) this is the type pf
                           // message you are looking for. If it is MPI_ANY_TAG
                           // it means any tag
	       MPI_COMM_WORLD,   // The collection (group) of process you are willing
	                         // to accept a message from. In this assignment
	                         // it shouldn't be changed
	       &status);    // This provides some feedback on the receive

      msgCount++;
      printf("Message from %d buff size %d count %d tag %d contents %d\n", 
	     status.MPI_SOURCE,
	     buff_size,
	     status.count, 
	     status.MPI_TAG,
	     buffer[0]);

      // Send an AYA message to the node
      buffer[0] = AYA_ID;
      MPI_Send(&buffer, 1, MPI_INT, status.MPI_SOURCE, IAA_ID, MPI_COMM_WORLD);
    }

    printf("Waiting for the IAA_ID messages\n");

    // All the process should now have received an IAA_ID message Let's recieve 
    // messages from any process with the right appropriate TAG.
    for (i = 1; i < size; i++) {
      // Wait for the answer 
      // printf("Waiting for the IAA_ID messages from %d\n", i);
      MPI_Recv(&buffer, buff_size, MPI_INT, MPI_ANY_SOURCE, 
	       IAA_ID, MPI_COMM_WORLD, &status);
      msgCount++;
      printf("Message from %d size %d tag %d contents %d\n", 
	     status.MPI_SOURCE, 
	     status.count, 
	     status.MPI_TAG,
	     buffer[0]);

      // Do it again
      buffer[0] = AYA_ID;
      MPI_Send(&buffer, 1, MPI_INT, status.MPI_SOURCE, IAA_ID, MPI_COMM_WORLD);
      MPI_Recv(&buffer, buff_size, MPI_INT, status.MPI_SOURCE, 
	       IAA_ID, MPI_COMM_WORLD, &status);
      printf("Message from %d size %d tag %d contents %d\n", 
	     status.MPI_SOURCE, 
	     status.count, 
	     status.MPI_TAG,
	     buffer[0]);
      msgCount++;

      buffer[0] = TERMINATE;
      MPI_Send(&buffer, 1, MPI_INT, status.MPI_SOURCE, IAA_ID, MPI_COMM_WORLD);
      
    }
  } else {
    // send a message to node 0
    int sbuffer = rank + 10000;
    int sbuff_size = 1;
    int rootNode = 0;

    MPI_Status stat;
    //    printf("About to send\n");
    MPI_Send(&sbuffer, sbuff_size, MPI_INT, 
	     rootNode, ELECTION_ID, MPI_COMM_WORLD);


    // keep receiving and ACKING messages until a terminate one is received
    for (;;) {
      // Wait for an ACK
      // printf("%d waiting for a message\n", rank);
      MPI_Recv(&sbuffer, sbuff_size, MPI_INT, 
	       MPI_ANY_SOURCE, IAA_ID, MPI_COMM_WORLD, &stat);
      // printf("%d GOTTA a message\n", rank);
      if (sbuffer == TERMINATE) 
	break;

      // Now send a message back
      sbuffer = rank + 20000;
      MPI_Send(&sbuffer, sbuff_size, MPI_INT, 
	       stat.MPI_SOURCE, IAA_ID, MPI_COMM_WORLD);
      //    printf("Rank %d, message sent status %d\n", rank, stat);    
    }
  }
  
  
  
  MPI_Finalize();
  if (0 == rank) {
    printf("%d Done received %d Messages\n", rank, msgCount);
  } else {
    //printf("%d DONE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n", rank);
  }
  return(0);
}
