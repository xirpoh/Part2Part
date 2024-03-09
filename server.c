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
#include <sqlite3.h>
#include <regex.h>

#define herr(msg) \
    {perror(msg); exit(EXIT_FAILURE);}

#define MTRY(v, msg) \
    if(-1 == (v)) herr(msg)

#define NTRY(v, msg) \
    if(NULL == (v)) herr(msg)

#define SV_IP      "0"
#define SV_PORT    2023
#define MX_CON     10
#define MX_THREADS 10
#define SZ_CHUNK   4096
#define STR_MXL    255
#define MX_FILES   16

sqlite3* db;

int sqlite_exec(sqlite3* db, const char* sql)
{
    char* errmsg;
    if (sqlite3_exec(db, sql, 0, 0, &errmsg) != SQLITE_OK) {
        printf("[db] err: %s\n", errmsg);
        sqlite3_free(errmsg);
        sqlite3_close(db);
        exit(1);
    }

    return 1;
}

sqlite3* create_db(sqlite3* db)
{

    if (sqlite3_open("users.db", &db) != SQLITE_OK) {
        sqlite3_close(db);
        herr("Failed to open database");
    }

    char sql[1024];
    sprintf(sql, "DROP TABLE IF EXISTS users;"
                 "CREATE TABLE users (id INTEGER NOT NULL, ip VARCHAR(255) NOT NULL, port INTEGER NOT NULL, file VARCHAR(255), path VARCHAR(255));");

    if (sqlite_exec(db, sql))
        printf("Database created\n");
}

/*
sqlite3* add_file(sqlite3* db, user_data* u)
{
    char sql[1024];
    printf("%s\n", u->file[u->file_cnt - 1]);
    sprintf(sql, "INSERT INTO users VALUES (%d, '%s', %d, '%s', '%s');", 
                u->id, u->ip, u->port, u->file[u->file_cnt - 1], u->path[u->file_cnt - 1]);

    if (sqlite_exec(db, sql))
        printf("file added");
}*/

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

struct user_data
{
    int logged;
    int id;
    char name[255];
    char ip[255];
    int port;
    int file_cnt;
    char file[MX_FILES][STR_MXL];
    char path[MX_FILES][STR_MXL];
} *user;


static void* treat(void*);
void handle_request(void*);

int id, user_cnt = 0;
int sv_d;

char req[STR_MXL], response[STR_MXL];

