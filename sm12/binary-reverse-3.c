#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#pragma pack(push, 1)
struct Data
{
    int16_t x;  // offset = 0/10
    int64_t y;  // offset = 2/10
};
#pragma pack(pop)

void marshall(unsigned char *out, const struct Data *in) {
    *((struct Data *)out) = *in;
}

void unmarshall(struct Data *out, const unsigned char *in) {
    *out = *(struct Data *)in;
}

int is_int32(char *arg, int32_t *dst) {
    char *eptr;
    errno = 0;
    int64_t value = strtol(arg, &eptr, 10);
    if (!*arg || *eptr || errno) {
        return 0;
    }
    if (value == (int32_t)value) {
        *dst = value;
        return 1;
    } else {
        return 0;
    }
}

int64_t MAX_INT64() {
    return ~0ULL >> 1;
}

int64_t MIN_INT64() {
    return ~MAX_INT64();
}

enum { ERROR_MSG_LENGTH = 11 };

int handle_error(ssize_t fd0, ssize_t fd1, const char* errmsg, int errcode) {
    close(fd0);
    close(fd1);
    fprintf(stderr, "%*s\n", ERROR_MSG_LENGTH, errmsg);
    return errcode;
}

int is_sum_int64_overflow(int64_t first, int64_t second) {
    if (first > 0 && second > 0) {
        return first > MAX_INT64() - second;
    } else if (first < 0 && second < 0) {
        return first < MIN_INT64() - second;
    }
    return 0;
}

int get_modified_data(
        int src_fd, int other_fd,
        struct Data *dst,
        unsigned char *buf,
        off_t *cursor_pos,
        int32_t multiplier) {
    ssize_t err_code = read(src_fd, buf, sizeof(struct Data));
    if (err_code != sizeof(struct Data)) {
        return handle_error(src_fd, other_fd, "READ  ERROR", 2);
    }
    *cursor_pos = lseek(src_fd, -sizeof(struct Data), SEEK_CUR);
    if (*cursor_pos == -1) {
        return handle_error(src_fd, other_fd, "LSEEK ERROR", 2);
    }
    unmarshall(dst, buf);
    int64_t addend = dst->x;
    addend *= multiplier;
    if (is_sum_int64_overflow(dst->y, addend)) {
        return handle_error(src_fd, other_fd, "OVERFLOW   ", 3);
    }
    dst->y += addend;
    return 0;
}

int write_data(
        int dst_fd, int other_fd,
        unsigned char *buf,
        const struct Data *src) {
    marshall(buf, src);
    ssize_t err_code = write(dst_fd, buf, sizeof(struct Data));
    if (err_code != sizeof(struct Data)) {
        return handle_error(dst_fd, other_fd, "WRITE ERROR", 2);
    }
    return 0;
}

int is_file_empty(int fd) {
    off_t pos = lseek(fd, 0, SEEK_END);
    if (pos == 0) {
        return 1;
    }
    pos = lseek(fd, 0, SEEK_SET);
    return 0;
}

// argv[1] == binary file name
// argv[2] == int32_t (multiplier)
int main(int argc, char** argv) {
    unsigned char buf[sizeof(struct Data)];
    int32_t multiplier;
    if (argc < 3) {
        abort();
    }
    if (!is_int32(argv[2], &multiplier)) {
        abort();
    }
    struct Data left_data, right_data;  // No more!
    ssize_t left_fd = open(argv[1], O_RDWR);
    ssize_t right_fd = open(argv[1], O_RDWR);
    if (left_fd == -1) {
        return handle_error(left_fd, right_fd, "OPEN  LEFT ", 2);
    }
    if (right_fd == -1) {
        return handle_error(left_fd, right_fd, "OPEN  RIGHT", 2);
    }
    if (is_file_empty(left_fd)) {
        return handle_error(left_fd, right_fd, "EMPTY FILE ", 0);
    }
    off_t left_pos = 0, right_pos = lseek(right_fd, -sizeof(struct Data), SEEK_END);
    ssize_t err_code;
    while (left_pos < right_pos) {
        // Read left one.
        err_code = get_modified_data(left_fd, right_fd, &left_data, buf, &left_pos, multiplier);
        if (err_code) return err_code;
        // Read right one.
        err_code = get_modified_data(right_fd, left_fd, &right_data, buf, &right_pos, multiplier);
        if (err_code) return err_code;
        // Write right one to left.
        err_code = write_data(left_fd, right_fd, buf, &right_data);
        if (err_code) return err_code;
        // Write left one to right.
        err_code = write_data(right_fd, left_fd, buf, &left_data);
        if (err_code) return err_code;
        // Get left cursor position.
        left_pos = lseek(left_fd, 0, SEEK_CUR);
        if (left_pos == -1) return handle_error(left_fd, right_fd, "LSEEK LEFT ", 2);
        // Move right cursor back.
        right_pos = lseek(right_fd, -2 * sizeof(struct Data), SEEK_CUR);
        if (right_pos == -1) return handle_error(left_fd, right_fd, "LSEEK RIGHT", 2);
    }
    if (left_pos == right_pos) {
        // Odd number of items. Do one change.
        err_code = get_modified_data(left_fd, right_fd, &left_data, buf, &left_pos, multiplier);
        if (err_code) return err_code;

        err_code = write_data(left_fd, right_fd, buf, &left_data);
        if (err_code) return err_code;
    }
    // else: entire file is reversed, all items are changed.
    close(left_fd);
    close(right_fd);
    return 0;
}

