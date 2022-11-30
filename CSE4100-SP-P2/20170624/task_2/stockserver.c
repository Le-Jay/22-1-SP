/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"

typedef struct { /* Represents a pool of connected descriptors */
	int maxfd; /* Largest descriptor in read_set */
	fd_set read_set; /* Set of all active descriptors */
	fd_set ready_set; /* Subset of descriptors ready for reading */
	int nready; /* Number of ready descriptors from select */
	int maxi; /* High water index into client array */
	int clientfd[FD_SETSIZE]; /* Set of active descriptors */
	rio_t clientrio[FD_SETSIZE]; /* Set of active read buffers */
} pool;

typedef struct item {
	int ID;
	int left_stock;
	struct item* leftChild;
	struct item* rightChild;
	int price;
} Item;

Item* root;

Item* insert(Item* root, int ID, int left_stock, int price){ // insert node to binary tree
	if(root == NULL){
		root = (Item *)malloc(sizeof(Item)); // allocate memory
		root->rightChild = root->leftChild = NULL; // copy values to new node
		root->ID = ID;
		root->left_stock = left_stock;
		root->price = price;
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
		fprintf(fp, "%d %d %d\n", node->ID, node->left_stock, node->price);
		store_tree(fp, node->rightChild);
	}
}

void show_item(int connfd, Item* node)
{
	char buf[1024];
	if(node != NULL) {
		show_item(connfd, node->leftChild);
		sprintf(buf, "%d %d %d\n", node->ID, node->left_stock, node->price);
		Rio_writen(connfd, buf, strlen(buf));
		show_item(connfd, node->rightChild);
	}
}

void buy_item(int connfd, Item* node, int ID, int n) {
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
}

void sell_item(int connfd, Item* node, int ID, int n) {
	Item* stock = search_item(node, ID);
	char buf[1024];
	stock->left_stock += n;
	sprintf(buf, "[sell] success\n");
	Rio_writen(connfd, buf, strlen(buf));
}

void init_pool(int listenfd, pool* p)
{
	/* Initially, there are no connected descriptors */
	int i;
	p->maxi = -1;
	for (i = 0; i < FD_SETSIZE; i++)
		p->clientfd[i] = -1;

	/* Initially, listenfd is only member of select read set */
	p->maxfd = listenfd;
	FD_ZERO(&p->read_set);
	FD_SET(listenfd, &p->read_set);
}

void add_client(int connfd, pool* p)
{
	int i;
	p->nready--;
	for (i = 0; i < FD_SETSIZE; i++) /* Find an available slot */
		if (p->clientfd[i] < 0) {
			/* Add connected descriptor to the pool */
			p->clientfd[i] = connfd;
			Rio_readinitb(&p->clientrio[i], connfd);

			/* Add the descriptor to descr iptor set */
			FD_SET(connfd, &p->read_set);
			/* Update max descriptor and pool high water mark */
			if (connfd > p->maxfd)
				p->maxfd = connfd;
			if (i > p->maxi)
				p->maxi = i;
			break;
		}
	if (i == FD_SETSIZE) /* Couldn't find an empty slot */
		app_error("add_client error : Too many clients");
}

void check_clients(pool *p)
{
	int i, connfd, n, id, num;
	char buf[MAXLINE];
	rio_t rio;
	char cmd[10];

	for (i = 0 ; (i <= p->maxi) && (p->nready > 0) ; i++) {
		connfd = p->clientfd[i];
		rio = p->clientrio[i];

		if((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) {
            p->nready--;
            if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
				cmd[0] = '\0';
				id = 0; num = 0;
				sscanf(buf, "%s %d %d", cmd, &id, &num);
				printf("server received %d bytes(%d)\n", n, connfd);

				if(!strcmp(cmd, "show")) {
					show_item(connfd, root);
					Rio_writen(connfd, "end\n", 4);
				}
				else if(!strcmp(cmd, "buy")) {
					buy_item(connfd,root,id,num);
				}
				else if(!strcmp(cmd, "sell")) {
					sell_item(connfd,root,id,num);
				}
				else Rio_writen(connfd, buf, n);
            }
            /* EOF detected, remove descriptor from pool */
            else {
                Close(connfd);
				FD_CLR(connfd, &p->read_set);
				p->clientfd[i] = -1;
                printf("disconnected %d\n", connfd);
                /* write on stock.txt */
                FILE *fp2 = fopen("stock.txt", "w");
                if(fp2 == NULL) {
					printf("Failed to open source file\n");
					exit(0);
				}
				else {
					store_tree(fp2, root);
					fclose(fp2);
				}
            }

        }
    }
}

int main(int argc, char **argv) 
{
    int listenfd, connfd;
    socklen_t clientlen;
	int id, left, price;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
	static pool pool;
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

	listenfd = Open_listenfd(argv[1]);
	init_pool(listenfd, &pool);

    while (1) {
		pool.ready_set = pool.read_set;
		pool.nready = Select(pool.maxfd + 1, &pool.ready_set, NULL, NULL, NULL);

		if(FD_ISSET(listenfd, &pool.ready_set)) {
			clientlen = sizeof(struct sockaddr_storage); 
			connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
			Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, 
						client_port, MAXLINE, 0);
			printf("Connected to (%s, %s)\n", client_hostname, client_port);
			add_client(connfd, &pool);
		}
		check_clients(&pool);

    }
    exit(0);
}
/* $end echoserverimain */