#ifndef LIST
#define LIST

typedef struct ClientNode {
    int data;
    struct ClientNode* prev;
    struct ClientNode* link;
    char ip[16];
    char name[31];
} ClientList;

ClientList *newNode(int sockfd, char* ip) {
    ClientList *np = (ClientList *)malloc( sizeof(ClientList) );
    np->data = sockfd;
    np->prev = NULL;
    np->link = NULL;
    strncpy(np->ip, ip, 16);
    strncpy(np->name, "NULL", 5);
    return np;
}

typedef struct BroadcastFileNode {
    int sockfd;
    char filelen[10];
    char sourcename[31];
    char targetname[31];
    char filename[31];
    int LENGTH_BUFFER;
} BroadcastFile;

#endif // LIST