/**********************************************************************
 * Copyright (c) 2020
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/wait.h>

#include "types.h"
#include "parser.h"

/*====================================================================*/
/*          ****** DO NOT MODIFY ANYTHING FROM THIS LINE ******       */
/**
 * String used as the prompt (see @main()). You may change this to
 * change the prompt */
static char __prompt[MAX_TOKEN_LEN] = "$";

int set_time = 0;
/**
 * Time out value. It's OK to read this value, but ** SHOULD NOT CHANGE
 * IT DIRECTLY **. Instead, use @set_timeout() function below.
 */
static unsigned int __timeout = 2;

static void set_timeout(unsigned int timeout)
{
    __timeout = timeout;

    if (__timeout == 0)
    {
        fprintf(stderr, "Timeout is disabled\n");
    }
    else
    {
        fprintf(stderr, "Timeout is set to %d second%s\n",
                __timeout,
                __timeout >= 2 ? "s" : "");
    }
}
/*          ****** DO NOT MODIFY ANYTHING UP TO THIS LINE ******      */
/*====================================================================*/


/***********************************************************************
 * run_command()
 *
 * DESCRIPTION
 *   Implement the specified shell features here using the parsed
 *   command tokens.
 *
 * RETURN VALUE
 *   Return 1 on successful command execution
 *   Return 0 when user inputs "exit"
 *   Return <0 on error
 */
pid_t pid, child_pid;
char *name;

void sig_handler(int signo)
{
    fprintf(stderr, "%s is timed out\n", name);
    kill(child_pid, SIGKILL);
}

static int run_command(int nr_tokens, char *tokens[])
{
    char PATH[2048];
    long num = 1;

    struct sigaction act;
    act.sa_handler = sig_handler;
    sigaction(SIGALRM, &act, NULL);

    if (strncmp(tokens[0], "for", strlen("for")) == 0)
    {
        int i;
        for (i = 0; i < nr_tokens; i++)
        {
            if (strcmp(tokens[i], "for") == 0)
            {
                num *= strtol(tokens[i + 1], NULL, 10);
                i++;
            }
            else
            {
                break;
            }
        }
        
        int k = i;
        for (int j = 0; j < nr_tokens - k; ++j, ++i)
        {
            tokens[j] = (char *)malloc(sizeof(char *) * strlen(tokens[i]));
            strcpy(tokens[j], tokens[i]);
            tokens[i] = 0;
        }   
        nr_tokens -= k;
    }
    
    for (int r = 0; r < num; r++)
    {
        if (strncmp(tokens[0], "exit", strlen("exit")) == 0)
        {
            return 0;
        }
        else if (strncmp(tokens[0], "prompt", strlen("prompt")) == 0)
        {
            fflush(stdin);
            strcpy(__prompt, tokens[1]);
        }
        else if (strncmp(tokens[0], "cd", strlen("cd")) == 0)
        {
            fflush(stdin);
            if (strcmp(tokens[1], "~") == 0)
            {
                strcpy(PATH, getenv("HOME"));
            }
            else if (strcmp(tokens[1], "..") == 0)
            {
                getcwd(PATH, 2048);
                int length = strlen(PATH);
                for (int i = length - 1; PATH[i] != '/'; i--)
                {
                    PATH[i] = 0;
                }
            }
            else
            {
                getcwd(PATH, 2048);

                strcpy(PATH, tokens[1]);
            }
            chdir(PATH);
        }
        else if (strncmp(tokens[0], "timeout", strlen("timeout")) == 0)
        {
            fflush(stdin);
            if (nr_tokens == 1)
            {
                printf("Currnet timeout is %d second\n", __timeout);
            }
            else
            {
                __timeout = strtol(tokens[1], NULL, 10);
                set_timeout(__timeout);
                set_time = 1;
            }
        }
        else
        {
            fflush(stdin);
            name = tokens[0];
            pid = fork();

            if (pid == -1)
            {
                printf("fork error");
            }
            else if (pid == 0)
            {
                getcwd(PATH, 2048);

                if (strncmp(tokens[0], "./", strlen("./")) == 0)
                {
                    int i, j;
                    for (i = strlen(PATH), j = 1; j < strlen(tokens[0]); j++, i++) //경로 뒤에
                    {
                        PATH[i] = tokens[0][j];
                    }
                    PATH[i] = '\0';

                    if (execv(PATH, tokens) == -1)
                    {
                        fprintf(stderr, "No such file or directory\n");
                        exit(EXIT_FAILURE);
                    }
                    exit(EXIT_SUCCESS);
                }
                else if (strncmp(tokens[0], "/bin/", strlen("/bin/")) == 0)
                {
                    if (execv(tokens[0], tokens) == -1)
                    {
                        fprintf(stderr, "No such file or directory\n");
                        exit(EXIT_FAILURE);
                    }
                    exit(EXIT_SUCCESS);
                }
                else
                {
                    char command[128] = {
                        0,
                    };
                    strcpy(command, "/bin/");
                    int pre_len = strlen("/bin/");

                    for (int i = pre_len, j = 0; i < pre_len + strlen(tokens[0]); ++i, ++j)
                    {
                        command[i] = tokens[0][j];
                    }
                    command[strlen(command)] = 0;

                    if (execv(command, tokens) == -1)
                    {
                        fprintf(stderr, "No such file or directory\n");
                        exit(EXIT_FAILURE);
                    }
                    exit(EXIT_SUCCESS);
                }
            }
            else
            {
                child_pid = pid;
                if (set_time == 1)
                {
                    alarm(__timeout);
                }
                waitpid(pid, NULL, 0);
            }
        }
    }
    return 1;
}

