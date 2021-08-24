//---------------------------------------------------------------------------
//
// wilkins typedefs, structs
//
//--------------------------------------------------------------------------

#ifndef WILKINS_TYPES_HPP
#define WILKINS_TYPES_HPP

#include <stdio.h>
#include <vector>
#include <queue>
#include <string>
#include <iostream>

#include<mpi.h>

using namespace std;

// communicator types
typedef unsigned char CommTypeWilkins;
#define WILKINS_OTHER_COMM      0x00
#define WILKINS_PRODUCER_COMM   0x01
#define WILKINS_CONSUMER_COMM   0x04

struct WilkinsSizes
{
	int prod_size;         // size (number of processes) of producer communicator
	int con_size;          // size (number of processes) of consumer communicator
	int prod_start;        // starting world process rank of producer communicator
	int con_start;         // starting world process rank of consumer communicator
};

typedef MPI_Comm        CommHandle;
typedef MPI_Aint        Address;

#endif
