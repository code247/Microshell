/******************************************************************************
 *
 *  File Name........: main.c
 *
 *  Description......: A Linux style Micro-shell that supports I/O redirection, piping and some built-in functions.
 *
 *  Author...........: Sagar Manohar
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "parse.h"

extern char **environ;

static bool is_built_in(char *c){
	if (strcmp(c, "cd") == 0) return true;
	else if (strcmp(c, "echo") == 0) return true;
	else if (strcmp(c, "logout") == 0) return true;
	else if (strcmp(c, "nice") == 0) return true;
	else if (strcmp(c, "pwd") == 0) return true;
	else if (strcmp(c, "setenv") == 0) return true;
	else if (strcmp(c, "unsetenv") == 0) return true;
	else if (strcmp(c, "where") == 0) return true;
	else return false;
}

static void built_in_handler(Cmd c){
	if (strcmp(c->args[0], "cd") == 0){
		if(c->nargs == 1){
			if (chdir(getpwuid(getuid())->pw_dir) < 0){
				fprintf(stderr, "%d\n", errno);
			}
		}
		else {
			if (chdir(c->args[1]) < 0){
				fprintf(stderr, "%d\n", errno);
			}
		}
	}
	else if (strcmp(c->args[0], "echo") == 0){
		int i;
		for( i = 1; i < c->nargs; i++){
			if (i == c->nargs - 1) printf("%s", c->args[i]);
			else printf("%s ", c->args[i]);
		}
		printf("\n");
	}
	else if (strcmp(c->args[0], "logout") == 0){
		exit(0);
	}
	else if (strcmp(c->args[0], "nice") == 0){

	}
	else if (strcmp(c->args[0], "pwd") == 0){
		char *buf = (char *) malloc (1000);
		if (getcwd (buf, 1000) == buf) printf("%s\n", buf);
		free(buf);
	}
	else if (strcmp(c->args[0], "setenv") == 0){
		if (c->nargs == 1){
			int i;
			for(i = 0; environ[i] != NULL; i++){
				printf("%s\n", environ[i]);
			}
		} else {
			if (c->nargs == 2){
				setenv(c->args[1], "", 1);
			} else {
				setenv(c->args[1], c->args[2], 1);
			}
		}
	}
	else if (strcmp(c->args[0], "unsetenv") == 0){
		if (c->nargs > 1){
			unsetenv(c->args[1]);
		}
	}
	else if (strcmp(c->args[0], "where") == 0){
		if (c->nargs > 1){
			if (is_built_in(c->args[1])){
				printf("%s: Shell built-in command\n", c->args[1]);
				return;
			}
			char path[4096];
			bool b = false;
			strcpy(path, getenv("PATH"));
			char *token = strtok(path, ":");
			while(token){
				char t[128];
				strcpy(t, token);
				strcat(t, "/");
				strcat(t, c->args[1]);
				if (access(t, F_OK) == 0) {
					printf("%s;", t);
					b = true;
				}
				token = strtok(NULL, ":");
			}
			if (!b) fprintf(stderr, "%s: Command not found", c->args[1]);
			printf("\n");
		}
	}
	return;
}

static int exePipeCmd(Cmd c, int inp) {
	int pipefd[2];
	pipe(pipefd);
	pid_t pid;
	if ( c ) {
		pid = fork();
		if (pid == 0){
			signal(SIGINT, SIG_DFL);
			dup2(inp, 0);
			dup2(pipefd[1], 1);
			if (c->out == TpipeErr){
				dup2(pipefd[1], 2);
			}
			if (is_built_in(c->args[0])){
				built_in_handler(c);
				exit(EXIT_SUCCESS);
			} else {
				if (execvp(c->args[0], c->args) < 0) printf("Exec failed!\n");
			}
		} else {
			wait(NULL);
		}
		close(inp);
		close(pipefd[1]);
		return pipefd[0];
	} else {
		return -1;
	}
}

static void exeLastCmd(Cmd c, int inp){
	int fd;
	pid_t pid;
	if (is_built_in(c->args[0])){
		int out, err;
		switch (c->out){
		case Tout:
			out = dup(1);
			fd = open(c->outfile, O_WRONLY | O_CREAT, 0777);
			dup2(fd, 1);
			close(fd);
			built_in_handler(c);
			dup2(out, 1);
			close(out);
			break;
		case ToutErr:
			out = dup(1);
			err = dup(2);
			fd = open(c->outfile, O_WRONLY | O_CREAT, 0777);
			dup2(fd, 1);
			dup2(fd, 2);
			close(fd);
			built_in_handler(c);
			dup2(out, 1);
			dup2(err, 2);
			close(out);
			close(err);
			break;
		case Tapp:
			out = dup(1);
			fd = open(c->outfile, O_WRONLY | O_CREAT | O_APPEND, 0777);
			dup2(fd, 1);
			close(fd);
			built_in_handler(c);
			dup2(out, 1);
			close(out);
			break;
		case TappErr:
			out = dup(1);
			err = dup(2);
			fd = open(c->outfile, O_WRONLY | O_CREAT | O_APPEND, 0777);
			dup2(fd, 1);
			dup2(fd, 2);
			close(fd);
			built_in_handler(c);
			dup2(out, 1);
			dup2(err, 2);
			close(out);
			close(err);
			break;
		default:
			break;
		}
	}
	else {
		pid = fork();
		if (pid == 0){
			signal(SIGINT, SIG_DFL);
			dup2(inp, 0);
			if (c->out != Tnil)
				switch (c->out){
					case Tout:
						fd = open(c->outfile, O_WRONLY | O_CREAT, 0777);
						dup2(fd, 1);
						close(fd);
						break;
					case ToutErr:
						fd = open(c->outfile, O_WRONLY | O_CREAT, 0777);
						dup2(fd, 1);
						dup2(fd, 2);
						close(fd);
						break;
					case Tapp:
						fd = open(c->outfile, O_WRONLY | O_CREAT | O_APPEND, 0777);
						dup2(fd, 1);
						close(fd);
						break;
					case TappErr:
						fd = open(c->outfile, O_WRONLY | O_CREAT | O_APPEND, 0777);
						dup2(fd, 1);
						dup2(fd, 2);
						close(fd);
						break;
					default:
						break;
				}
			if (execvp(c->args[0], c->args) < 0) printf("Exec failed!\n");
		} else {
			wait(NULL);
		}
		close(inp);
	}
}

static int exeFirstCmd(Cmd c){
	int out_fd, out = 0, err = 0;
	if ( c ) {
		if(is_built_in(c->args[0]) && c->next == NULL){
			if ( c->out != Tnil )
			  switch ( c->out ) {
				case Tout:
					out = dup(1);
					out_fd = open(c->outfile, O_WRONLY | O_CREAT, 0777);
					dup2(out_fd, 1);
					close(out_fd);
					break;
				case ToutErr:
					out = dup(1);
					err = dup(2);
					out_fd = open(c->outfile, O_WRONLY | O_CREAT, 0777);
					dup2(out_fd, 1);
					dup2(out_fd, 2);
					close(out_fd);
					break;
				case Tapp:
					out = dup(1);
					out_fd = open(c->outfile, O_WRONLY | O_CREAT | O_APPEND, 0777);
					dup2(out_fd, 1);
					close(out_fd);
					break;
				case TappErr:
					out = dup(1);
					err = dup(2);
					out_fd = open(c->outfile, O_WRONLY | O_CREAT | O_APPEND, 0777);
					dup2(out_fd, 1);
					dup2(out_fd, 2);
					close(out_fd);
					break;
				default:
			  		fprintf(stderr, "Shouldn't get here\n");
			  		exit(-1);
			}
			built_in_handler(c);
			if (out != 0){
				dup2(out, 1);
				close(out);
			}
			if (err != 0){
				dup2(err, 2);
				close(err);
			}
			return 0;
		} else {
			if (c->out == Tpipe || c->out == TpipeErr){
				int pipefd[2];
				pipe(pipefd);
				pid_t pid;
				pid = fork();
				int inp_fd;
				if (pid == 0){
					signal(SIGINT, SIG_DFL);
					if (c->in == Tin) {
						inp_fd = open(c->infile, O_RDONLY, 0777);
						dup2(inp_fd, 0);
						close(inp_fd);
					}
					dup2(pipefd[1], 1);
					if (c->out == TpipeErr) dup2(pipefd[1], 2);
					if (!is_built_in(c->args[0])){
						if (execvp(c->args[0], c->args) < 0) printf ("Exec failed");
					} else {
						built_in_handler(c);
						exit(EXIT_SUCCESS);
					}
				} else {
					wait(NULL);
				}
				close(pipefd[1]);
				return pipefd[0];
			} else {
				pid_t pid;
				pid = fork();
				int inp_fd, out_fd;
				if (pid == 0){
					signal(SIGINT, SIG_DFL);
					if (c->in == Tin) {
						inp_fd = open(c->infile, O_RDONLY, 0777);
						dup2(inp_fd, 0);
						close(inp_fd);
					}
					if ( c->out != Tnil )
					  switch ( c->out ) {
						case Tout:
							out_fd = open(c->outfile, O_WRONLY | O_CREAT, 0777);
							dup2(out_fd, 1);
							close(out_fd);
							break;
						case ToutErr:
							out_fd = open(c->outfile, O_WRONLY | O_CREAT, 0777);
							dup2(out_fd, 1);
							dup2(out_fd, 2);
							close(out_fd);
							break;
						case Tapp:
							out_fd = open(c->outfile, O_WRONLY | O_CREAT | O_APPEND, 0777);
							dup2(out_fd, 1);
							close(out_fd);
							break;
						case TappErr:
							out_fd = open(c->outfile, O_WRONLY | O_CREAT | O_APPEND, 0777);
							dup2(out_fd, 1);
							dup2(out_fd, 2);
							close(out_fd);
							break;
						default:
						  fprintf(stderr, "Shouldn't get here\n");
						  exit(-1);
					  }
					if (execvp(c->args[0], c->args) < 0) printf("Exec failed!");
				} else {
					wait(NULL);
				}
				return 0;
			}
		}
	} else {
		return -1;
	}
}

static int checkCmd(char *c){
	if (strchr(c, '/') != NULL){
		struct stat buf;
		if (c[0] == '/'){
			if (stat(c, &buf) == -1) {
				fprintf(stderr, "ush: %s: No such file or directory\n", c);
				return -1;
			} else {
				if ((buf.st_mode & S_IFMT) == S_IFDIR) {
					fprintf(stderr, "ush: %s: Is a directory\n", c);
					return -1;
				} else if ((buf.st_mode & S_IFMT) == S_IFREG){
					if (access(c, X_OK) == -1){
						fprintf(stderr, "ush: %s: Permission denied\n", c);
						return -1;
					} else return 0;
				}
			}
		} else {
			char *real = realpath(c, NULL);
			if (real) {
				if (stat(real, &buf) == -1) {
					fprintf(stderr, "ush: %s: No such file or directory\n", c);
					return -1;
				} else {
					if ((buf.st_mode & S_IFMT) == S_IFDIR) {
						fprintf(stderr, "ush: %s: Is a directory\n", c);
						return -1;
					} else if ((buf.st_mode & S_IFMT) == S_IFREG){
						if (access(c, X_OK) == -1){
							fprintf(stderr, "ush: %s: Permission denied\n", c);
							return -1;
						} else return 0;
					}
				}
			} else {
				fprintf(stderr,"ush: %s: No such file or directory\n", c);
				return -1;
			}
		}
	} else {
		char path[4096];
		strcpy(path, getenv("PATH"));
		char *token = strtok(path, ":");
		while(token){
			char t[128];
			strcpy(t, token);
			strcat(t, "/");
			strcat(t, c);
			if (access(t, F_OK) == 0) return 0;
			token = strtok(NULL, ":");
		}
		fprintf(stderr, "%s: command not found\n", c);
		return -1;
	}
	return -1;
}

static void exePipe(Pipe p){
	Cmd c = NULL;
	if ( p == NULL ) return;
	int inp;
	for (c = p->head; c != NULL; c = c->next ) {
		if ( !strcmp(c->args[0], "end") ) break;
		if (!is_built_in(c->args[0])) {
			if (checkCmd(c->args[0]) != 0)
				break;
		}
		if (c == p->head) inp = exeFirstCmd(c); //First command
		else if (c->in == Tpipe && c->next != NULL) inp = exePipeCmd(c, inp); //Middle command
		else exeLastCmd(c, inp); //Final command
	}
	exePipe(p->next);
}

int countLines(FILE *fp){
	int ch, number_of_lines = 0;

	do
	{
	    ch = fgetc(fp);
	    if(ch == '\n')
	    	number_of_lines++;
	} while (ch != EOF);

	return number_of_lines;
}

int main(int argc, char *argv[]) {
	Pipe p;
	char host[256];
	gethostname(host, 256);
	int in = dup(0);
	int out = dup(1);
	int err = dup(2);
	int lines;
	char* homeDir = getpwuid(getuid())->pw_dir;
	strcat(homeDir, "/.ushrc");
	FILE *fp = fopen(homeDir, "r");
	if (fp != NULL) lines = countLines(fp);
	int ushFd = open(homeDir, O_RDONLY);
	if (ushFd != -1){
		dup2(ushFd, STDIN_FILENO);
		close(ushFd);
		while(lines > 0){
			printf("%s%%\n", host);
			p = parse();
			exePipe(p);
			freePipe(p);
			lines--;
		}
	}

	while ( 1 ) {
		dup2(in, 0);
		dup2(out, 1);
     	dup2(err, 2);
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		signal(SIGTERM, SIG_IGN);
		if (isatty(STDIN_FILENO)){
			printf("%s%% ", host);
			fflush(stdout);
			fflush(stderr);
		}
		p = parse();
		if (isatty(STDIN_FILENO) == 0 && !strcmp(p->head->args[0], "end")) {
			break;
		}
		exePipe(p);
		freePipe(p);
	}
}

/*........................ end of main.c ....................................*/
