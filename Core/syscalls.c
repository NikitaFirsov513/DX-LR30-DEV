#include <errno.h>
#include <stddef.h>
#include <sys/stat.h>

int _close(int fd) { (void)fd; errno = EBADF; return -1; }
int _fstat(int fd, struct stat *st) { (void)fd; st->st_mode = S_IFCHR; return 0; }
int _isatty(int fd) { return fd >= 0 && fd <= 2; }
int _lseek(int fd, int offset, int whence) {
    (void)fd; (void)offset; (void)whence; errno = ESPIPE; return -1;
}
int _read(int fd, char *data, int size) {
    (void)fd; (void)data; (void)size; errno = ENOSYS; return -1;
}
void *_sbrk(ptrdiff_t increment) {
    extern char _end[];
    static char *current;
    if (!current) current = _end;
    if (increment < 0 || increment > (_end + 0x200) - current) {
        errno = ENOMEM;
        return (void *)-1;
    }
    char *previous = current;
    current += increment;
    return previous;
}