/***********************************************************************
 * initialize()
 *
 * DESCRIPTION
 *   Call-back function for your own initialization code. It is OK to
 *   leave blank if you don't need any initialization.
 *
 * RETURN VALUE
 *   Return 0 on successful initialization.
 *   Return other value on error, which leads the program to exit.
 */
static int initialize(int argc, char *const argv[])
{
    return 0;
}

/***********************************************************************
 * finalize()
 *
 * DESCRIPTION
 *   Callback function for finalizing your code. Like @initialize(),
 *   you may leave this function blank.
 */
static void finalize(int argc, char *const argv[])
{
}

/*====================================================================*/
/*          ****** DO NOT MODIFY ANYTHING BELOW THIS LINE ******      */

static bool __verbose = true;
static char *__color_start = "[0;31;40m";
static char *__color_end = "[0m";

/***********************************************************************
 * main() of this program.
 */
int main(int argc, char *const argv[])
{
    char command[MAX_COMMAND_LEN] = {'\0'};
    int ret = 0;
    int opt;

    while ((opt = getopt(argc, argv, "qm")) != -1)
    {
        switch (opt)
        {
        case 'q':
            __verbose = false;
            break;
        case 'm':
            __color_start = __color_end = "\0";
            break;
        }
    }

    if ((ret = initialize(argc, argv)))
        return EXIT_FAILURE;

    if (__verbose)
        fprintf(stderr, "%s%s%s ", __color_start, __prompt, __color_end);

    while (fgets(command, sizeof(command), stdin))
    {
        char *tokens[MAX_NR_TOKENS] = {NULL};
        int nr_tokens = 0;

        if (parse_command(command, &nr_tokens, tokens) == 0)
            goto more; /* You may use nested if-than-else, however .. */

        ret = run_command(nr_tokens, tokens);
        if (ret == 0)
        {
            break;
        }
        else if (ret < 0)
        {
            fprintf(stderr, "Error in run_command: %d\n", ret);
        }

    more:
        if (__verbose)
            fprintf(stderr, "%s%s%s ", __color_start, __prompt, __color_end);
    }

    finalize(argc, argv);

    return EXIT_SUCCESS;
}