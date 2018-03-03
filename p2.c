#include "station.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h> 
#include <unistd.h>
#include <assert.h>

#define BILLION 1000000000L
#define TS_LEN 28
#define TENTH_SEC 100000

typedef struct Train{ 
    pid_t pid; 
    int id; 
    int pr; //1 high, 0 low
    char b; // bound
    char bound[5];
    int ld;
    double ld_time; // Timestamp stop loading time
    int x;	// crossing time
    int s; // 0-loading, 1-ready, 2-granted, 3-xing, 4-gone 
    struct Train *next; 
}Train;

pthread_mutex_t queue_mutex;
pthread_mutex_t main_track_mutex;
pthread_cond_t green_light_cv;
struct Train *root, *temp;
int train_ctr = 0; // number of trains
int rdy_ctr = 0;
int awaiting_ld = 1; //global variable used to signal runner start loading
int train_on_track = 0; //global variable used to signal dispatcher,

Train *addTrain (char b, int pr, int ld, int x, char *bound){
	Train *new_train = (Train*)malloc(sizeof(Train));
	memset(new_train, 0, sizeof(Train));

	new_train->id = train_ctr;
	new_train->pr = pr;
	new_train->ld = ld;
	new_train->x = x;
	new_train->b = b;
	strncpy(new_train->bound,bound,5);	
	new_train->s = -1; 
	new_train->next = NULL;

	if (train_ctr == 0){
		root = new_train;
		printf("Id=%d - s:%d, b:%c-%s, p:%d, ld-x:%d-%d, ldts:%f\n", 
			new_train->id, new_train->s, new_train->b, new_train->bound, new_train->pr, new_train->ld, new_train->x, new_train->ld_time);

	}else{
		for (temp = root ; temp->next != NULL ; temp = temp->next);
		temp->next = new_train;
		printf("Id=%d - s:%d, b:%c-%s, p:%d, ld-x:%d-%d, ldts:%f\n", 
			new_train->id, new_train->s, new_train->b, new_train->bound, new_train->pr, new_train->ld, new_train->x, new_train->ld_time);
	}
	train_ctr ++;

	return new_train;
}

int getTime(struct timeval start, struct timeval stop, char *buf){
	double hours, t, minutes, seconds, accum;
	memset( buf, '\0', sizeof(char)*TS_LEN );

	accum = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
	hours = accum/3600;
	t = fmod(accum,3600);
	minutes = t/60;
	seconds = fmod(t,60);
	int w = sprintf(buf, "%02d:%02d:%04.1f", (int)hours, (int)minutes, seconds);
	int x = (w > 0) ? w : -1;
	return x;
}

void* runner(void *train_arg){
	char buf[TS_LEN];
	struct Train *train = (Train *)train_arg;
	struct timeval start_ld, stop_ld, start_x, stop_x;

	int loading = train->ld * TENTH_SEC;
	int xing = train->x * TENTH_SEC;
	// Waiting for dispatcher global var signal for all trains to start loading
	while (awaiting_ld);
	
	// LOADING
	train->s = 0; // loading
	gettimeofday(&start_ld, NULL);
	usleep(loading);
	gettimeofday(&stop_ld, NULL);
	if (getTime(start_ld, stop_ld, buf) <= 0){
		fprintf(stderr,"ERROR - Converting clock format (1) \n");
	}
	printf("%s Train %d is READY to go %s\n", buf, train->id, train->bound);
	train->ld_time = (double)(stop_ld.tv_usec) / 1000000 + (double)(stop_ld.tv_sec); 
	train->s = 1; //ready
	
	pthread_mutex_lock(&queue_mutex);
	addNode(train->id, train->pr, train->b, train->ld_time);
	rdy_ctr++;
	pthread_mutex_unlock(&queue_mutex);

	//CROSSING main track
	pthread_mutex_lock(&main_track_mutex);
	while(train->s != 2){
		pthread_cond_wait(&green_light_cv, &main_track_mutex);
	}
	gettimeofday(&start_x, NULL);
	if (getTime(start_ld, start_x, buf) <= 0){
		fprintf(stderr,"ERROR - Converting clock format (2)\n");
	}
	train->s = 3; //xing
	train_on_track = 1; 
	printf("%s Train:%d is ON the main track going %s\n", buf, train->id, train->bound);
	usleep(xing);
	gettimeofday(&stop_x, NULL);
	if (getTime(start_ld, stop_x, buf) <= 0){
		fprintf(stderr,"ERROR - Converting clock format (3)\n");
	}
	printf("%s Train:%d is OFF the main track going %s\n", buf, train->id, train->bound);
	pthread_mutex_unlock(&main_track_mutex);
	train->s = 4; //gone
	train_on_track = 0; 
	
	return NULL;

}

