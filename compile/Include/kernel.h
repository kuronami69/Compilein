#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>

class c_driver {
private:
    pid_t pid;
    int fd;
    char path[256];

    struct dan_uct {
        int read_write;
        pid_t pid;
        uintptr_t addr;
        void* buffer;
        size_t size;
    };

    struct process {
        pid_t process_pid;
        char process_comm[256];  // Fixed: Changed to char array to prevent undefined behavior
    };

    const char* driver_path() {
        DIR* dir = opendir("/proc");
        if (!dir) {
            perror("Unable to open /proc");
            return nullptr;
        }

        struct dirent* entry;
        struct stat statbuf;
        
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;  // Skip non-PID directories

            snprintf(path, sizeof(path), "/proc/%s", entry->d_name);
            if (stat(path, &statbuf) < 0) continue;

            if (S_ISREG(statbuf.st_mode) && statbuf.st_size == 0 &&
                statbuf.st_gid == 0 && statbuf.st_uid == 0) {
                closedir(dir);
                return path;
            }
        }

        closedir(dir);
        return nullptr;
    }

    int open_driver() {
        const char* dev_path1 = driver_path();
        if (dev_path1) {
            fd = open(dev_path1, O_RDWR);
            if (fd > 0) {
                printf("[-] Driver file: %s\n", dev_path1);
                return 1;
            }
        }
        return 0;
    }

public:
    c_driver() {
        if (!open_driver() || fd <= 0) {
            printf("[-] Failed to connect to driver\n");
            exit(EXIT_FAILURE);
        }
    }

    ~c_driver() {
        if (fd > 0) close(fd);
    }

    void initialize(pid_t pid) { this->pid = pid; }

    bool init_key(const char* key) {  // Fixed: `char*` to `const char*`
        char buf[256];
        strncpy(buf, key, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        return ioctl(fd, 0x900, buf) == 0;
    }

    bool read(uintptr_t addr, void* buffer, size_t size) {
        dan_uct dan = {0x999, this->pid, addr, buffer, size};
        return ioctl(fd, 0x999, &dan) == 0;
    }

    bool write(uintptr_t addr, void* buffer, size_t size) {
        dan_uct dan = {0x998, this->pid, addr, buffer, size};
        return ioctl(fd, 0x998, &dan) == 0;
    }

    template <typename T>
    T read(uintptr_t addr) {
        T res{};
        this->read(addr, &res, sizeof(T));
        return res;
    }

    template <typename T>
    bool write(uintptr_t addr, T value) {
        return this->write(addr, &value, sizeof(T));
    }

    uint64_t getModuleBase(const char* module_name) {
        FILE* fp;
        unsigned long addr = 0;
        char line[1024], filename[64];

        snprintf(filename, sizeof(filename), "/proc/%d/maps", pid);
        fp = fopen(filename, "r");
        if (!fp) return 0;

        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, module_name)) {
                addr = strtoul(strtok(line, "-"), nullptr, 16);
                if (addr == 0x8000) addr = 0;
                break;
            }
        }
        fclose(fp);
        return addr;
    }

    void hide_process() { ioctl(fd, 0x996); }

    void hide_pid_process(unsigned int pid) { ioctl(fd, 0x995, &pid); }

    int kernel_getpid(const char* PackageName) {
        process pc;
        strncpy(pc.process_comm, PackageName, sizeof(pc.process_comm) - 1);
        pc.process_comm[sizeof(pc.process_comm) - 1] = '\0';

        if (ioctl(fd, 0x994, &pc) != 0) return 0;
        if (pc.process_pid > 0) this->pid = pc.process_pid;

        return pc.process_pid;
    }
};

static c_driver* driver = new c_driver();

pid_t pid;

float Kernel_v() {
    const char* command = "uname -r | cut -d. -f1,2";
    FILE* file = popen(command, "r");
    if (!file) return 0.0f;

    char result[512] = {0};
    fgets(result, sizeof(result), file);
    pclose(file);

    return atof(result);
}

const char* GetVersion(const char* PackageName) {
    static char result[512];
    char command[256];

    snprintf(command, sizeof(command), "dumpsys package %s | grep versionName | sed 's/=/\\n/g' | tail -n 1", PackageName);
    FILE* file = popen(command, "r");
    if (!file) return nullptr;

    if (!fgets(result, sizeof(result), file)) {
        pclose(file);
        return nullptr;
    }
    pclose(file);
    result[strcspn(result, "\n")] = '\0';  // Remove newline character
    return result;
}

uint64_t GetTime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

const char* getDirectory() {
    static char buf[128];
    int len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len < 0 || len >= (int)sizeof(buf) - 1) return nullptr;

    buf[len] = '\0';
    for (int i = len; i >= 0; i--) {
        if (buf[i] == '/') {
            buf[i] = '\0';
            break;
        }
    }
    return buf;
}

int getPID(const char* PackageName) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "pidof %s", PackageName);

    FILE* fp = popen(cmd, "r");
    if (!fp) return -1;

    if (fscanf(fp, "%d", &pid) != 1) pid = -1;
    pclose(fp);

    if (pid > 0) driver->initialize(pid);
    return pid;
}

bool PidExamine() {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d", pid);
    return access(path, F_OK) == 0;
}

