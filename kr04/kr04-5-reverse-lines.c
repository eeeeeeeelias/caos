#include <unistd.h>
#include <sys/types.h>
#include <asm/unistd_32.h>

int my_read(int in_fd, unsigned char *out, int size) {
    int errcode = 0;
    asm volatile(
        "mov %1, %%eax\n"
        "mov %2, %%ebx\n"
        "mov %3, %%ecx\n"
        "mov %4, %%edx\n"
        "int $0x80\n"
        "mov %%eax, %0\n"
        : "=g" (errcode)
        : "g" (__NR_read), "g" (in_fd), "g" (out), "g" (size)
        : "%eax", "%ebx", "%ecx", "%edx", "memory"
    );
    return errcode;
}

int my_write(int out_fd, const unsigned char *out, int size) {
    int errcode = 0;
    asm volatile(
        "mov %1, %%eax\n"
        "mov %2, %%ebx\n"
        "mov %3, %%ecx\n"
        "mov %4, %%edx\n"
        "int $0x80\n"
        "mov %%eax, %0\n"
        : "=g" (errcode)
        : "g" (__NR_write), "g" (out_fd), "g" (out), "g" (size)
        : "%eax", "%ebx", "%ecx", "%edx", "memory"
    );
    return errcode;
}

void my_exit(int exitcode) {
    asm volatile(
        "movl %0, %%eax\n"
        "movl %1, %%ebx\n"
        "int $0x80\n"
        :: "g" (__NR_exit), "g" (exitcode)
        : "%eax", "%ebx", "memory"
    );
}

void *my_brk(void *newbrk) {
    void *result = NULL;
    asm volatile(
        "movl %1, %%eax\n"
        "movl %2, %%ebx\n"
        "int $0x80\n"
        "movl %%eax, %0\n"
        : "=g" (result)
        : "g" (__NR_brk), "g" (newbrk)
        : "%eax", "%ebx", "memory"
    );
    return result;
}

enum { BUF_SIZE = 4096 };

int read_data(int in_fd, unsigned char **pdata) {
    unsigned char *data = (unsigned char *)my_brk(NULL);
    ssize_t num_read;
    unsigned char *new_data = NULL;
    int current_size = 0;
    do {
        new_data = (unsigned char *)my_brk(data + current_size + BUF_SIZE);
        if (new_data) {}
        current_size += BUF_SIZE;
        num_read = my_read(in_fd, data + current_size - BUF_SIZE, BUF_SIZE);
        if (num_read > 0) {
            current_size += num_read - BUF_SIZE;
            new_data = (unsigned char *)my_brk(data + current_size);
        } else {
            if (num_read == 0) {
                current_size -= BUF_SIZE;
                if (*(data + current_size - 1) != '\n') {
                    new_data = (unsigned char *)my_brk(data + current_size + 1);
                    *(data + current_size) = '\n';
                    ++current_size;
                }
            } else {
                // num_read == -1
                // abort();
            }
            break;
        }
    } while (42);
    *pdata = data;
    return current_size;
}

struct Data {
    unsigned char *data;
    int size;
};

void write_reversed_data(int out_fd, struct Data *data) {
    int last_nl = data->size - 1;
    for (int i = data->size - 1; i > 0; --i) {
        if (data->data[i - 1] == '\n') {
            my_write(out_fd, data->data + i, last_nl - i + 1);
            last_nl = i - 1;
        }
    }
    my_write(out_fd, data->data, last_nl + 1);
}

void _start() {
    struct Data data;
    // 0 == stdin
    data.size = read_data(0, &data.data);
    if (data.size > 0) {
        write_reversed_data(1, &data);
    }
    // 1 == stdout
    data.data = (unsigned char *)my_brk(data.data);
    my_exit(0);
}