void *dispatcher(void* args){
	pthread_t tid[train_ctr];
	pthread_attr_t attr;
	int total_dispatched = 0, x;
	struct Train *temp;

	// Initialize mutex and condition variable objects
	pthread_mutex_init(&queue_mutex, NULL);
	pthread_mutex_init(&main_track_mutex, NULL);
	pthread_cond_init (&green_light_cv, NULL);
	// For portability, explicitly create threads in joinable state
	pthread_attr_init(&attr);
  	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);


	// Initialize train threads
	for (x = 0, temp = root ; x < train_ctr ; x++, temp = temp->next){
		if (pthread_create(&tid[x], &attr, runner, (void *) temp) != 0){
			fprintf(stderr,"ERROR - Cannot create TRAIN thread tid:%d\n", x);
		}
	}

	usleep(500000);
	awaiting_ld = 0; // Signal ready to load

	int last_b_disp = 0; // 0 West, 1 East 
	while (total_dispatched < train_ctr){
		Train *to_disp_train;
		int to_disp_id;
		while (train_on_track); //waiting for train to leave main track befores dispatches next train
		while (rdy_ctr == 0);
		if (stEA_ctr > 0 || stWE_ctr > 0){ // High train waiting 
			// More than 1 High waiting?
			if ((stEA_ctr >= 1 && stWE_ctr >= 1) || stEA_ctr >= 2 || stWE_ctr >= 2 ){ 
				// 2+ High, OPPOSITE direction - dispatch opposite direction from last dispatched
				if (stEA_ctr >= 1 && stWE_ctr >= 1) {
					if (last_b_disp){ // 0 West, 1 East 
						to_disp_id = getHeadID('W'); 
					} else{
						to_disp_id = getHeadID('E'); 
					}
				// 2+ High, SAME direction - dispatch by min ld and lower id
				}else{	
					int x;
					if (stWE_ctr >0){
						assert(stEA_ctr == 0);
						x = findMinLd('W');
					}else{
						assert(stWE_ctr == 0);
						x = findMinLd('E');
					}
					to_disp_id = x;
				}
			// Single High train waiting
			}else{
				if (stEA_ctr == 1){
					to_disp_id = getHeadID('E'); 
				} else{
					to_disp_id = getHeadID('W'); 
				}
			}
		// LOW waiting (no high) waiting
		}else { 
			// More than 1 Low waiting?
			if ((ste_ctr >= 1 && stw_ctr >= 1) || ste_ctr >= 2 || stw_ctr >= 2 ){ 
				// 2+ low, OPPOSITE direction - dispatch opposite direction from last dispatched
				if (ste_ctr >= 1 && stw_ctr >= 1) {
					if (last_b_disp){ // 0 West, 1 East 
						to_disp_id = getHeadID('w'); 
					} else{
						to_disp_id = getHeadID('e'); 
					}
				// 2+ High, SAME direction - dispatch by min ld and lower id
				}else{	
					int x;
					if (stw_ctr >0){
						assert(ste_ctr == 0);
						x = findMinLd('w');
					}else{
						assert(stw_ctr == 0);
						x = findMinLd('e');
					}
					to_disp_id = x;
				}
			// Single Low train waiting
			}else{
				if (ste_ctr == 1){
					to_disp_id = getHeadID('e'); 
				} else{
					to_disp_id = getHeadID('w'); 
				}
			}				
		}

		for(to_disp_train = root ; to_disp_train->id != to_disp_id ; to_disp_train = to_disp_train->next);
		// Simulation rule: if same priority, opposite directions, 
		// pick train which will travel in the opposite dir from last dispatched
		switch (to_disp_train->b){
			case 'W':
			case 'w':
				last_b_disp = 0;
				break;
			case 'E':
			case 'e':
				last_b_disp = 1;
				break;
		}

		//Signal Granted train to dispatch
		pthread_mutex_lock(&main_track_mutex);
		to_disp_train->s = 2; // Granted
		pthread_cond_broadcast(&green_light_cv);
		pthread_mutex_unlock(&main_track_mutex);

		//Removing train from station
		pthread_mutex_lock(&queue_mutex);
		rdy_ctr--;
		removeTrain(to_disp_train->b, to_disp_train->id);
		pthread_mutex_unlock(&queue_mutex);	

		total_dispatched++;
	}	

	// Join all running Train threads 
	for (x =0 ; x < train_ctr ; x++){
		if ( pthread_join(tid[x], NULL) != 0){
			fprintf(stderr, "ERROR - Cannot join thread TRAIN tid:%d\n", x);
		}
	}
	return NULL;
}

