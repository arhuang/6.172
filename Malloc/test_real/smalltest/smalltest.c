#include <stdio.h>
#include <stdlib.h>

#include "memlib.h"
#include "allocator_interface.h"
#include "malloc_replace.h"

#define OUTER_LEN 100
#define MAX_INNER_LEN 1000
#define MAX_SIZE 1000

int verbose = 1;
int main(){

  _mem_init();

  int **array;
  int i, j;

  array = (int**)_malloc(sizeof(int*)*OUTER_LEN);

  for(i=0; i<OUTER_LEN; i++){

    int innerLen = rand()%MAX_INNER_LEN;
	array[i] = (int*)_malloc(sizeof(int)*innerLen);

    for(j=0; j<innerLen; j++){
	    array[i][j] = rand()%(1<<31);
 	}
  }

  for(i=0; i<OUTER_LEN; i++){
	  _free(array[i]);
  }
  return 0;
}
