/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"
#define NTHREADS 30
#define SBUFSIZE 200

typedef struct{
	int *buf; // buffer array
	int n; // maximum number of slots
	int front; // buf[front+1 % n] is first item
	int rear; // buf[rear % n] is last item
	sem_t mutex; // protects accesses to buf
	sem_t slots; // counts available slots
	sem_t items; // counts available items
}sbuf_t;

typedef struct item {
	int ID;
	int left_stock;
	struct item* leftChild;
	struct item* rightChild;
	int price;
	int readcnt; // # of readers in critical section now
	sem_t mutex; //protects access to shared readcnt var
	sem_t w; // controls access to the c.s that access shared obj
} Item;

void echo(int connfd, Item* node);
void sbuf_init(sbuf_t* sp, int n);
void sbuf_deinit(sbuf_t* sp);
void sbuf_insert(sbuf_t* sp, int item);
int sbuf_remove(sbuf_t* sp);
void print_tree(Item* node);
void delete_tree(Item* node);
void store_tree(FILE *fp, Item* node);

sbuf_t sbuf; // buffer
static sem_t mutex;
int byte_cnt = 0; // number of total byte
char str[MAXLINE]; // input
Item* root;

void *thread(void *vargp)
{
	Pthread_detach(pthread_self());

	while (1) {
			int connfd = sbuf_remove(&sbuf);
			echo(connfd, root);
			Close(connfd);
			FILE *fp2 = fopen("stock.txt", "w");
			if (fp2 == NULL) {
				printf("Failed to open the source");
				exit(0);
			}
			else {
				store_tree(fp2, root);
				fclose(fp2);
			}
		}
}


void sbuf_init(sbuf_t *sp, int n){
	sp->buf = Calloc(n, sizeof(int));
	sp->n = n; // buffer holds max of n items
	sp->front = sp->rear = 0; // empty buffer
	Sem_init(&sp->mutex, 0, 1); // binary semaphore for locking
	Sem_init(&sp->slots, 0, n);
	Sem_init(&sp->items, 0, 0);
}

void sbuf_deinit(sbuf_t *sp){ // clean up buffer
	Free(sp->buf);
}

void sbuf_insert(sbuf_t *sp, int item){ // insert item onto the rear of shared buffer
	P(&sp->slots); // wait for available slot
	P(&sp->mutex); // lock buffer
	sp->buf[(++sp->rear)%(sp->n)] = item; // insert item
	V(&sp->mutex); // unlock buffer
	V(&sp->items); // announce available item
}

int sbuf_remove(sbuf_t *sp){ // remove and return the first item from buffer
	int item;
	P(&sp->items); // wait for available item
	P(&sp->mutex); // lock buffer
	item = sp->buf[(++sp->front)%(sp->n)]; // remove item
	V(&sp->mutex); // unlock buffer
	V(&sp->slots); // announce available slot
	return item;
}

Item* insert(Item* root, int ID, int left_stock, int price){ // insert node to binary tree
	if(root == NULL){
		root = (Item *)malloc(sizeof(Item)); // allocate memory
		root->rightChild = root->leftChild = NULL; // copy values to new node
		root->ID = ID;
		root->left_stock = left_stock;
		root->price = price;
		Sem_init(&(root->mutex),0,1);
		Sem_init(&(root->w),0,1); //-> in set??
		return root;
	}
	else { // find the position for new node
		if(ID < root->ID)
			root->leftChild = insert(root->leftChild, ID, left_stock, price);
		else
			root->rightChild = insert(root->rightChild, ID, left_stock, price);
	}
	return root;
}

Item* search_item(Item* ptr, int key)
{
	while(ptr != NULL && ptr->ID != key) {
		if(ptr->ID < key) ptr = ptr->rightChild;
		else ptr = ptr->leftChild;
	}
	return ptr;
}

void print_tree(Item* node)
{
	if(node != NULL) {
		print_tree(node->leftChild);
		printf("%d %d %d\n", node->ID, node->left_stock, node->price);
		print_tree(node->rightChild);
	}
}

void delete_tree(Item* node)
{
	if(node != NULL) {
		delete_tree(node->leftChild);
		delete_tree(node->rightChild);
		free(node);
	}
}

void store_tree(FILE *fp, Item* node)
{
	if(node != NULL) {
		store_tree(fp, node->leftChild);

		P(&(node->mutex)); //mutex lock
		node->readcnt++;
		if(node->readcnt == 1)
			P(&(node->w));
		V(&(node->mutex));


		fprintf(fp, "%d %d %d\n", node->ID, node->left_stock, node->price);

		P(&(node->mutex));
		node->readcnt--;
		if(node->readcnt == 0)
			V(&(node->w));
		V(&(node->mutex));

		store_tree(fp, node->rightChild);
	}
}

void show_item(int connfd, Item* node)
{
	char buf[1024];
	if(node != NULL) {
		show_item(connfd, node->leftChild);
		P(&(node->mutex)); //mutex lock
		node->readcnt++;
		if(node->readcnt == 1)
			P(&(node->w));
		V(&(node->mutex));

		sprintf(buf, "%d %d %d\n", node->ID, node->left_stock, node->price);
		Rio_writen(connfd, buf, strlen(buf));

		P(&(node->mutex));
		node->readcnt--;
		if(node->readcnt == 0)
			V(&(node->w));
		V(&(node->mutex));

		show_item(connfd, node->rightChild);
	}
}

void buy_item(int connfd, Item* node, int ID, int n) {
	P(&(node->w));
	Item* stock = search_item(node, ID);
	char buf[1024];
	if ((stock->left_stock - n) >= 0) { //left stock > 0, buy amount of n
		stock->left_stock -= n;
		sprintf(buf, "[buy] success\n");
		Rio_writen(connfd, buf, strlen(buf));
	}
	else {
		sprintf(buf, "Not enough left stock\n");
		Rio_writen(connfd, buf, strlen(buf));
	}
	V(&(node->w));
}

void sell_item(int connfd, Item* node, int ID, int n) {
	P(&(node->w));
	Item* stock = search_item(node, ID);
	char buf[1024];
	stock->left_stock += n;
	sprintf(buf, "[sell] success\n");
	Rio_writen(connfd, buf, strlen(buf));
	P(&(node->w));
}

int main(int argc, char **argv) 
{
    int listenfd, connfd;
    socklen_t clientlen;
	int id, left, price;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
	pthread_t tid;
	FILE* fp = fopen("stock.txt", "r");
	if(fp == NULL) {
		fprintf(stderr, "txt file does not exist.\n");
		exit(0);
	}

    if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
    }

	while(!feof(fp)) {
		fscanf(fp, "%d %d %d", &id, &left, &price);
		root = insert(root, id, left, price);
	}
	fclose(fp);
	Sem_init(&mutex, 0, 1);
	listenfd = Open_listenfd(argv[1]);
	sbuf_init(&sbuf, SBUFSIZE);

	for(int i = 0 ; i < NTHREADS ; i++)
		Pthread_create(&tid, NULL, thread, NULL);

    while (1) {
		clientlen = sizeof(struct sockaddr_storage); 
		connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
		Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, 
					client_port, MAXLINE, 0);
		printf("Connected to (%s, %s)\n", client_hostname, client_port);
		sbuf_insert(&sbuf, connfd);
    }
	delete_tree(root);
    exit(0);
}
/* $end echoserverimain */