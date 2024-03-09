#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>

#define herr(msg) \
    {perror(msg); exit(EXIT_FAILURE);}

#define MTRY(v, msg) \
    if(-1 == (v)) herr(msg)

#define NTRY(v, msg) \
    if(NULL == (v)) herr(msg)

#define SV_IP      "0"
#define SV_PORT    2023
#define STR_MXL    255
#define MX_CON     10
#define MX_THREADS 10

char command[STR_MXL], response[STR_MXL];

struct th_data
{
    pthread_t th;
    int id;
    int d;
    int busy;
} *tdpool;

int give_th_id()
{
    for (int i = 0; i < MX_THREADS; i++)
        if (tdpool[i].busy == 0) {
            tdpool[i].busy = 1;
            return i;
        }
    return -1;
}

void peerServer();
void sendMessage2Peer();
int peer_svport;
int check_paths(const char*);
int share_dir(char*);
void download_files(char*);
int sv_d;
char down_path[255] = "download-";

int main()
{
    struct sockaddr_in server;
    MTRY(sv_d = socket(AF_INET, SOCK_STREAM, 0), "[peer] err: socket");
    
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(SV_IP);
    server.sin_port = htons(SV_PORT);

    MTRY(connect(sv_d, (struct sockaddr*) &server, sizeof(server)), "[peer] err: connect");
    read(sv_d, &peer_svport, 4);
    //peer_svport--;

    int pid = fork();
    if (pid == 0)
        peerServer();
    else {

    while (1) {
        printf("> "); fflush(stdout);
        fgets(command, STR_MXL, stdin);
        int len = strlen(command);
        if (len <= 1) continue;
        command[len - 1] = '\0';

        if (strncmp(command, "share", 5) == 0) {
            if (check_paths(command) == 0)
                continue;
        }

        if (strncmp(command, "sdr", 3) == 0) {
            if (share_dir(command) == 0)
                continue;
        }

        if (strncmp(command, "cls", 5) == 0) {
            system("clear");
            continue;
        }

        if (strncmp(command, "message", 7) == 0) {
            char* token = strtok(command, " ");
            token = strtok(NULL, " ");
            int port = atoi(token);
            token = strtok(NULL, " ");

            sendMessage2Peer(token, port);
            continue;
        }

        write(sv_d, command, STR_MXL);

        char* token = strtok(command, " ");
        if (strcmp(token, "quit") == 0) {
            break;
        }

        else if (strcmp(token, "users") == 0) {
            read(sv_d, response, STR_MXL);
            printf("%s*\n", response);
        }

        else if (strcmp(token, "files") == 0) {
            read(sv_d, response, STR_MXL);
            printf("%s*\n", response);
        }

        else if (strcmp(token, "search") == 0) {
            read(sv_d, response, STR_MXL);
            printf("%s*\n", response);
        }

        else if (strcmp(token, "download") == 0) {
            download_files(token);
        }

        else if (strcmp(token, "login") == 0) {
            token = strtok(NULL, " ");
            strcat(down_path, token);
            int s = mkdir(down_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        }

        read(sv_d, response, STR_MXL);
        printf("[sv] %s*\n", response);
    }

    }

    return 0;
}

int check_paths(const char* comm)
{
    char cpy[STR_MXL];
    strcpy(cpy, comm);
    char* token = strtok(cpy, " ");
    token = strtok(NULL, " ");
    if (token == NULL) {
        printf("No arguments\n");
        return 0;
    }

    int ok = 1;
    while (token != NULL) {
        struct stat path_stat;
        if (stat(token, &path_stat)) {
            printf("Invalid file path: %s\n", token);
            ok = 0;
        }

        token = strtok(NULL, " ");
    }

    return ok;
}

int share_dir(char* comm)
{
    char cpy[STR_MXL];
    strcpy(cpy, comm);
    char* token = strtok(cpy, " ");
    token = strtok(NULL, "  ");
    printf("%s\n", token);
    struct stat path_stat, file_stat;

    int res = stat(token, &path_stat);
    if (res == 0 && S_ISDIR(path_stat.st_mode)) {
        DIR* dir;
        struct dirent* entry;
        dir = opendir(token);
        if (dir == NULL) {
            perror("Unable to open directory");
            return 0;
        }

        strcpy(comm, "share");
        char file[1024];
        while ((entry = readdir(dir)) != NULL) {
            snprintf(file, sizeof(file), "%s/%s", token, entry->d_name);

            if (stat(file, &file_stat) == 0) {
                if (S_ISREG(file_stat.st_mode)) {
                    strcat(comm, " ");
                    strcat(comm, file);
                }
            } 
            else {
                perror("Unable to get file status");
                return 0;
            }
        }
    }
    printf("[%s]\n", comm);

    return 1;
}

void sendMessage2Peer(const char* msg, int port)
{
    struct sockaddr_in peer;
    int p_d;
    MTRY(p_d = socket(AF_INET, SOCK_STREAM, 0), "[peer] err: socket");
    
    peer.sin_family = AF_INET;
    peer.sin_addr.s_addr = inet_addr(SV_IP);
    peer.sin_port = htons(port);

    MTRY(connect(p_d, (struct sockaddr*) &peer, sizeof(peer)), "[peer] err: connect");
    
    write(p_d, msg, STR_MXL);

    char res[STR_MXL];
    read(p_d, res, STR_MXL);
    printf("[peer:%d] %s*\n", port, res); fflush(stdout);

    close(p_d);
}

const char *extr_file_name(const char* path) 
{
    const char *file_name = strrchr(path, '/');
    if (file_name != NULL)
        file_name++;
    else
        file_name = path;

    return file_name;
}

void download_files(char* token)
{
    int r, peer_port, f_cnt = 0;
    read(sv_d, &r, 4);
    if (r < 0) return;
    char peer_ip[255], paths[20][255];
    read(sv_d, peer_ip, STR_MXL);
    read(sv_d, &peer_port, 4);
    //printf("[%s:%d]\n", ip, port);

    token = strtok(NULL, " ");
    token = strtok(NULL, " ");
    int ok, irr = 0;
    while (token != NULL){
        read(sv_d, &ok, 4);
        if (ok) {
            read(sv_d, paths[f_cnt++], STR_MXL);
            printf("%s\n", paths[f_cnt - 1]);
        }
        else {
            printf("File '%s' does not exist\n", token);
            irr = 1;
        }

        token = strtok(NULL, " ");
    }
    if (irr) return;


    struct sockaddr_in peer;
    int p_d;
    MTRY(p_d = socket(AF_INET, SOCK_STREAM, 0), "[peer] err: socket");
    
    peer.sin_family = AF_INET;
    peer.sin_addr.s_addr = inet_addr(peer_ip);
    peer.sin_port = htons(peer_port);

    MTRY(connect(p_d, (struct sockaddr*) &peer, sizeof(peer)), "[peer] err: connect");

    char chunk[4096], file_path[5 * STR_MXL], file_name[STR_MXL];
    write(p_d, &f_cnt, 4); 
    for (int i = 0; i < f_cnt; i++) {
        write(p_d, paths[i], STR_MXL); 

        strcpy(file_name, extr_file_name(paths[i]));
        sprintf(file_path, "%s/%s", down_path, file_name);

        FILE *file = fopen(file_path, "wb");
        if (file == NULL) {
            perror("err: download_files fopen");
            continue;
        }
        
        int bytesRead;
        while ((bytesRead = recv(p_d, chunk, sizeof(chunk), 0)) > 0) {
            fwrite(chunk, 1, bytesRead, file);
        }
        fclose(file);
    }
}

static void* treat(void* arg);
void handle_request(void*);
void handle_request_download(void*);

void peerServer()
{
    tdpool = calloc(sizeof(struct th_data), MX_THREADS); 

    struct sockaddr_in server;
    struct sockaddr_in peer;

    int sv_d;
    MTRY(sv_d = socket(AF_INET, SOCK_STREAM, 0), "[peer_sv] err: socket");

    int on = 1;
    setsockopt(sv_d, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	bzero(&server, sizeof(server));
	bzero(&peer, sizeof(peer));

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(peer_svport);

    MTRY(bind(sv_d, (struct sockaddr*) &server, sizeof(struct sockaddr)), "[peer_sv] err: bind");
	MTRY(listen(sv_d, MX_CON), "[peer_sv] err: listen");

    printf("[peer_sv] Waiting on port %d...\n", peer_svport);
    fflush(stdout);

    while (1) {
        int p_d, cl_szof = sizeof(peer);
        
        if ((p_d = accept(sv_d, (struct sockaddr*) &peer, &cl_szof)) == -1) {
            perror("[peer_sv] err: accept\n");
            continue;
        }

        int id = give_th_id(tdpool); 

        //char* ip = inet_ntoa(peer.sin_addr);
        //int port = ntohs(peer.sin_port);
        //printf("Connection from %s:%d\n", ip, port);

        tdpool[id].id = id;
        tdpool[id].d = p_d;

        pthread_create(&tdpool[id].th, NULL, &treat, tdpool + id); 
    }
}


static void* treat(void* arg)
{
    struct th_data td = *((struct th_data*) arg);

    pthread_detach(pthread_self());
    
    //handle_request((struct th_data*) arg);
    handle_request_download((struct th_data*) arg);
    
    //pthread_detach(pthread_self());
    pthread_t new_th;
    tdpool[td.id].th = new_th;
    tdpool[td.id].busy = 0;

    close(td.d);
    return NULL;
}

void handle_request_download(void* arg)
{
    struct th_data td = *((struct th_data*) arg);

    char chunk[4096], req[STR_MXL];
    int f_cnt;
    read(td.d, &f_cnt, 4);
    for (int i = 0; i < f_cnt; i++) {
        read(td.d, req, STR_MXL);
        printf("[th %d] %s*\n", td.id, req); fflush(stdout);

        FILE *file = fopen(req, "rb");
        if (file == NULL)
            perror ("err: handle_request_download fopen");

        int bytesRead;
        while ((bytesRead = fread(chunk, 1, sizeof(chunk), file)) > 0) {
            send(td.d, chunk, bytesRead, 0);
        }
        fclose(file);
    }
}

void handle_request(void* arg)
{
    struct th_data td = *((struct th_data*) arg);

    printf("[th %d] Waiting for message...\n", td.id);

    char req[STR_MXL];

    if (read(td.d, req, STR_MXL) <= 0) {
        printf("[th %d] ", td.id);
        perror("read from client\n");
        return;
    }
    else {
        printf("[th %d] %s*\n", td.id, req);
    }

    strcpy(response, "Message received");
    if (write(td.d, response, STR_MXL) <= 0) {
        printf("[th %d] ", td.id);
        perror("write to client\n");
    }
}

