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
int awaiting = 1; //global variable used for thread signaling to start loading

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

/*
	// ssize_t format_timeval(struct timeval *tv, char *buf, size_t sz){
	// 	ssize_t written = -1;
	// 	struct tm *gm = gmtime(&tv->tv_sec);
	// 	if (gm){
	// 		written = (ssize_t)strftime(buf, sz, "%T", gm);
		
	// 		// Getting first digit from usec
	// 		int first = tv->tv_usec; 
	// 		while (first >= 10) { first = first/10;}
	// 		// printf("\nformat_timeval First digit:%d \n", first);
		
	// 		if ((written > 0) && ((size_t)written < sz)){
	// 			int w = snprintf(buf+written, sz-(size_t)written, ".%d", first);
	// 			// int w = snprintf(buf+written, sz-(size_t)written, ".%01i", tv->tv_usec);
	// 			written = (w > 0) ? written + w : -1;
	// 		}
	// 	}
	// 	return written;
	// }
*/

int getTime(struct timeval start, struct timeval stop, char *buf){
	double hours, t, minutes, seconds, accum;
	memset( buf, '\0', sizeof(char)*TS_LEN );


	// double waka1 = (double)(start.tv_usec) / 1000000 + (double)(start.tv_sec); 
	// double waka2 = (double)(stop.tv_usec) / 1000000 + (double)(stop.tv_sec); 
	// // printf("getTime - start:%f | stop:%f\n", waka1, waka2);
	
	accum = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);

	hours = accum/3600;
	t = fmod(accum,3600);
	minutes = t/60;
	seconds = fmod(t,60);
	int w = sprintf(buf, "%02d:%02d:%04.1f", (int)hours, (int)minutes, seconds);
	// printf("getTime - Accum:%f | PRE STRING :%lf, %02d:%02d:%04.1f | BUF:%s\n", accum, (int)hours, (int)minutes, seconds, buf);

	int x = (w > 0) ? w : -1;
	return x;
}

void* runner(void *train_arg){
	char buf[TS_LEN];
	struct Train *train = (Train *)train_arg;
	struct timeval start_ld, stop_ld, start_x, stop_x;
	printf("Runner - NEW THREAD Train | id:%d | s:%d | b:%c | pr:%d \n", train->id, train->s, train->b, train->pr );


	int loading = train->ld * TENTH_SEC;
	int xing = train->x * TENTH_SEC;
	// Waiting for dispatcher global var signal for all trains to start loading
	while (awaiting);
	
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
		printf("Train:%d is going to sleep ZzZzZZZZZzzzzZz\n", train->id);
		pthread_cond_wait(&green_light_cv, &main_track_mutex);
	}
	printf("Train:%d is AWAKE\n", train->id);
	gettimeofday(&start_x, NULL);
	if (getTime(start_ld, start_x, buf) <= 0){
		fprintf(stderr,"ERROR - Converting clock format (2)\n");
	}
	train->s = 3; //xing
	printf("%s Train:%d is ON the main track going %s\n", buf, train->id, train->bound);
	usleep(xing);
	gettimeofday(&stop_x, NULL);
	if (getTime(start_ld, stop_x, buf) <= 0){
		fprintf(stderr,"ERROR - Converting clock format (3)\n");
	}
	printf("%s Train:%d is OFF the main track going %s\n", buf, train->id, train->bound);
 	// TODO: Timestamp finish xing
	pthread_mutex_unlock(&main_track_mutex);
	train->s = 4; //gone

	printf("Exit Train:%d thread with status:%d \n", train->id, train->s);





	// return EXIT_SUCCESS;






	return NULL;

}

void *dispatcher(void* args){
	pthread_t tid[train_ctr];
	pthread_attr_t attr;
	int x, total_dispatched = 0;
	struct Train *temp;

	// Initialize mutex and condition variable objects
	pthread_mutex_init(&queue_mutex, NULL);
	pthread_mutex_init(&main_track_mutex, NULL);
	pthread_cond_init (&green_light_cv, NULL);
	// For portability, explicitly create threads in joinable state
	pthread_attr_init(&attr);
  	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);


	// Initialize train threads
	// printf("DISP - address of root:%p\n", (void *) &root);

	for (x = 0, temp = root ; x < train_ctr ; x++, temp = temp->next){
		printf("DISP - Creating thread #%d | id:%d | s:%d | b:%c | pr:%d |temp_add:%p |root_add:%p\n",
					 x, temp->id, temp->s, temp->b, temp->pr, temp, root);
		Train *train_arg = malloc(sizeof(Train));
		train_arg = temp;
		if (pthread_create(&tid[x], &attr, runner, (void *) temp) != 0){
			fprintf(stderr,"ERROR - Cannot create TRAIN thread tid:%d\n", x);
		}
	}

	usleep(100000);
	awaiting = 0; // Signal all trains to start loading
	usleep(3000000);
	temp = root;
	temp->s = 2;
	pthread_mutex_lock(&main_track_mutex);
	pthread_cond_broadcast(&green_light_cv);
	pthread_mutex_unlock(&main_track_mutex);
	usleep(3000000);
	temp = root->next;
	temp->s = 2;
	pthread_mutex_lock(&main_track_mutex);
	pthread_cond_broadcast(&green_light_cv);
	pthread_mutex_unlock(&main_track_mutex);
	// usleep(3000000);
	// root->next->s =2;

	// return EXIT_SUCCESS;


