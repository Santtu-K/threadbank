#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include "my_log.c"

#define FILENAME_SIZE 1024
#define MAX_LINE 256

FILE *f; // Lokitiedosto

struct num_bal_struct { // Sructi tilinumeron riville tietokannassa ja sen balansille 
    int num, bal;
};

typedef struct num_bal_struct num_bal;

int add_acc(int acc_num) { // Lisää käyttäjän, jos sitä ei löydy tietokannasta
    FILE *pFile;
    pFile = fopen("database.txt", "a");
    if (pFile == NULL) {
        printf("Error opening file.\n");
        log_up("Error opening file.\n");
        return 1;
    }
    int ret = fprintf(pFile, "%d 0\n", acc_num);
    if (ret <= 0) {
        printf("Error adding account: %d\n", acc_num);
        log_up("Error adding account: %d\n", acc_num);
        return 1;
    } else {
        printf("Added account: %d\n", acc_num);
        log_up("Added account: %d\n", acc_num);
    }
    fclose(pFile);
    return 0;
}

num_bal find_acc(int acc_num) { // Etsii tietokannasta tietyn tilinumeron rivin ja sen balanssin
    num_bal s;
    FILE *pFile;
    pFile = fopen("database.txt", "r");
    if (pFile == NULL) {
        printf("Error opening file.\n");
        log_up("Error opening file.\n");
        s.bal = -1;
        s.num = -1;
        return s;
    }
    const char delim[2] = " "; // Tilinumeron ja balanssin erottava merkki
    char line[MAX_LINE];
    int acc_num_f, balance_f;

    bool keep_reading = true;
    int current_line = 1;
    char *temp;

    do {
        temp = fgets(line, MAX_LINE, pFile);
        if (temp != NULL) {
            acc_num_f = atoi(strtok(line, delim));
            if (acc_num_f == acc_num) { // account löytyi
                balance_f = atoi(strtok(NULL, delim));
                keep_reading = false;
                fclose(pFile);
                s.num = current_line; // palauta rivi&balanssi
                s.bal = balance_f;
            }
        } else { // EOF saavutettu, ei löytynyt acc_num
            fclose(pFile);
            assert(add_acc(acc_num) == 0); // Lisää uusi account
            keep_reading = false;
            s.num = current_line; // palauta rivi&balanssi
            s.bal = 0;
        }
        current_line++;
    } while (keep_reading);

    return s;
}

int modify_acc(int line_num, int acc_num, int balance) { // Muokkaa tietokannassa tiettyä tiliä, sen rivinnumeron avulla
    FILE *pFile, *temp_file;
    char *filename = "database.txt";
    char *temp_filename = "tempfile.txt"; // Luo uuden tekstitiedoston, joka lopulta korvaa edellisen

    temp_file = fopen(temp_filename,"w");
    pFile = fopen(filename, "r");
    
    if (temp_file == NULL || pFile == NULL) {
        printf("Error opening file.\n");
        log_up("Error opening file.\n");
        return 1;
    }

    char line[MAX_LINE];   
    bool keep_reading = true;
    int current_line = 1;

    do {
        if (fgets(line, MAX_LINE, pFile) == NULL) keep_reading = false;
        else if (current_line == line_num) {
            fprintf(temp_file, "%d %d\n", acc_num, balance);
        }
        else fputs(line, temp_file);
        current_line++;
    } while (keep_reading);

    fclose(pFile);
    fclose(temp_file);

    remove(filename);
    rename(temp_filename, filename);
    return 0;
}


#define BUFSIZE 255
int give_cmd(char* cmd, int to, size_t cmd_len) { // Toteuttaa annetun käskyn tietokantaan
    char *kasky = malloc(BUFSIZE);
    assert(snprintf(kasky, cmd_len, "%s", cmd)>=0);
    char resp[100];
    int len;
    switch (kasky[0]) {	// First letter determines the command
        case 'q': // quit
            len = sprintf(resp, "quittin\n"); // Client kuittaus
            assert(write(to, resp, len) == len);

            printf("quittin\n"); 
            break;
        case 'l': { // List account value
            int accno = -1;
            num_bal res;
            if (sscanf(kasky,"l %d",&accno) == 1) {
                res = find_acc(accno);
                
                len = sprintf(resp, "ok: %d\n", res.bal); // Client kuittaus
                assert(write(to, resp, len) == len);

                printf("ok: %d\n",res.bal); 
            } else {
                len = sprintf(resp, "fail: Error in command\n"); // Client kuittaus
                assert(write(to, resp, len) == len);
                printf("fail: Error in command\n");
            }
            break;
        }
        case 'w': { // Withdraw from an account
            int accno = -1, amnt = 0, new_bal;
            num_bal res;
            if (sscanf(kasky,"w %d %d",&accno,&amnt) == 2) {
                res = find_acc(accno);
                new_bal = res.bal - amnt;
                if (new_bal < 0) {

                    len = sprintf(resp, "fail: insufficient funds\n"); // Client kuittaus
                    assert(write(to, resp, len) == len);

                    printf("fail: insufficient funds\n");
                } else {
                    modify_acc(res.num, accno, new_bal);
                    
                    len = sprintf(resp, "ok: did it\n"); // Client kuittaus
                    assert(write(to, resp, len) == len);
                    printf("ok: did it\n");
                }
            } else {
                len = sprintf(resp, "fail: Error in command\n"); // Client kuittaus
                assert(write(to, resp, len) == len);
                printf("fail: Error in command\n");
            }
            break;
        }
        case 't': { // Transfer from one account to another
            int from, to_acc, amnt, new_bal_from, new_bal_to;
            num_bal resFrom, resTo;

            if (sscanf(kasky,"t %d %d %d",&from,&to_acc,&amnt) == 3) {
                resFrom = find_acc(from);
                resTo = find_acc(to_acc);
                new_bal_from = resFrom.bal - amnt;
                if (new_bal_from < 0) {
                    len = sprintf(resp, "fail: insufficient funds\n"); // Client kuittaus
                    assert(write(to, resp, len) == len);
                    printf("fail: insufficient funds\n");
                } else {
                    new_bal_to = resTo.bal + amnt;
                    modify_acc(resFrom.num, from, new_bal_from);
                    modify_acc(resTo.num, to_acc, new_bal_to);
                    len = sprintf(resp, "ok: did it\n"); // Client kuittaus
                    assert(write(to, resp, len) == len);
                    printf("ok: did it\n");
                }
            } else {
                len = sprintf(resp, "fail: Error in command\n"); // Client kuittaus
                assert(write(to, resp, len) == len);
                printf("fail: Error in command\n");
            } 
            break;
        }
        case 'd': { // Deposit to an account
            int accno, amnt, new_bal;
            num_bal res;
            if (sscanf(kasky,"d %d %d",&accno,&amnt) == 2) {
                res = find_acc(accno);
                new_bal = res.bal + amnt;
                modify_acc(res.num, accno, new_bal);
                len = sprintf(resp, "ok: did it\n"); // Client kuittaus
                assert(write(to, resp, len) == len);
                printf("ok: did it\n");
            } else {
                len = sprintf(resp, "fail: Error in command\n"); // Client kuittaus
                assert(write(to, resp, len) == len);
                printf("fail: Error in command\n");
            }
            break;
        }
        default: {// Unknown command
            len = sprintf(resp, "fail: Unknown command\n"); // Client kuittaus
            assert(write(to, resp, len) == len);
            printf("fail: Unknown command\n");
            break;
        }
    }
    free(kasky);
    return 0;
}