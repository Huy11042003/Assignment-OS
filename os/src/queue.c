#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
        if (q == NULL) return 1;
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
        /* TODO: put a new process to queue [q] */

        //If q or proc is empty, we do nothing and return
        if(q==NULL || proc==NULL) return;

        //Check if the queue is full or not
        if(q->size==MAX_QUEUE_SIZE){
                return;         //if full -> do nothing and return
        } else {        //if not, we continue adding proc to this q and increase the size
                q->proc[q->size]=proc;
                q->size++;
        }
}

struct pcb_t * dequeue(struct queue_t * q) {
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */

        if(!empty(q)){
                int highest=q->proc[0]->prio;   //initialize highest prio at 0th element
                int highestindex=0;     //initialize highest prio position at 0th element
                for(int i=0; i<q->size; i++){
                        if(q->proc[i]->prio < highest){
                                highestindex=i;
                                highest=q->proc[i]->prio;
                        }
                }
                struct pcb_t *temp = q->proc[highestindex];
                if(highestindex<q->size){
                        for(int i=highestindex; i<q->size-1; i++){        //shift left the right side of dequeue element
                                q->proc[i]=q->proc[i+1];
                        }
                }
                q->proc[q->size]=NULL;
                q->size--;
                return temp;
        }
	return NULL;
}

