#include "station.h"
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int ste_ctr = 0, stEA_ctr = 0, stw_ctr = 0, stWE_ctr = 0;
struct Node *root_w = NULL, *root_WE = NULL, *root_e = NULL, *root_EA = NULL, *temp = NULL;

int removeTrain(char b, int id){
	assert(b == 'e' || b == 'w' || b == 'E' || b =='W');
	switch (b){
		case 'e':
			if (ste_ctr == 1 || root_e->id == id){
				removeHead('e');
				return 0;
			}else{
				temp = root_e;
				ste_ctr--;		
			}
			break;
		case 'w':
			if (stw_ctr == 1 || root_w->id == id){
				removeHead('w');
				return 0;
			}else{
				temp = root_w;
				stw_ctr--;		
			}
			break;
		case 'E':
			if (stEA_ctr == 1 || root_EA->id == id){
				removeHead('E');
				return 0;
			}else{
				temp = root_EA;
				stEA_ctr--;		
			}
			break;
		case 'W':
			if (stWE_ctr == 1 || root_WE->id == id){
				removeHead('W');
				return 0;
			}else{
				temp = root_WE;
				stWE_ctr--;		
			}
			break;
	}

	struct Node *prev;

	for (prev = temp, temp = temp->next ; temp->id != id ; prev = prev->next, temp = temp->next);
	prev->next = temp->next;
	temp->next = NULL;
	// printf("removeTrain | id:%d | b:%c | ld:%f \n", temp->id, temp->b, temp->ld_time);

	free(temp);
	return 0;
}

int findMinLd(char b){
	assert(b == 'e' || b == 'w' || b == 'E' || b =='W');
	int min_id = -1;
	double min_ld_time = (double)9999999999.999999;
	switch (b){
		case 'e':
			assert(root_e != NULL);
			temp = root_e;
			break;
		case 'w':
			assert(root_w != NULL);
			temp = root_w;
			break;
		case 'E':
			assert(root_EA != NULL);
			temp = root_EA;
			break;
		case 'W':
			assert(root_WE != NULL);
			temp = root_WE;
			break;
	}
	for ( ; temp != NULL ; temp = temp->next){
		if (temp->ld_time < min_ld_time){
			min_id = temp->id;
			min_ld_time = temp->ld_time;
		}
	}

	// printf("findMinLd |id:%d | ld_time:%f\n", min_id, min_ld_time);
	return min_id;
}

int getSecondID(char b){
	assert(b == 'e' || b == 'w' || b == 'E' || b =='W');
	switch (b){
		case 'e':
			assert(root_e != NULL);
			return root_e->next->id;
		case 'w':
			assert(root_w != NULL);
			return root_w->next->id;
		case 'E':
			assert(root_EA != NULL);
			return root_EA->next->id;
		case 'W':
			assert(root_WE != NULL);
			return root_WE->next->id;
	}
	return 0;
}
int getHeadID(char b){
	assert(b == 'e' || b == 'w' || b == 'E' || b =='W');
	switch (b){
		case 'e':
			assert(root_e != NULL);
			return root_e->id;
		case 'w':
			assert(root_w != NULL);
			return root_w->id;
		case 'E':
			assert(root_EA != NULL);
			return root_EA->id;
		case 'W':
			assert(root_WE != NULL);
			return root_WE->id;
	}
	return 0;
}


double getLoadTime(char b){
	assert(b == 'e' || b == 'w' || b == 'E' || b =='W');
	switch (b){
		case 'e':
			assert(root_e != NULL);
			return root_e->ld_time;
		case 'w':
			assert(root_w != NULL);
			return root_w->ld_time;
		case 'E':
			assert(root_EA != NULL);
			return root_EA->ld_time;
		case 'W':
			assert(root_WE != NULL);
			return root_WE->ld_time;
	}
	return 0;
}

