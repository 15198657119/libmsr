#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cpuid.h"
#include "msr_core.h"
#include "msr_rapl.h"
#include "msr_thermal.h"
#include "msr_counters.h"
#include "msr_clocks.h"
#include "profile.h"
#include "msr_misc.h"
#include "msr_turbo.h"
#include "csr_core.h"
#include "csr_imc.h"
#include "libmsr_error.h"

uint64_t pp_policy = 0x5;
struct rapl_limit l1, l2, l3, l4;

void get_limits()
{
    int i;
    static uint64_t sockets = 0;

    if (!sockets)
    {
        core_config(NULL, NULL, &sockets, NULL);
    }
    for (i = 0; i < sockets; i++)
    {
        if (i != 0)
        {
            fprintf(stdout, "\n");
        }
        fprintf(stdout, "Socket %d:\n", i);
        if (get_pkg_rapl_limit(i, &l1, &l2) == 0)
        {
            fprintf(stdout, "Pkg Domain Power Lim 1 (lower lim)\n");
            dump_rapl_limit(&l1, stdout);
            fprintf(stdout, "\n");
            fprintf(stdout, "Pkg Domain Power Lim 2 (upper lim)\n");
            dump_rapl_limit(&l2, stdout);
        }
    }
}

void test_pkg_lower_limit(unsigned s)
{
    l1.watts = 95;
    l1.seconds = 1;
    l1.bits = 0;
    set_pkg_rapl_limit(s, &l1, NULL);
    get_limits();
}

void test_pkg_upper_limit(unsigned s)
{
    l2.watts = 120;
    l2.seconds = 9;
    l2.bits = 0;
    set_pkg_rapl_limit(s, NULL, &l2);
    get_limits();
}

void test_socket_1_limits(unsigned s)
{
    l1.watts = 100;
    l1.seconds = 2;
    l1.bits = 0;
    l2.watts =  180;
    l2.seconds =  3;
    l2.bits = 0;
    set_pkg_rapl_limit(s, &l1, &l2);
    l3.watts = 25;
    l3.seconds = 2;
    l3.bits = 0;
    set_dram_rapl_limit(s, &l3);
    l4.watts = 115;
    l4.seconds = 1;
    l4.bits = 0;
    set_pp_rapl_limit(s, &l4, NULL);
    pp_policy = 8;
    set_pp_rapl_policies(1, &pp_policy, NULL);
    get_limits();
}

void test_socket_0_limits(unsigned s)
{
    l1.watts = 110;
    l1.seconds = 1;
    l1.bits = 0;
    l2.watts =  135;
    l2.seconds =  5;
    l2.bits = 0;
    set_pkg_rapl_limit(s, &l1, &l2);
    l3.watts = 35;
    l3.seconds = 1;
    l3.bits = 0;
    set_dram_rapl_limit(s, &l3);
    l4.watts = 132;
    l4.seconds = 2;
    l4.bits = 0;
    set_pp_rapl_limit(s, &l4, NULL);
    pp_policy = 1;
    set_pp_rapl_policies(0, &pp_policy, NULL);
    get_limits();
}

void set_to_defaults()
{
    int socket = 0;
    int numsockets = num_sockets();
    struct rapl_power_info raplinfo;
    struct rapl_limit socketlim, socketlim2;

    for (socket = 0; socket < numsockets; socket++)
    {
        if (socket != 0)
        {
            fprintf(stdout, "\n");
        }
        fprintf(stdout, "Socket %d:\n", socket);
        get_rapl_power_info(socket, &raplinfo);
        socketlim.bits = 0;
        socketlim.watts = raplinfo.pkg_therm_power;
        socketlim.seconds = 1;
        socketlim2.bits = 0;
        socketlim2.watts = raplinfo.pkg_therm_power * 1.2;
        socketlim2.seconds = 3;
        fprintf(stdout, "Pkg Domain Power Lim 1 (lower lim)\n");
        dump_rapl_limit(&socketlim, stdout);
        fprintf(stdout, "\n");
        fprintf(stdout, "Pkg Domain Power Lim 2 (upper lim)\n");
        dump_rapl_limit(&socketlim2, stdout);
        set_pkg_rapl_limit(socket, &socketlim, &socketlim2);
    }
}

int main(int argc, char **argv)
{
    struct rapl_data *rd = NULL;
    uint64_t *rapl_flags = NULL;
    uint64_t cores = 0;
    uint64_t threads = 0;
    uint64_t sockets = 0;
    int ri_stat = 0;
    unsigned i;

    if (!sockets)
    {
        core_config(&cores, &threads, &sockets, NULL);
    }

    if (init_msr())
    {
        libmsr_error_handler("Unable to initialize libmsr", LIBMSR_ERROR_MSR_INIT, getenv("HOSTNAME"), __FILE__, __LINE__);
        return -1;
    }
    fprintf(stdout, "\n===== MSR Init Done =====\n");

    ri_stat = rapl_init(&rd, &rapl_flags);
    if (ri_stat < 0)
    {
        libmsr_error_handler("Unable to initialize rapl", LIBMSR_ERROR_RAPL_INIT, getenv("HOSTNAME"), __FILE__, __LINE__);
        return -1;
    }
    fprintf(stdout, "\n===== RAPL Init Done =====\n");

    fprintf(stdout, "\n===== Get Initial RAPL Power Limits =====\n");
    get_limits();

    for (i = 0; i < sockets; i++)
    {
        fprintf(stdout, "\n===== Start Socket %u RAPL Power Limit Test =====\n", i);
        fprintf(stdout, "\n--- Testing Pkg Domain Lower Limit ---\n");
        test_pkg_lower_limit(i);
        fprintf(stdout, "\n--- Testing Pkg Domain Upper Limit ---\n");
        test_pkg_upper_limit(i);
        fprintf(stdout, "\n===== End Socket %u RAPL Power Limit Test =====\n", i);
    }

    fprintf(stdout, "\n--- Testing Socket 0 All RAPL Power Limits ---\n");
    test_socket_0_limits(0);

    fprintf(stdout, "\n--- Testing Socket 1 All RAPL Power Limits ---\n");
    test_socket_1_limits(1);

    fprintf(stdout, "\n===== Setting Defaults =====\n");
    set_to_defaults();

    finalize_msr();
    fprintf(stdout, "===== MSR Finalized =====\n");

    fprintf(stdout, "\n===== Test Finished Successfully =====\n");
    if (ri_stat)
    {
        fprintf(stdout, "\nFound %d locked rapl register(s)\n", ri_stat);
    }

    return 0;
}