#endif // KERNEL_H
#ifndef KERNEL_H
#define KERNEL_H

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>

class c_driver {
private:
    pid_t pid;
    int fd;
    char path[256];

    struct dan_uct {
        int read_write;
        pid_t pid;
        uintptr_t addr;
        void* buffer;
        size_t size;
    };

    struct process {
        pid_t process_pid;
        char process_comm[256];  // Fixed: Changed to char array to prevent undefined behavior
    };

    const char* driver_path() {
        DIR* dir = opendir("/proc");
        if (!dir) {
            perror("Unable to open /proc");
            return nullptr;
        }

        struct dirent* entry;
        struct stat statbuf;
        
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;  // Skip non-PID directories

            snprintf(path, sizeof(path), "/proc/%s", entry->d_name);
            if (stat(path, &statbuf) < 0) continue;

            if (S_ISREG(statbuf.st_mode) && statbuf.st_size == 0 &&
                statbuf.st_gid == 0 && statbuf.st_uid == 0) {
                closedir(dir);
                return path;
            }
        }

        closedir(dir);
        return nullptr;
    }

    int open_driver() {
        const char* dev_path1 = driver_path();
        if (dev_path1) {
            fd = open(dev_path1, O_RDWR);
            if (fd > 0) {
                printf("[-] Driver file: %s\n", dev_path1);
                return 1;
            }
        }
        return 0;
    }

public:
    c_driver() {
        if (!open_driver() || fd <= 0) {
            printf("[-] Failed to connect to driver\n");
            exit(EXIT_FAILURE);
        }
    }

    ~c_driver() {
        if (fd > 0) close(fd);
    }

    void initialize(pid_t pid) { this->pid = pid; }

    bool init_key(const char* key) {  // Fixed: `char*` to `const char*`
        char buf[256];
        strncpy(buf, key, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        return ioctl(fd, 0x900, buf) == 0;
    }

    bool read(uintptr_t addr, void* buffer, size_t size) {
        dan_uct dan = {0x999, this->pid, addr, buffer, size};
        return ioctl(fd, 0x999, &dan) == 0;
    }

    bool write(uintptr_t addr, void* buffer, size_t size) {
        dan_uct dan = {0x998, this->pid, addr, buffer, size};
        return ioctl(fd, 0x998, &dan) == 0;
    }

    template <typename T>
    T read(uintptr_t addr) {
        T res{};
        this->read(addr, &res, sizeof(T));
        return res;
    }

    template <typename T>
    bool write(uintptr_t addr, T value) {
        return this->write(addr, &value, sizeof(T));
    }

    uint64_t getModuleBase(const char* module_name) {
        FILE* fp;
        unsigned long addr = 0;
        char line[1024], filename[64];

        snprintf(filename, sizeof(filename), "/proc/%d/maps", pid);
        fp = fopen(filename, "r");
        if (!fp) return 0;

        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, module_name)) {
                addr = strtoul(strtok(line, "-"), nullptr, 16);
                if (addr == 0x8000) addr = 0;
                break;
            }
        }
        fclose(fp);
        return addr;
    }

    void hide_process() { ioctl(fd, 0x996); }

    void hide_pid_process(unsigned int pid) { ioctl(fd, 0x995, &pid); }

    int kernel_getpid(const char* PackageName) {
        process pc;
        strncpy(pc.process_comm, PackageName, sizeof(pc.process_comm) - 1);
        pc.process_comm[sizeof(pc.process_comm) - 1] = '\0';

        if (ioctl(fd, 0x994, &pc) != 0) return 0;
        if (pc.process_pid > 0) this->pid = pc.process_pid;

        return pc.process_pid;
    }
};

static c_driver* driver = new c_driver();

pid_t pid;

float Kernel_v() {
    const char* command = "uname -r | cut -d. -f1,2";
    FILE* file = popen(command, "r");
    if (!file) return 0.0f;

    char result[512] = {0};
    fgets(result, sizeof(result), file);
    pclose(file);

    return atof(result);
}

const char* GetVersion(const char* PackageName) {
    static char result[512];
    char command[256];

    snprintf(command, sizeof(command), "dumpsys package %s | grep versionName | sed 's/=/\\n/g' | tail -n 1", PackageName);
    FILE* file = popen(command, "r");
    if (!file) return nullptr;

    if (!fgets(result, sizeof(result), file)) {
        pclose(file);
        return nullptr;
    }
    pclose(file);
    result[strcspn(result, "\n")] = '\0';  // Remove newline character
    return result;
}

uint64_t GetTime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

const char* getDirectory() {
    static char buf[128];
    int len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len < 0 || len >= (int)sizeof(buf) - 1) return nullptr;

    buf[len] = '\0';
    for (int i = len; i >= 0; i--) {
        if (buf[i] == '/') {
            buf[i] = '\0';
            break;
        }
    }
    return buf;
}

int getPID(const char* PackageName) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "pidof %s", PackageName);

    FILE* fp = popen(cmd, "r");
    if (!fp) return -1;

    if (fscanf(fp, "%d", &pid) != 1) pid = -1;
    pclose(fp);

    if (pid > 0) driver->initialize(pid);
    return pid;
}

bool PidExamine() {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d", pid);
    return access(path, F_OK) == 0;
}

#endif // KERNEL_H
