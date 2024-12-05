#include "exec/simbricks_mem.h"
#include "qemu/qemu-print.h"

static SimbricksMemState* memstate = NULL;

int uninit_simbricks_mem_if(void) {
    SimbricksBaseIfClose(&memstate->memif.base);
    g_free(memstate);
    memstate = NULL;
    return 0;
}


int init_new_simbricks_mem_if(void) {
    assert(memstate == NULL);
    memstate = g_malloc0(sizeof(*memstate));
    struct SimbricksBaseIf* base_if = &memstate->memif.base;
    struct SimbricksBaseIfParams memParams;

    SimbricksMemIfDefaultParams(&memParams);

    QemuOpts* simbricksMemCliOptions = qemu_find_opts_singleton("simbricks_mem");
    const char* socket_path = qemu_opt_get(simbricksMemCliOptions, "socket");
    uint64_t link_latency = strtoull(qemu_opt_get(simbricksMemCliOptions, "link_latency"), NULL, 0);
    bool sync = strtol(qemu_opt_get(simbricksMemCliOptions, "sync"), NULL, 0);

    assert(socket_path);

    if (link_latency == 0){ //Sid TODO: set default cli args in a more QEMU-esque way
        link_latency = 500000;
    }

    memParams.link_latency = link_latency;
    memParams.sync_mode = (sync ? kSimbricksBaseIfSyncRequired :
        kSimbricksBaseIfSyncDisabled);
    memParams.sock_path = socket_path;
    memParams.blocking_conn = true;
    

    if (SimbricksBaseIfInit(base_if, &memParams)) {
        perror("SimbricksBaseIfInit failed\n");
        return 1;
    }

    if (SimbricksBaseIfConnect(base_if)) {
        perror("SimbricksBaseIfConnect failed\n");
        return 1;
    }

    if (SimbricksBaseIfConnected(base_if)) {
        perror("SimbricksBaseIfConnected indicates unconnected\n");
        return 1;
    }

    struct SimBricksBaseIfEstablishData ests[1];
    struct SimbricksProtoMemMemIntro m_intro;
    struct SimbricksProtoMemHostIntro h_intro;
    unsigned n_bifs = 0;

    memset(&m_intro, 0, sizeof(m_intro));
    ests[n_bifs].base_if = base_if;
    ests[n_bifs].tx_intro = &h_intro;
    ests[n_bifs].tx_intro_len = sizeof(h_intro);
    ests[n_bifs].rx_intro = &m_intro;
    ests[n_bifs].rx_intro_len = sizeof(m_intro);
    n_bifs++;   

    if (SimBricksBaseIfEstablish(ests, 1)) {
        perror("SimBricksBaseIfEstablish failed\n");
        return 1;
    }

    qemu_printf("Successfully initialized Simbricks socket\n");

    uninit_simbricks_mem_if(); // placeholder for testing to avoid memory leaks
    // remove when implementing read and write

    // Sid TODO: handle errors properly instead of just perror()'ing

    return 0;
}