void addNode (int train_id, int train_pr, char train_b, double train_ld_time){
	assert(train_b == 'e' || train_b == 'w' || train_b == 'E' || train_b =='W');

	Node *new_node = (Node*)malloc(sizeof(Node));
	memset(new_node, 0, sizeof(Node));
	Node *temp;
	int flag = 0;

	new_node->id = train_id;
	new_node->pr = train_pr;
	new_node->b = train_b;
	new_node->ld_time = train_ld_time;
	// printf("addNode - TrainID:%d - bound:%c|pr:%d \n", train_id, train_b, train_pr);

	//Station has waiting trains
	if ((train_b == 'e') && (ste_ctr > 0)) {
		temp = root_e;
		ste_ctr++;
		flag =1;
	}else if ((train_b == 'w') && (stw_ctr > 0)){
		temp = root_w;
		stw_ctr++;
		flag =1;
	}else if ((train_b == 'E') && (stEA_ctr > 0)){
		temp = root_EA;
		stEA_ctr++;
		flag =1;
	}else if ((train_b == 'W') && (stWE_ctr > 0)){
		temp = root_WE;
		stWE_ctr++;
		flag =1;
	}

	//Station empty
	if ((train_b == 'e') && (ste_ctr == 0)) {
		root_e = new_node;
		ste_ctr++;
	}else if ((train_b == 'w') && (stw_ctr == 0)){
		(root_w = new_node);
		stw_ctr++;

	}else if ((train_b == 'E') && (stEA_ctr == 0)){
		root_EA = new_node;
		stEA_ctr++;

	}else if ((train_b == 'W') && (stWE_ctr == 0)){
		root_WE = new_node;
		stWE_ctr++;
	}

	if(flag){
		for ( ; temp->next != NULL ; temp = temp->next){
		}
		temp->next = new_node;
		new_node->next = NULL;
	}
}

int waitingLine(char b){
	assert(b == 'e' || b == 'w' || b == 'E' || b =='W');
	switch (b){
		case 'e':
			return ste_ctr;
		case 'w':
			return stw_ctr;
		case 'E':
			return stEA_ctr;
		case 'W':
			return stWE_ctr;
	}
	return 0;
}

int printStation(char b){
	assert(b == 'e' || b == 'w' || b == 'E' || b =='W');

	int x = 0;
	printf("*********** %c - Printing Station***********\n",b);


	switch (b){
		case 'e':
			temp = root_e;
			x = ste_ctr;
			break;
		case 'w':
			temp = root_w;
			x = stw_ctr;
			break;
		case 'E':
			temp = root_EA;
			x = stEA_ctr;
			break;
		case 'W':
			temp = root_WE;
			x = stWE_ctr;
			break;
	}

	if (temp == NULL){fprintf(stderr,"ERROR - Station is empty, could not print list\n"); return 1;}

	printf("Number of trains waiting:%d\n", x);
	for ( ; temp != NULL ; temp = temp->next){
		printf("TrainID:%d - bound:%c|pr:%d |ld:%f\n", temp->id, temp->b, temp->pr, temp->ld_time);
	}
	printf("*********************************************\n");
	return 0;
}
int removeHead(char b){
	assert(b == 'e' || b == 'w' || b == 'E' || b =='W');
	
	if (b == 'e') {
		if (root_e == NULL){fprintf(stderr, "Error - Empty station '%c'\n", b);}
		temp = root_e;
		root_e = temp->next;
		ste_ctr--;
	}
	else if (b == 'w'){
		if (root_w == NULL){fprintf(stderr, "Error - Empty station '%c'\n", b); return 1;}
		temp = root_w;
		root_w = temp->next;
		stw_ctr--;
	}
	else if (b == 'E'){
		if (root_EA == NULL){fprintf(stderr, "Error - Empty station '%c'\n", b); return 1;}
		temp = root_EA;
		root_EA = temp->next;
		stEA_ctr--;
	}
	else if (b == 'W'){
		if (root_WE == NULL){fprintf(stderr, "Error - Empty station '%c'\n", b); return 1;}
		temp = root_WE;
		root_WE = temp->next;
		stWE_ctr--;
	}

	// printf("Removed - TrainID:%d - bound:%c|pr:%d - St,counter:%d\n", temp->id, temp->b, temp->pr, x);
	free(temp);
	return 0;


}

