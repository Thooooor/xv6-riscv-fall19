#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define MAXARGS 10  // 限制参数最大数量
#define MAXBUF 100

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
char mybuf[100];


int getcmd(char *buf, int nbuf) {
    fprintf(2, "@ ");
    memset(buf, 0, nbuf);
    gets(buf, nbuf);
    if (buf[0] == 0) return -1;
    return 0;
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

void runcmd(struct cmd *cmd) {
    exit(0);
}

/*
    create pipecmd and convert to cmd
*/
struct cmd *pipecmd(struct cmd *left, struct cmd *right) {
    struct pipecmd *cmd = &mypipecmd;
    cmd->type = "|";
    cmd->left = left;
    cmd->right = right;
    return (struct cmd*) cmd;
}

/*
    parse cmd -> parse pipe -> parse exec -> parse redirs
*/
struct cmd *parsecmd(char *s);
struct cmd *parsepipe(char **ps, char *es);
struct cmd *parseexec();
struct cmd *parseredirs();

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
                cmd = redircmd(cmd, copy(q, eq), '<');
                break;
            case '>':
                cmd = redircmd(cmd, copy(q, eq), '>');
                break;
        }
    }
    return cmd;
}