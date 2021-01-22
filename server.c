#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include "proto.h"
#include "server.h"

// Global variables
int server_sockfd = 0, client_sockfd = 0;
ClientList *root, *now;

//************************************工具函数************************************
void catch_ctrl_c_and_exit(int sig) 
{
    ClientList *tmp;
    while (root != NULL) {
        printf("\nClose socketfd: %d\n", root->data);
        close(root->data); // close all socket include server_sockfd
        tmp = root;
        root = root->link;
        free(tmp);
    }
    printf("Bye\n");
    exit(EXIT_SUCCESS);
}

unsigned long get_file_size(const char *path)  
{  
    unsigned long filesize = -1;      
    struct stat statbuff;  
    if (stat(path, &statbuff) < 0)
    {   
        return filesize;  
    }
    else
    {  
        filesize = statbuff.st_size;  
    }  
    return filesize; 
} 

void itoa(long i, char *string)
{
	long power, j;
	j = i;
	for (power = 1; j >= 10; j /= 10)
    {
		power *= 10;
    }
	for (; power > 0; power /= 10)
	{
		*string++ = '0' + i / power;
		i %= power;
	}
	*string = '\0';
}

//************************************功能函数************************************
void return_ol_host_info(void *p_client)
{
    char sendOlHostInfo[LENGTH_SEND] = {};
    ClientList *np = (ClientList *)p_client;
    ClientList *tmp = root->link;

    while (tmp != NULL) 
    {
        if(tmp->data != np->data)
        {
            strcat(sendOlHostInfo, tmp->name);
            strcat(sendOlHostInfo, " ");
            strcat(sendOlHostInfo, tmp->ip);
            strcat(sendOlHostInfo, "\n");
        }
        tmp = tmp->link;
    }
    send(np->data, sendOlHostInfo, LENGTH_SEND, 0);
}

void transmit(void *p_client)
{
    char targetname[LENGTH_NAME] = {};
    char targetfilename[LENGTH_NAME] = {};
    ClientList *np = (ClientList *)p_client;

    recv(np->data, targetname, LENGTH_NAME, 0);
    recv(np->data, targetfilename, LENGTH_NAME, 0);
    printf("\n开始将文件%s从主机%s转发至主机%s\n", targetfilename, np->name, targetname);
    printf("开始从主机%s接受文件%s\n", np->name, targetfilename);

    FILE *fp = fopen(targetfilename, "w+");
    if (fp == NULL)
    {
        printf("创建文件%s失败\n", targetfilename);
        exit(1);
    }
    else
    {
        printf("创建文件%s成功\n", targetfilename);
    }

	char file_len[10] = {0};
	recv(np->data, file_len, sizeof(file_len), 0);
	long file_size = atoi(file_len); 
    printf("文件大小 = %ld\n",file_size); 
    
    char length_buffer[10] = {0};
    recv(np->data, length_buffer, sizeof(length_buffer), 0);
    int LENGTH_BUFFER = atoi(length_buffer); 
    printf("传输文件块的字节数 = %d\n",LENGTH_BUFFER); 
	char data_buf[LENGTH_BUFFER];
	size_t fwtv, rrtv, len = 0;
	while(1)
	{
		memset(data_buf, 0, sizeof(data_buf));
		rrtv = recv(np->data, data_buf, sizeof(data_buf), 0);
		fwtv = fwrite(data_buf, 1, rrtv, fp);
        printf("[收到字节数: %ld，写入文件字节数：%ld]\n", strlen(data_buf) - 6, strlen(data_buf) - 6);
		fflush(fp);
		len += fwtv;
		if (len == file_size)
		{
			break;
		}
	}
    close(fp);
    printf("成功从主机%s接受文件%s\n", np->name, targetfilename);

    //************************************转发部分************************************
    int target_sockfd;
    ClientList *tmp = root->link;

    while (tmp != NULL)
    {
        if (strcmp(tmp->name, targetname) == 0)
        {
            target_sockfd = tmp->data;
        }
        tmp = tmp->link;
    }
    printf("开始将文件%s转发至主机%s\n", targetfilename, targetname);

    FILE *fp2 = fopen(targetfilename, "r");
    if (fp2 == NULL)
    {
        printf("打开文件%s失败\n", targetfilename);
        exit(1);
    }

    send(target_sockfd, targetfilename, sizeof(targetfilename), 0);
    send(target_sockfd, np->name, sizeof(np->name), 0);
    send(target_sockfd, file_len, sizeof(file_len), 0);
    send(target_sockfd, length_buffer, sizeof(length_buffer), 0);


    size_t frtv;
    while (1)
	{
		memset(data_buf, 0, sizeof(data_buf));
		frtv = fread(data_buf, 1, LENGTH_BUFFER, fp2);
		if (frtv == 0)
		{
			break;
		}
		send(target_sockfd, data_buf, frtv, 0);
        printf("[读取文件字节数: %ld，发送字节数：%ld]\n", strlen(data_buf), strlen(data_buf));
	}
    close(fp2);
    printf("成功将文件%s转发至主机%s\n\n", targetfilename, targetname);

    char message[LENGTH_MSG] = {};
    strncpy(message, "success", LENGTH_MSG);
    send(np->data, message, LENGTH_MSG, 0);
}

