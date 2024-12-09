#include "exec/simbricks_mem.h"
#include "qemu/qemu-print.h"

static SimbricksMemState* memstate = NULL;


int uninit_simbricks_mem_if(void) {
    SimbricksBaseIfClose(&memstate->memif.base);
    g_free(memstate);
    memstate = NULL;
    return 0;
}

int init_new_simbricks_mem_if(FarOffSocket * far_off_socket) {
    // TODO : remove anything that realisse to params here, everything should come from the machine
    assert(memstate == NULL);
    memstate = g_malloc0(sizeof(*memstate));
    struct SimbricksBaseIf* base_if = &memstate->memif.base;
    struct SimbricksBaseIfParams memParams;

    SimbricksMemIfDefaultParams(&memParams);

    bool sync = far_off_socket->sync;
    uint64_t link_latency = far_off_socket->link_latency;
    const char *socket_path = far_off_socket->socket_path;

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
    // Sid TODO: handle errors properly instead of just perror()'ing

    return 0;
}

static void simbricks_mem_poll_response(uint64_t *value, unsigned size) {
    struct SimbricksMemIf *mem_if = &memstate->memif;
    uint8_t type; 
    uint64_t cur_ts = 0;

    volatile union SimbricksProtoMemM2H *m2h_msg;
    
    while (!(m2h_msg = SimbricksMemIfM2HInPoll(mem_if, cur_ts)));
    type = SimbricksMemIfM2HInType(mem_if, m2h_msg);

    switch (type){

        case SIMBRICKS_PROTO_MEM_M2H_MSG_READCOMP:
            volatile struct SimbricksProtoMemM2HReadcomp *readcomp = &m2h_msg->readcomp;
            memcpy(value, readcomp->data, size);
            break;
        
        case SIMBRICKS_PROTO_MEM_M2H_MSG_WRITECOMP:
            break;

        default:
            perror("Invalid/Unimplemented Message Type");
            break;
    }
    SimbricksMemIfM2HInDone(mem_if, m2h_msg);

    return;
}

int simbricks_mem_read(uint64_t addr, uint64_t *value, unsigned size) {
    struct SimbricksMemIf *mem_if = &memstate->memif;
    uint64_t cur_ts = 0; // Sid TODO: implement proper timestamping
    volatile union SimbricksProtoMemH2M *h2m_msg = SimbricksMemIfH2MOutAlloc(mem_if, cur_ts); // Sid TODO: error handling
    volatile struct SimbricksProtoMemH2MRead *read = &h2m_msg->read;
    read->req_id = 0;
    read->addr = addr;
    read->len = size;
    read->as_id = 0;

    SimbricksMemIfH2MOutSend(mem_if, h2m_msg, SIMBRICKS_PROTO_MEM_H2M_MSG_READ);

    simbricks_mem_poll_response(value, size);

    return 0;
}

int simbricks_mem_write(uint64_t addr, uint64_t *value, unsigned size) {
    struct SimbricksMemIf *mem_if = &memstate->memif;
    uint64_t cur_ts = 0; // Sid TODO: implement proper timestamping
    volatile union SimbricksProtoMemH2M *h2m_msg = SimbricksMemIfH2MOutAlloc(mem_if, cur_ts); // Sid TODO: error handling
    volatile struct SimbricksProtoMemH2MWrite *write = &h2m_msg->write;
    write->req_id = 0;
    write->addr = addr;
    write->len = size;
    write->as_id = 0;
    memcpy(write->data, value, size);

    SimbricksMemIfH2MOutSend(mem_if, h2m_msg, SIMBRICKS_PROTO_MEM_H2M_MSG_WRITE);

    simbricks_mem_poll_response(NULL, 0);

    return 0;
}