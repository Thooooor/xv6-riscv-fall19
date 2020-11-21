#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "assert.h"
#include "string.h"
#include "unistd.h"
#include "stdlib.h"

#define MAXARGS 10  // 限制参数最大数量
#define MAXBUF 100

// Parsed command representationss
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>";

struct cmd {
  int type;
};

struct execcmd {
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd {
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct pipecmd mypipecmd;
struct execcmd myexeccmd;
struct redircmd myredircmd;
char mybuf[100];

int getcmd(char *buf, int nbuf);
void runcmd(struct cmd *cmd);

struct cmd *pipecmd(struct cmd *left, struct cmd *right);
struct cmd *execcmd(void);
struct cmd *redircmd(struct cmd *subcmd, char *file, int type);

/*
    parse cmd -> parse pipe -> parse exec -> parse redirs
*/
struct cmd *parsecmd(char *s);
struct cmd *parsepipe(char **ps, char *es);
struct cmd *parseexec();
struct cmd *parseredirs();

char *copy(char *s, char *es);
int scan(char **ps, char *es, const char *tokens, char **q, char **eq);
int peek(char **ps, char *es, char *tokens);
int gettoken(char **ps, char *es, char **q, char **eq);


char* searchfile(char *file, int len, char* name, int mode){
	// PATH
	char* env= getenv("PATH");
	char* end= env + strlen(env);
	char *spath, *epath;
	while( scan(&env, end, ":", &spath, &epath)){
		env++;
		if(spath == epath){
			break;
		}
		memset(file, 0, len);
		if(epath-spath+strlen(name)+2 > len){ // 1 for '/', 1 for '\0'
			fprintf(2, "file abs path too long\n");
		}
		strncat(file, spath, epath-spath);
		strncat(file, "/", 1);
		strncat(file, name, strlen(name));
		if( 0 == access(file, mode) ){
			return file;
		}
	}
	// PWD
	env= getenv("PWD");
	end= env + strlen(env);
	memset(file, 0, len);
	if(end-env+strlen(name)+2 > len){ // 1 for '/', 1 for '\0'
		fprintf(2, "file abs path too long\n");
	}
	strncat(file, env, end-env);
	strncat(file, "/", 1);
	strncat(file, name, strlen(name));
	if( 0 == access(file, mode) ){
		return file;
	}

	return NULL;
}

/*
    get cmd -> cd? -> fork -> parse cmd -> run cmd -> get cmd
                   -> cd
*/
int main(void) {
    static char buf[MAXBUF];
    int fd;

    while ((fd = open("console", O_RDWR)) >= 0){
        if (fd >= 3) {
            close(fd);
            break;
        }
    }

    while (getcmd(buf, sizeof(buf)) >= 0) {
        if (buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' ') {
            buf[strlen(buf) - 1] = 0;
            if (chdir(buf+3) < 0) fprintf(2, "cannot cd %s\n", buf+3);
            continue;
        }
        if(fork() == 0) runcmd(parsecmd(buf));
        wait(0);
    }
    exit(0);
}


int getcmd(char *buf, int nbuf) {
    fprintf(2, "@ ");
    memset(buf, 0, nbuf);
    gets(buf, nbuf);
    if (buf[0] == 0) return -1;
    return 0;
}


void runcmd(struct cmd *cmd) {
    int p[2];
    struct execcmd *ecmd;
    struct pipecmd *pcmd;
    struct redircmd *rcmd;

    if (cmd == 0) exit(0);

    switch(cmd->type) {
        default:
            fprintf(2, "error cmd.\n");
            exit(-1);
        case EXEC:
            fprintf(2, "exec command.\n");
            ecmd = (struct execcmd*)cmd;
            char abscmd[MAXARGS];
            if (NULL == searchfile(abscmd, MAXARGS, ecmd->argv[0], F_OK|R_OK|X_OK)) {
                fprintf(2, "file not exist or permission denied.\n");
                exit(-1);
            }
            fprintf(2, "command :%c.\n", ecmd->argv[2]);
            if (ecmd->argv[0] == 0) exit(-1);
            if (-1 == exec(ecmd->argv[0], ecmd->argv)) fprintf(2, "exec %s failed.\n", ecmd->argv[0]);
            break;
        case REDIR:
            rcmd = (struct redircmd*)cmd;
            close(rcmd->fd);
            if (open(rcmd->file, rcmd->mode) < 0) {
                fprintf(2, "open %s failed.\n", rcmd->file);
                exit(-1);
            }
            runcmd(rcmd->cmd);
            break;
        case PIPE:
            pcmd = (struct pipecmd*)cmd;
            if (pipe(p) < 0) exit(-1);
            if (fork() == 0) {
                close(1);
                dup(p[1]);
                close(p[0]);
                close(p[1]);
                runcmd(pcmd->left);
            }
            if (fork() == 0) {
                close(0);
                dup(p[0]);
                close(p[0]);
                close(p[1]);
                runcmd(pcmd->right);
            }
            close(p[0]);
            close(p[1]);
            wait(0);
            wait(0);
            break;
    }

    exit(0);
}

struct cmd* parsecmd(char *s) {
    struct cmd *cmd;
    char *es;
    es = s + strlen(s);
    cmd = parsepipe(&s, es);
    if (s != es) {
        fprintf(2, "leftovers: %s", s);
        exit(-1);
    }
    return cmd;
}

/*
    left_cmd | right_cmd
    left_cmd -> parse exec -> pipecmd(cmd, )
*/
struct cmd* parsepipe(char **ps, char *es) {
    struct cmd *cmd;
    char *q, *eq;
    if (1 == scan(ps, es, "|", &q, &eq)) {
        cmd = parseexec(&q, eq);
        (*ps)++;
        cmd = pipecmd(cmd, parsepipe(ps, es));
    } else {
        cmd = parseexec(&q, eq);
    }

    return cmd;
}

struct cmd* parseexec(char **ps, char *es){
    char *q, *eq;
    int token, argc;
    struct execcmd *cmd;
    struct cmd *ret;

    ret = execcmd();
    cmd = (struct execcmd*)ret;

    argc = 0;
    ret = parseredirs(ret, ps, es);

    while(*ps < es) {
        if((token=gettoken(ps, es, &q, &eq)) == 0) break;

        if(token != 'a') {
            fprintf(2, "syntax error.\n");
            exit(-1);
        }

        cmd->argv[argc] = copy(q, eq);
        argc++;
        if(argc >= MAXARGS) {
            fprintf(2, "too many args.\n");
            exit(-1);
        }

        ret = parseredirs(ret, ps, es);
    }

    cmd->argv[argc] = 0;

    return ret;
}

struct cmd* parseredirs(struct cmd *cmd, char **ps, char *es) {
    int token;
    char *q, *eq;

    while(peek(ps, es, "<>")) {
        token = gettoken(ps, es, 0, 0);
        if (gettoken(ps, es, &q, &eq) != 'a') {
            fprintf(2, "missing file for redirection.\n");
            exit(-1);
        }

        switch(token) {
            case '<':
                cmd = redircmd(cmd, copy(q, eq), REDIR);
                break;
            case '>':
                cmd = redircmd(cmd, copy(q, eq), REDIR);
                break;
        }
    }
    return cmd;
}

/*
    create pipecmd and convert to cmd
*/
struct cmd *pipecmd(struct cmd *left, struct cmd *right) {
    struct pipecmd *cmd = &mypipecmd;
    cmd->type = PIPE;
    cmd->left = left;
    cmd->right = right;
    return (struct cmd*) cmd;
}

struct cmd *execcmd(void) {
    struct execcmd *cmd = &myexeccmd;

    cmd->type = EXEC;
    
    return (struct cmd*)cmd;
}

struct cmd *redircmd(struct cmd *subcmd, char *file, int type) {
    struct redircmd *cmd = &myredircmd;

    cmd->type = type;
    cmd->cmd = subcmd;
    cmd->file = file;
    cmd->mode = (type == '<') ? O_RDONLY: O_WRONLY|O_CREATE;
    cmd->fd = (type == '<') ? 0: 1;

    return (struct cmd*)cmd;
}

char *copy(char *s, char *es) {
    int n = es-s;
    if (n > MAXBUF) {
        fprintf(2, "too longer.\n");
        exit(-1);
    }
    char *c = mybuf;
    assert(c);
    c[n] = 0;
    return c;
}

/*
    scan string to find token
*/
int scan(char **ps, char *es, const char *tokens, char **q, char **eq) {
    char* s = *ps;

    while (s < es && strchr(whitespace, *s)) s++;
    *q = s;

    while (s < es && !strchr(tokens, *s)) s++;
    *eq = s;
    *ps = s;

    return s==es ? 0:1;
}

int peek(char **ps, char *es, char *tokens) {
    char *s;

    s = *ps;
    while(s < es && strchr(whitespace, *s)) s++;

    *ps = s;

    return *s && strchr(tokens, *s);
}

int gettoken(char **ps, char *es, char **q, char **eq) {
    char *s;
    int ret;

    s = *ps;
    while(s < es && strchr(whitespace, *s)) s++;
    if (q) *q = s;
    ret = *s;
    switch(*s) {
        case 0: break;
        case '|':
        case '<':
            s++;
            break;
        case '>':
            s++;
            break;
        default:
            ret = 'a';
            while (s < es && !strchr(whitespace, *s) && !strchr(symbols, *s)) s++;
            break;
    }
    if (eq) *eq = s;
    while (s < es && strchr(whitespace, *s)) s++;
    *ps = s;
    return ret;
}