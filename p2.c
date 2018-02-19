#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h> 

typedef struct Train{ 
    pid_t pid; 
    int id; 
    int pr; //1 high, 0 low
    char b; // bound
    int ld; // loading time
    int x;	// crossing time
    int status; // 0
    struct Train *next; 
}Train;

struct Train *root, *temp;
int counter = 0;

void addTrain (char bound, int priority, int loading, int crossing){
	Train *new_train = (Train*)malloc(sizeof(Train));
	memset(new_train, 0, sizeof(Train));

	new_train->id = counter;
	new_train->pr = priority;
	new_train->ld = loading;
	new_train->x = crossing;
	new_train->b = bound;
	new_train->status = 0; 
	new_train->next = NULL;

	if (counter == 0){
		root = new_train;
		printf("Added - id=%d - bound:%c, prior:%d, load:%d, xing:%d\n", new_train->id, new_train->b, new_train->pr, new_train->ld, new_train->x);

	}else{
		for (temp = root ; temp->next != NULL ; temp = temp->next);
		temp->next = new_train;
		printf("Added - id=%d - bound:%c, prior:%d, load:%d, xing:%d\n", new_train->id, new_train->b, new_train->pr, new_train->ld, new_train->x);

	}

	counter ++;
	// return &new_train;
}

int main (int argc, char *argv[]){
	FILE *fp;

	if (argc != 2){
		printf("ERROR - Usage: [file_name]\n");
		return 1;
	}

	fp = fopen(argv[1], "r");
	if (fp == NULL){
		printf("ERROR - failed opening file\n");
		return 1;
	}else {
		printf("Open filed successfully\n");
	}


	char b;
	int l;
	int p;
	int x;
	for ( ; fscanf(fp, "%c %d %d \n", &b, &l, &x) != EOF ;){
		if (b == 'w' || b == 'e'){
			p=0;
		}else{
			p=1;
		}
		addTrain(b,p,l,x);
		// printf("bound: %c | priority: %d | loading:%d | xing:%d\n", b, p, l, x );
	}



	fclose(fp);
	return 0;
}