void broadcast_pthread(void *fileinfo)
{
    int target_sockfd;
    char filelen[10] = {};
    char length_buffer[10] = {0};
    char sourcename[LENGTH_NAME] = {};
    char targetname[LENGTH_NAME] = {};
    char targetfilename[LENGTH_NAME] = {};
    int LENGTH_BUFFER;
    BroadcastFile *nfileinfo = (BroadcastFile *)fileinfo;
    target_sockfd = nfileinfo->sockfd;
    strcpy(filelen, nfileinfo->filelen);
    strcpy(sourcename, nfileinfo->sourcename);
    strcpy(targetname, nfileinfo->targetname);
    strcpy(targetfilename, nfileinfo->filename);
    LENGTH_BUFFER = nfileinfo->LENGTH_BUFFER;

    // //调试语句
    // pthread_t tid;
    // tid = pthread_self();
    // printf("***\ntid:%u\ntargetname:%s\ntarget_sockfd:%d\n***\n", tid, targetname, target_sockfd);
    // //调试语句

    FILE *fp = fopen(targetfilename, "r");
    if (fp == NULL)
    {
        printf("发送至主机%s的线程打开文件%s失败\n", targetname, targetfilename);
        exit(1);
    }

    send(target_sockfd, targetfilename, sizeof(targetfilename), 0);
    send(target_sockfd, sourcename, sizeof(sourcename), 0);
    send(target_sockfd, filelen, sizeof(filelen), 0);
    itoa((long)LENGTH_BUFFER, length_buffer);
    send(target_sockfd, length_buffer, sizeof(length_buffer), 0);


	char data_buf[LENGTH_BUFFER];
    size_t frtv;
    while (1)
	{
		memset(data_buf, 0, sizeof(data_buf));
		frtv = fread(data_buf, 1, LENGTH_BUFFER, fp);
		if (frtv == 0)
		{
			break;
		}
		send(target_sockfd, data_buf, frtv, 0);
        // printf("[读取文件字节数: %ld，发送字节数：%ld]\n", strlen(data_buf), strlen(data_buf));
	}
    printf("成功将文件%s发送至主机%s\n", targetfilename, targetname);

    close(fp);
    pthread_exit(0);
}

void broadcast(void *p_client)
{
    char targetfilename[LENGTH_NAME] = {};
    ClientList *np = (ClientList *)p_client;

    recv(np->data, targetfilename, LENGTH_NAME, 0);
    printf("\n开始将文件%s从主机%s广播至所有在线主机\n", targetfilename, np->name);
    printf("开始从主机%s接受文件%s\n", np->name, targetfilename);

    FILE *fp = fopen(targetfilename, "w+");
    if (fp == NULL)
    {
        printf("创建文件%s失败\n", targetfilename);
        exit(1);
    }
    else
    {
        printf("创建文件%s成功\n", targetfilename);
    }

	char file_len[10] = {0};
	recv(np->data, file_len, sizeof(file_len), 0);
	long file_size = atoi(file_len); 
    printf("文件大小 = %ld\n",file_size); 
    
    char length_buffer[10] = {0};
    recv(np->data, length_buffer, sizeof(length_buffer), 0);
    int LENGTH_BUFFER = atoi(length_buffer); 
    printf("传输文件块的字节数 = %d\n",LENGTH_BUFFER); 
	char data_buf[LENGTH_BUFFER];
	size_t fwtv, rrtv, len = 0;
	while(1)
	{
		memset(data_buf, 0, sizeof(data_buf));
		rrtv = recv(np->data, data_buf, sizeof(data_buf), 0);
		fwtv = fwrite(data_buf, 1, rrtv, fp);
        printf("[收到字节数: %ld，写入文件字节数：%ld]\n", strlen(data_buf) - 6, strlen(data_buf) - 6);
		fflush(fp);
		len += fwtv;
		if (len == file_size)
		{
			break;
		}
	}
    close(fp);
    printf("成功从主机%s接受文件%s\n", np->name, targetfilename);

    //************************************广播部分************************************
    pthread_t threadid[100];
    int threadnum = 0;
    ClientList *tmp = root->link;
    BroadcastFile *fileinfo = (BroadcastFile *)malloc( sizeof(BroadcastFile) );
    strcpy(fileinfo->filelen, file_len);
    strcpy(fileinfo->sourcename, np->name);
    strcpy(fileinfo->filename, targetfilename);
    fileinfo->LENGTH_BUFFER = LENGTH_BUFFER;

    printf("开始将文件%s广播至所有在线主机\n", targetfilename);
    while (tmp != NULL)
    {
        if(tmp->data != np->data)
        {
            fileinfo->sockfd = tmp->data;
            strcpy(fileinfo->targetname, tmp->name);

            if(pthread_create(&threadid[threadnum], NULL, (void *)broadcast_pthread, (void *)fileinfo) != 0)
            {
                perror("创建线程错误\n");
                exit(EXIT_FAILURE);
            }
            usleep(200);
            threadnum++;
            //调试语句
            // printf("***\ntmp->name:%s\nfileinfo->targetname:%s\nfileinfo->sockfd:%d\nthreadnum:%d\n***\n", tmp->name, fileinfo->targetname, fileinfo->sockfd, threadnum);
            //调试语句
        }
        tmp = tmp->link;
    }

    int i;
    for(i = 0; i < threadnum; i++)
    {
        pthread_join(threadid[i], NULL);
    }
    printf("成功将文件%s从主机%s广播至所有在线主机\n\n", targetfilename, np->name);
    
    char message[LENGTH_MSG] = {};
    strncpy(message, "success", LENGTH_MSG);
    send(np->data, message, LENGTH_MSG, 0);
}

