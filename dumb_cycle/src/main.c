typedef short i16;
typedef unsigned short u16;
typedef int i32;
typedef unsigned int u32;
typedef long i64;
typedef unsigned long u64;

u64 _syscall(
    u64 scid,
    u64 arg0,
    u64 arg1,
    u64 arg2,
    u64 arg3,
    u64 arg4,
    u64 arg5
);

enum sys {
    SYS_WRITE = 1,
    SYS_EXIT = 60,
};

enum std {
    STD_IN = 0,
    STD_OUT = 1,
    STD_ERR = 2,
};

i64 write(int fd, const char *buf, i64 len) {
    return _syscall(SYS_WRITE, fd, (u64)buf, len, 0, 0, 0);
}

void exit(int error_code) {
    _syscall(SYS_EXIT, error_code, 0, 0, 0, 0, 0);
}

i64 strlen(const char *str) {
    i64 len = 0;
    for (const char *c = str; *c != 0; c++) {
        len++;
    }
    return len;
}

int main(int argc, char **argv) {
    char msg[] = "Hello from ";
    write(STD_OUT, msg, sizeof(msg) - 1);
    write(STD_OUT, argv[0], strlen(argv[0]));
    write(STD_OUT, "\n", 1);
    return 0;
}

void _start_c(int argc, char **argv) {
    exit(main(argc, argv));
}