int main()
{
    create_db(db);

    user = calloc(sizeof(struct user_data), MX_CON);
    tdpool = calloc(sizeof(struct th_data), MX_THREADS); 

    struct sockaddr_in server;
    struct sockaddr_in client;

    MTRY(sv_d = socket(AF_INET, SOCK_STREAM, 0), "[sv] err: socket");

    int opt = 1;
    setsockopt(sv_d, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	bzero(&server, sizeof(server));
	bzero(&client, sizeof(client));

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(SV_PORT);

    MTRY(bind(sv_d, (struct sockaddr*) &server, sizeof(struct sockaddr)), "[sv] err: bind");
	MTRY(listen(sv_d, MX_CON), "[sv] err: listen");

    printf("[sv] Waiting on port %d...\n", SV_PORT);
    fflush(stdout);

    while (1) {
        int cl_d, cl_szof = sizeof(client);
        
        if ((cl_d = accept(sv_d, (struct sockaddr*) &client, &cl_szof)) == -1) {
            perror("[sv] err: accept\n");
            continue;
        }
        user_cnt++;

        id = give_th_id(tdpool); 

        user[id].logged = 0;
        user[id].file_cnt = 0;
        user[id].id = id;
        strcpy(user[id].ip, inet_ntoa(client.sin_addr));
        user[id].port = ntohs(client.sin_port) - 1;
        printf("Connection from %s:%d\n", user[id].ip, user[id].port);
        write(cl_d, &user[id].port, 4);

        tdpool[id].id = id;
        tdpool[id].d = cl_d;

        pthread_create(&tdpool[id].th, NULL, &treat, tdpool + id); 
    }

    return 0;
}

static void* treat(void* arg)
{
    struct th_data td = *((struct th_data*) arg);

    pthread_detach(pthread_self());
    
    handle_request((struct th_data*) arg);

    pthread_t new_th;
    tdpool[td.id].th = new_th;
    tdpool[td.id].busy = 0;

    close(td.d);
    return NULL;
}

void search_files(char* args, int th_id, int cd);
void share_files(char* args, int th_id, int cd);
void download_files(char* args, int th_id, int cd);
void login_user(char* args, int th_id, int cd);
void list_users(char* args, int th_id, int cd);
void list_files(char* args, int th_id, int cd);

void send_response(const char* res, int th_id, int cd)
{
    if (write(cd, res, STR_MXL) <= 0) {
        printf("[th %d] ", th_id);
        perror("write to client\n");
    }
}

void handle_request(void* arg)
{
    struct th_data td = *((struct th_data*) arg);

    printf("[th %d] Waiting for message...\n", td.id);
    fflush(stdout);

    while (1) {
        if (read(td.d, req, STR_MXL) <= 0) {
            printf("[th %d] ", td.id);
            perror("read from client\n");
            break;
        }
        else {
            printf("[th %d] \n%s*\n", td.id, req);
            fflush(stdout);
        }

        char* token = strtok(req, " ");
        if (!strcmp(token, "login"))
            login_user(token, td.id, td.d);
        
        else if (!strcmp(token, "search"))
            search_files(token, td.id, td.d);

        else if (!strcmp(token, "share"))
            share_files(token, td.id, td.d);

        else if (!strcmp(token, "download"))
            download_files(token, td.id, td.d);

        else if (!strcmp(token, "files"))
            list_files(token, td.id, td.d);

        else if (!strcmp(token, "users"))
            list_users(token, td.id, td.d);

        else if (!strcmp(token, "quit")) {
            printf("[th %d] disconnected\n", td.id);
            user[td.id].logged = 0;
            fflush(stdout);
            break;
        }

        else {
            send_response("Unknown command", td.id, td.d);
        }
    }
}

void search_files(char* args, int th_id, int cd)
{
    args = strtok(NULL, " ");
    if (args == NULL) {
        send_response("No arguments", th_id, cd);
        return;
    }
    regex_t regex;
    char err_msg[100];
    char response[STR_MXL] = "", line[5 * STR_MXL];

    while (args != NULL) {
        printf("%s*\n", args);

        int com_res = regcomp(&regex, args, 0);
        if (com_res) {
            regerror(com_res, &regex, err_msg, sizeof(err_msg));
            fprintf(stderr, "Regex compilation failed: %s\n", err_msg);
        }

        for (int i = 0; i < user_cnt; i++) { 
            if (user[i].logged == 0) continue;
            int fc = user[i].file_cnt;
            for (int j = 0; j < fc; j++) {
                int match_res = regexec(&regex, user[i].file[j], 0, NULL, 0);

                if (!match_res) {
                    sprintf(line, "[%s] %s\n", user[i].name, user[i].file[j]);
                    strcat(response, line);
                }
                else if (match_res != REG_NOMATCH) {
                    regerror(match_res, &regex, err_msg, sizeof(err_msg));
                    fprintf(stderr, "Regex match failed: %s\n", err_msg);
                }
            }
        }

        args = strtok(NULL, " ");
    }
    write(cd, response, STR_MXL);

    send_response("Search command received", th_id, cd);
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

void share_files(char* args, int th_id, int cd)
{
    args = strtok(NULL, " ");
    if (args == NULL) {
        send_response("No arguments", th_id, cd);
        return;
    }
    int fc = user[th_id].file_cnt;

    while (args != NULL){
        printf("%s*\n", args);

        strcpy(user[th_id].path[fc], args);
        strcpy(user[th_id].file[fc], extr_file_name(args));
        ++fc;

        args = strtok(NULL, " ");
    }
    user[th_id].file_cnt = fc;

    send_response("Share command received", th_id, cd);
}

void download_files(char* args, int th_id, int cd)
{
    args = strtok(NULL, " ");
    int r = -1;
    if (args == NULL) {
        write(cd, &r, 4);
        send_response("No arguments", th_id, cd);
        return;
    }

    for (int i = 0; i < user_cnt; i++)
        if (strcmp(args, user[i].name) == 0) {
            r = i;
            break;
        }

    write(cd, &r, 4);
    if (r < 0) {
        send_response("Invalid user", th_id, cd);
        return;
    }
    write(cd, user[r].ip, STR_MXL);
    write(cd, &user[r].port, 4);
    
    args = strtok(NULL, " ");
    while (args != NULL) {
        int ok = 0;
        for (int i = 0; i < user[r].file_cnt; i++)
            if (strcmp(args, user[r].file[i]) == 0) {
                ok = 1;
                write(cd, &ok, 4);
                write(cd, user[r].path[i], STR_MXL);
                break;
            }
        if (!ok)
            write(cd, &ok, 4);

        args = strtok(NULL, " ");
    }

    send_response("Download command received", th_id, cd);
}

void login_user(char* args, int th_id, int cd)
{
    args = strtok(NULL, " ");
    if (args == NULL) {
        send_response("No arguments", th_id, cd);
        return;
    }
    
    user[th_id].logged = 1;
    strcpy(user[th_id].name, args);

    send_response("Login command received", th_id, cd);
}

void list_users(char* args, int th_id, int cd)
{
    char response[STR_MXL] = "", line[5 * STR_MXL];
    for (int i = 0; i < user_cnt; i++) { 
        if (user[i].logged == 0) continue;
        int fc = user[i].file_cnt;
        sprintf(line, "%s[%s:%d] -- %d files\n", user[i].name, 
                                                user[i].ip,
                                                user[i].port,
                                                fc);
        strcat(response, line);
    }
    write(cd, response, STR_MXL);

    send_response("List users command received", th_id, cd);
}

void list_files(char* args, int th_id, int cd)
{
    char response[STR_MXL] = "", line[5 * STR_MXL];
    for (int i = 0; i < user_cnt; i++) {
        if (user[i].logged == 0) continue;

        int fc = user[i].file_cnt;
        sprintf(line, "%s -- %d files\n", user[i].name, fc);
        strcat(response, line);

        for (int j = 0; j < fc; j++) {
            sprintf(line, "    %s\n", user[i].file[j]);
            strcat(response, line);
        }
        strcat(response, "\n");
    }
    write(cd, response, STR_MXL);

    send_response("List files command received", th_id, cd);
}

