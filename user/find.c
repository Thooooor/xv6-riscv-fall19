#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"


/* from grep.c */
int matchhere(char*, char*);
int matchstar(int, char*, char*);

int match(char *re, char *text)
{
  if(re[0] == '^')
    return matchhere(re+1, text);
  do{  // must look at empty string
    if(matchhere(re, text))
      return 1;
  }while(*text++ != '\0');
  return 0;
}

// matchhere: search for re at beginning of text
int matchhere(char *re, char *text)
{
  if(re[0] == '\0')
    return 1;
  if(re[1] == '*')
    return matchstar(re[0], re+2, text);
  if(re[0] == '$' && re[1] == '\0')
    return *text == '\0';
  if(*text!='\0' && (re[0]=='.' || re[0]==*text))
    return matchhere(re+1, text+1);
  return 0;
}

// matchstar: search for c*re at beginning of text
int matchstar(int c, char *re, char *text)
{
  do{  // a * matches zero or more instances
    if(matchhere(re, text))
      return 1;
  }while(*text!='\0' && (*text++==c || c=='.'));
  return 0;
}

/* from ls.c */
char* fmtname(char *path)
{   
    static char buf[DIRSIZ+1];
    char *p;

    // Find first character after last slash.
    for(p=path+strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;

    // Return blank-padded name.
    if(strlen(p) >= DIRSIZ)
        return p;
    memmove(buf, p, strlen(p));
    memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
    return buf;
}

void find(char *path, char *name){
    // from ls.c
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;
    // 打开当前路径的文件
    if((fd = open(path, 0)) < 0){
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    // 将文件转换为结构体
    if(fstat(fd, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }
    // 判断当前文件类型是文件还是文件夹
    switch(st.type){
        case T_FILE:  // 判断名字是否匹配
            if(match(name, fmtname(path)))
            printf("%s\n", path);
            break;
        case T_DIR: {
            if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
                printf("find: path too long\n");
                break;
            }
            strcpy(buf, path);
            p = buf+strlen(buf);
            *p++ = '/';
            // 对文件夹下的文件/文件夹依次进行判断
            while(read(fd, &de, sizeof(de)) == sizeof(de)) {
                if(de.inum == 0)
                    continue;
                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = 0;
                if(stat(buf, &st) < 0){
                    printf("find: cannot stat %s\n", buf);
                    continue;
                }
                // avoid recursing into "." and ".."
                if(strlen(de.name) == 1 && de.name[0] == '.') continue;
                if(strlen(de.name) == 2 && de.name[0] == '.' && de.name[1] == '.') continue;
                // 继续递归
                find(buf, name);
            }
            break;
        }
    }
    close(fd);
}


void main(int argc, char *argv[]) {
    if (argc <= 2) printf("Please provide enough parameters.\n");

    find(argv[1], argv[2]);

    exit();
}