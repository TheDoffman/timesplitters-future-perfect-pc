#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/resource.h>
#include <unistd.h>

int main(void) {
    struct rlimit cpu, files, descriptors, core;
    if(getrlimit(RLIMIT_CPU,&cpu) || getrlimit(RLIMIT_FSIZE,&files) ||
       getrlimit(RLIMIT_NOFILE,&descriptors) || getrlimit(RLIMIT_CORE,&core))return 2;
    printf("{\"alarm\":%u,\"cpu\":%" PRIu64 ",\"file\":%" PRIu64
           ",\"fds\":%" PRIu64 ",\"core\":%" PRIu64 "}\n",
           alarm(0),(uint64_t)cpu.rlim_max,(uint64_t)files.rlim_max,
           (uint64_t)descriptors.rlim_max,(uint64_t)core.rlim_max);
    return 0;
}