int main (int argc, char *argv[]){
	FILE *fp;

	if (argc != 2){
		printf("ERROR - Usage: [file_name]\n");
		return EXIT_FAILURE;
	}

	fp = fopen(argv[1], "r");
	if (fp == NULL){
		printf("ERROR - failed opening file\n");
		return EXIT_FAILURE;
	}else {
		printf("File opened successfully\n");
	}

	//Reading contents from input file
	char b, bound[5] ;
	int l, p, x;
	Train *temp;
	printf("=======INPUT FILE=======\n");
	for ( ; fscanf(fp, "%c %d %d \n", &b, &l, &x) != EOF ; ){
		if (b == 'w' || b == 'e'){
			p = 0;
		}else{
			p = 1;
		}

		if (b == 'w' || b == 'W'){
			strncpy(bound, "West", 5);
		}else{
			strncpy(bound, "East", 5);		
		}
		temp = addTrain(b,p,l,x, bound);
	}
	printf("=========================\n\n");

	// For portability, explicitly create threads in joinable state
	pthread_t tid;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
  	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	
	//Create dispatcher thread
	if (pthread_create(&tid, &attr, dispatcher, NULL) != 0){
		fprintf(stderr,"ERROR - Cannot create DISPATCHER thread TID:%lu\n", tid);
		return EXIT_FAILURE;
	}

	if (pthread_join(tid, NULL) != 0){
		fprintf(stderr, "ERROR - Cannot join thread #%d\n", x);
	}

	printf("\nMAIN: All threads finalized\n");

	printf("==========FINAL DATA==========\n");
	for(temp = root; temp != NULL ; temp = temp->next ){
		printf("Id=%d - s:%d, b:%c-%s, p:%d, ld-x:%d-%d, ld_time:%f\n", temp->id, temp->s, temp->b, temp->bound, temp->pr, temp->ld, temp->x, temp->ld_time);
	}
	printf("==============================\n");



	//TODO: NEED TO FREE ALL MALLOC FROM ADD TRAIN
	//TODO: DESTROY MUTEXES 

	/*
	  pthread_attr_destroy(&attr);
   pthread_mutex_destroy(&count_mutex);
   pthread_cond_destroy(&count_threshold_cv);
   pthread_exit(NULL);
   */


	fclose(fp);
	return EXIT_SUCCESS;
}