/*	
	// printf("DISP - 1. Temp | s:%d | ID:%d | b:%c \n", temp->s, temp->id, temp->b);
	
	while (total_dispatched < train_ctr){
		// printf("DISP - LOOP, total_dispatched:%d\n", total_dispatched);
		while (rdy_ctr == 0);
		if (stEA_ctr > 0 || stWE_ctr > 0){ // High PR 
			double E1, W1;
			int total_waiting=0 ,dispatch_id;
			printf("\nDISP - High priority train is ready to depart - EAST:%d, WEST:%d\n", stEA_ctr, stWE_ctr);
			if (stEA_ctr > 0 ){
				E1 = getLoadTime('E');
				// printf("EAST waiting Ld_time:%f\n", E1);
				total_waiting++;
			}
			if (stWE_ctr > 0 ){
				W1 = getLoadTime('W');
				// printf("WEST waiting Ld_time:%f\n", W1);
				total_waiting++;
			}
			if (total_waiting > 1){
				if  (E1 < W1) {
					printf("DISP - >2 Waiting - EAST is smaller, must go first - %f | total_waiting:%d\n",E1, total_waiting);
					dispatch_id = getHeadID('E');
					// dispatch_b = 'E';
				}else{
					printf("DISP - >2 Waiting - WEST is smaller, must go first - %f | total_waiting:%d\n",W1, total_waiting);
					
					dispatch_id = getHeadID('W');
					// dispatch_b = 'W';
				}
			}else {
				if (E1 != 0){
					printf("DISP - Single - EAST is smaller, must go first - %f | total_waiting:%d\n",E1, total_waiting);
					printf("DISP - EAST counter:%d\n", stEA_ctr);
					dispatch_id = getHeadID('E');
					// dispatch_b = 'E';
				}else{
					printf("DISP - Single - WEST is smaller, must go first - %f | total_waiting:%d\n",W1, total_waiting);
					dispatch_id = getHeadID('W');
					// dispatch_b = 'W';
				}
			}
			for(temp = root; temp->id != dispatch_id ; temp = temp->next );
			temp->s = 2; // Granted
			printf("DISP - TO DISPATCH | ID=%d | S:%d | b:%c-%s | p:%d | ld-x:%d-%d | ld_time:%f\n", temp->id, temp->s, temp->b, temp->bound, temp->pr, temp->ld, temp->x, temp->ld_time);
			
			pthread_mutex_lock(&main_track_mutex);
			pthread_cond_broadcast(&green_light_cv);
			pthread_mutex_unlock(&main_track_mutex);
			
			pthread_mutex_lock(&queue_mutex);
			rdy_ctr--;
			removeHead(temp->b);
			pthread_mutex_unlock(&queue_mutex);
			while (temp->s != 4){

				// printf ("DISP - train dispatched status:%d\n", temp->s);
			}
			printf("DISP - Train dispatched GONE | ID:%d | Status(should be 4):%d  \n\n", temp->id, temp->s);
		}
		total_dispatched++;
		// printf("DISP - total_dispatched:%d |train_ctr:%d\n", total_dispatched, train_ctr);
	}

	printf ("DISP - Joining threads\n");


*/



	// Join all running Train threads 
	for (x =0 ; x < train_ctr ; x++){
		if ( pthread_join(tid[x], NULL) != 0){
			fprintf(stderr, "ERROR - Cannot join thread TRAIN tid:%d\n", x);
		}
	}
	printf("\n");
	printStation('E');
	printStation('W');
	printStation('e');
	printStation('w');
	printf("\n==========Dispatcher RAW===========\n");
	for(temp = root; temp != NULL ; temp = temp->next ){
		printf("Id=%d - s:%d, b:%c-%s, p:%d, ld-x:%d-%d, ld_time:%f\n", temp->id, temp->s, temp->b, temp->bound, temp->pr, temp->ld, temp->x, temp->ld_time);
	}
	printf("==================================\n");




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

	printf("==========FINAL RAW===========\n");
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