void remove_node(void *p_client)
{
    ClientList *np = (ClientList *)p_client;

    printf("%s(%s)(%d) leave the chatroom.\n", np->name, np->ip, np->data);
    close(np->data);
    if (np == now) { // remove an edge node
        now = np->prev;
        now->link = NULL;
    } else { // remove a middle node
        np->prev->link = np->link;
        np->link->prev = np->prev;
    }
    free(np);
}

void start_run(void *p_client)
{
    int leave_flag = 0;
    char nickname[LENGTH_NAME] = {};
    char recv_command[LENGTH_CMD] = {};
    ClientList *np = (ClientList *)p_client;

    // Naming
    if (recv(np->data, nickname, LENGTH_NAME, 0) <= 0 || strlen(nickname) < 2 || strlen(nickname) >= LENGTH_NAME-1) 
    {
        printf("%s didn't input name.\n", np->ip);
    }
    strncpy(np->name, nickname, LENGTH_NAME);

    for(;;)
    {
        if (leave_flag)
        {
            break;
        }
        int receive = recv(np->data, recv_command, LENGTH_CMD, 0);
        if (receive > 0)
        {
            if (strncmp(recv_command, "1", LENGTH_CMD) == 0)
            {
                return_ol_host_info(np);
                memset(recv_command, 0, LENGTH_CMD);
            }
            if (strncmp(recv_command, "3", LENGTH_CMD) == 0)
            {
                transmit(np);
                memset(recv_command, 0, LENGTH_CMD);
            }
            if (strncmp(recv_command, "4", LENGTH_CMD) == 0)
            {
                broadcast(np);
                memset(recv_command, 0, LENGTH_CMD);
            }
            if (strncmp(recv_command, "0", LENGTH_CMD) == 0)
            {
                remove_node(np);
                memset(recv_command, 0, LENGTH_CMD);
                leave_flag = 1;
            }
        }
        else
        {
            printf("receive error\n");
            leave_flag = 1;
        }
    }
}

int main()
{
    //捕捉ctrl_c信号然后执行catch_ctrl_c_and_exit函数
    signal(SIGINT, catch_ctrl_c_and_exit);

    // Create socket
    server_sockfd = socket(AF_INET , SOCK_STREAM , 0);
    if (server_sockfd == -1) {
        printf("Fail to create a socket.");
        exit(EXIT_FAILURE);
    }

    // Socket information
    struct sockaddr_in server_info, client_info;
    int s_addrlen = sizeof(server_info);
    int c_addrlen = sizeof(client_info);
    memset(&server_info, 0, s_addrlen);
    memset(&client_info, 0, c_addrlen);
    server_info.sin_family = PF_INET;
    server_info.sin_addr.s_addr = INADDR_ANY;
    server_info.sin_port = htons(8888);

    // Bind and Listen
    bind(server_sockfd, (struct sockaddr *)&server_info, s_addrlen);
    listen(server_sockfd, 5);

    // Print Server IP
    getsockname(server_sockfd, (struct sockaddr*) &server_info, (socklen_t*) &s_addrlen);
    printf("Start Server on: %s:%d\n", inet_ntoa(server_info.sin_addr), ntohs(server_info.sin_port));

    // Initial linked list for clients
    root = newNode(server_sockfd, inet_ntoa(server_info.sin_addr));
    now = root;

    while (1) {
        client_sockfd = accept(server_sockfd, (struct sockaddr*) &client_info, (socklen_t*) &c_addrlen);

        // Print Client IP
        getpeername(client_sockfd, (struct sockaddr*) &client_info, (socklen_t*) &c_addrlen);
        printf("Client %s:%d come in.\n", inet_ntoa(client_info.sin_addr), ntohs(client_info.sin_port));

        // Append linked list for clients
        ClientList *c = newNode(client_sockfd, inet_ntoa(client_info.sin_addr));
        c->prev = now;
        now->link = c;
        now = c;

        pthread_t id;
        if (pthread_create(&id, NULL, (void *)start_run, (void *)c) != 0) {
            perror("Create pthread error!\n");
            exit(EXIT_FAILURE);
        }
    }
    return 0;
}