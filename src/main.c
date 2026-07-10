#include "meridian/runtime.h"

#include <stdio.h>

int main(int argc, char **argv) {
    const char *scenario = "routed";
    MdStatus status;

    if (argc > 1) {
        scenario = argv[1];
    }
    status = md_runtime_run(scenario);
    if (status != MD_OK) {
        fprintf(stderr, "meridiandtl: %s\n", md_status_name(status));
        return (int)status;
    }
    return 0;
}
