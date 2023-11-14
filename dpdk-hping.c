#include <stdbool.h>
#include <getopt.h>
#include <stdlib.h>

// #include "globals.h"
#include "final.h"

#define APP "pingpong"

uint32_t PINGPONG_LOG_LEVEL = RTE_LOG_DEBUG;
uint64_t duration;
int timer = 0;

/* the client side */
static struct rte_ether_addr target_ether_addr;


/* the server side */
static struct rte_ether_addr my_ether_addr;


#define MAX_PKT_BURST 32
#define MEMPOOL_CACHE_SIZE 128

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 1024
#define RTE_TEST_TX_DESC_DEFAULT 1024
static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

int RTE_LOGTYPE_PINGPONG;

struct rte_mempool *pingpong_pktmbuf_pool = NULL;

/* enabled port */
static uint16_t portid = 0;
/* number of bytes */
static uint64_t nb_bytes_min = 8;
static uint64_t nb_bytes_max = 8192;
/* number of iterations */
static uint64_t total_steps = 100;
/* server mode */
static bool server_mode = false;

static struct rte_eth_conf port_conf = {
    //.rxmode = {
    //    .split_hdr_size = 0,
    //},
    .txmode = {
        .mq_mode = RTE_ETH_MQ_TX_NONE,
    },
};

static const char short_options[] =
    "p:" /* port id */
    "m:" /* maximum size of the message */
    "n:" /* minimum size of the message */
    "i:" /* number of interations */
    "c:" /* client mode, with MAC address */
    "s"  /* server mode */
    "t:"
    "l:" /*number of packets to be sent*/
    "W:" /*timeout interval*/
    "a:" /* server ip address */
    "b:" /* client ip address */
    ;

/* display usage */
static void pingpong_usage(const char *prgname)
{
    printf("%s [EAL options] --"
           "\t-p PORTID: port to configure\n"
           "\t-m BYTES: minimum size of the message\n"
           "\t-n BYTES: maximum size of the message\n"
           "\t-i ITERS: number of iterations\n"
           "\t-c TARGET_MAC: target MAC address\n"
           "\t-s: enable server mode\n",
           //
           "\t-t: time interval for which client pings\n",
           "\t-l: number of packets to be sent\n",
           "\t-W: timeout interval",
           
           //
           prgname);
}

/* Parse the argument given in the command line of the application */
static int pingpong_parse_args(int argc, char **argv)
{
    int opt, ret;
    char *prgname = argv[0];

    while ((opt = getopt(argc, argv, short_options)) != EOF)
    {
        switch (opt)
        {
        /* port id */
        case 'p':
            portid = (uint16_t)strtol(optarg, NULL, 10);
            break;

        case 'm':
            nb_bytes_min = (uint64_t)strtoull(optarg, NULL, 10);
            break;

        case 'n':
            nb_bytes_max = (uint64_t)strtoull(optarg, NULL, 10);
            break;

        case 'i':
            total_steps = (uint64_t)strtoull(optarg, NULL, 10);
            break;

        case 's':
            server_mode = true;
            break;

        case 'c':
        {
            const char *PARSE_STRING = "%02X:%02X:%02X:%02X:%02X:%02X";
            sscanf(optarg, PARSE_STRING,
                   &target_ether_addr.addr_bytes[0],
                   &target_ether_addr.addr_bytes[1],
                   &target_ether_addr.addr_bytes[2],
                   &target_ether_addr.addr_bytes[3],
                   &target_ether_addr.addr_bytes[4],
                   &target_ether_addr.addr_bytes[5]);
            break;
        }

        case 't':
        {
            duration = (uint64_t)strtoull(optarg, NULL, 10);
            timer = 1;
            break;
        }

        case 'l':
        {
            total_packets = (uint16_t)strtol(optarg, NULL, 10);
            is_not_limited = false;
            break;
        }

        case 'W':
        {
            time_out_value = (unsigned int)strtol(optarg, NULL, 10);
            break; 
        }

         case 'a':
        {
            const char *PARSE_STRING = "%d.%d.%d.%d";
            int ip_parts[4];
            sscanf(optarg, PARSE_STRING,
                   &ip_parts[0],
                   &ip_parts[1],
                   &ip_parts[2],
                   &ip_parts[3]);
            client_ip_addr = RTE_IPV4(ip_parts[0], ip_parts[1], ip_parts[2], ip_parts[3]);
            break;
        }

        case 'b':
        {
            const char *PARSE_STRING = "%d.%d.%d.%d";
            int ip_parts[4];
            sscanf(optarg, PARSE_STRING,
                   &ip_parts[0],
                   &ip_parts[1],
                   &ip_parts[2],
                   &ip_parts[3]);
            server_ip_addr = RTE_IPV4(ip_parts[0], ip_parts[1], ip_parts[2], ip_parts[3]);
            break;
        }

        default:
            pingpong_usage(prgname);
            return -1;
        }
    }

    if (optind >= 0)
        argv[optind - 1] = prgname;

    ret = optind - 1;
    optind = 1; /* reset getopt lib */
    return ret;
}

/* construct ping packet */
static struct rte_mbuf *create_packet(unsigned pkt_size)
{
    struct rte_mbuf *pkt;
    struct rte_ether_hdr *eth_hdr;

    pkt = rte_pktmbuf_alloc(pingpong_pktmbuf_pool);
    if (!pkt)
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_PINGPONG, "fail to alloc mbuf for packet\n");


    pkt->next = NULL;

    /* Initialize Ethernet header. */
    eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
    rte_ether_addr_copy(&target_ether_addr, &eth_hdr->dst_addr);
    rte_ether_addr_copy(&my_ether_addr, &eth_hdr->src_addr);
    eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

    l2_len = sizeof(struct rte_ether_hdr);

    uint32_t pkt_len = sizeof(struct rte_ether_hdr); 

    if (!opt_only_eth)
    {
        pkt_len += add_ip(pkt, client_ip_addr, server_ip_addr);
    }
    
    pkt->data_len = pkt_len; // update

    return pkt;
}

/* main ping loop */
static void ping_main_loop(uint64_t nb_bytes)
{
    unsigned nb_rx, nb_tx;
    const uint64_t tsc_hz = rte_get_tsc_hz();
    struct rte_ether_hdr *eth_hdr;
    struct rte_mbuf *pkts_burst[MAX_PKT_BURST];

    unsigned nb_pkts = (nb_bytes + RTE_ETHER_MTU - 1) / RTE_ETHER_MTU;
    struct rte_mbuf **pkts = malloc(nb_pkts * sizeof(struct rte_mbuf *));
    for (int i = 0; i < nb_pkts - 1; ++i)
    {
        pkts[i] = create_packet(RTE_ETHER_MTU);
    }
    pkts[nb_pkts - 1] = create_packet(nb_bytes % RTE_ETHER_MTU);

    ///

    // double max_tsc = 0;
    // double sum_tsc = 0;

    // ///

    // double min_tsc = 999999;

    // if (timer == 1)
    // {
    //     clock_t start_time = clock();
    //     while ((double)(clock() - start_time) / CLOCKS_PER_SEC < duration)
    //     {
    //         double ping_tsc = rte_rdtsc();
    //         /* do ping */
    //         nb_tx = 0;
    //         while (nb_tx < nb_pkts)
    //         {
    //             nb_tx += rte_eth_tx_burst(portid, 0, pkts + nb_tx, nb_pkts - nb_tx);
    //         }

    //         /* wait for pong */
    //         nb_rx = 0;
    //         while (nb_rx < nb_pkts)
    //         {
    //             unsigned nb_rx_once = rte_eth_rx_burst(portid, 0, pkts_burst, MAX_PKT_BURST);
    //             if (nb_rx_once)
    //             {
    //                 if (nb_rx + nb_rx_once > nb_pkts)
    //                     rte_log(RTE_LOG_WARNING, RTE_LOGTYPE_PINGPONG, "%u packets received, %u expected.\n", nb_rx + nb_rx_once, nb_pkts);
    //                 for (int i = 0; i < nb_rx_once; ++i)
    //                 {
    //                     pkts[nb_rx + i] = pkts_burst[i];
    //                     eth_hdr = rte_pktmbuf_mtod(pkts[nb_rx + i], struct rte_ether_hdr *);
    //                     /* compare mac, confirm it is a pong packet */
    //                     // assert(rte_is_same_ether_addr(&eth_hdr->dst_addr, &my_ether_addr));

    //                     rte_ether_addr_copy(&eth_hdr->src_addr, &eth_hdr->dst_addr);
    //                     rte_ether_addr_copy(&my_ether_addr, &eth_hdr->src_addr);
    //                 }
    //                 nb_rx += nb_rx_once;
    //             }
    //         }

    //         double pong_tsc = rte_rdtsc();
    //         double diff_tsc = pong_tsc - ping_tsc;
    //         double rtt_us = diff_tsc * US_PER_S / tsc_hz;
    //         if (rtt_us < min_tsc)
    //             min_tsc = rtt_us;

    //         if (rtt_us > max_tsc)
    //             max_tsc = rtt_us;

    //         sum_tsc += rtt_us;
    //     }
    // }
    // else
    // {
    //     for (int step_idx = 0; step_idx < total_steps; step_idx++)
    //     {

    //         double ping_tsc = rte_rdtsc();
    //         /* do ping */
    //         nb_tx = 0;
    //         while (nb_tx < nb_pkts)
    //         {
    //             nb_tx += rte_eth_tx_burst(portid, 0, pkts + nb_tx, nb_pkts - nb_tx);
    //         }

    //         /* wait for pong */
    //         nb_rx = 0;
    //         while (nb_rx < nb_pkts)
    //         {
    //             unsigned nb_rx_once = rte_eth_rx_burst(portid, 0, pkts_burst, MAX_PKT_BURST);
    //             if (nb_rx_once)
    //             {
    //                 if (nb_rx + nb_rx_once > nb_pkts)
    //                     rte_log(RTE_LOG_WARNING, RTE_LOGTYPE_PINGPONG, "%u packets received, %u expected.\n", nb_rx + nb_rx_once, nb_pkts);
    //                 for (int i = 0; i < nb_rx_once; ++i)
    //                 {
    //                     pkts[nb_rx + i] = pkts_burst[i];
    //                     eth_hdr = rte_pktmbuf_mtod(pkts[nb_rx + i], struct rte_ether_hdr *);
    //                     /* compare mac, confirm it is a pong packet */
    //                     // assert(rte_is_same_ether_addr(&eth_hdr->dst_addr, &my_ether_addr));

    //                     rte_ether_addr_copy(&eth_hdr->src_addr, &eth_hdr->dst_addr);
    //                     rte_ether_addr_copy(&my_ether_addr, &eth_hdr->src_addr);
    //                 }
    //                 nb_rx += nb_rx_once;
    //             }
    //         }

    //         double pong_tsc = rte_rdtsc();
    //         double diff_tsc = pong_tsc - ping_tsc;
    //         double rtt_us = diff_tsc * US_PER_S / tsc_hz;
    //         if (rtt_us < min_tsc)
    //             min_tsc = rtt_us;

    //         if (rtt_us > max_tsc)
    //             max_tsc = rtt_us;

    //         sum_tsc += rtt_us;
    //     }
    // }
    
    Signal(SIGALRM, timeout_handler);
    Signal(SIGINT, print_statistics);
    Signal(SIGTERM, print_statistics);

    for(int num_packets = 0; (num_packets < total_packets) || is_not_limited; num_packets++)
    {
    // while(true)
    // {

        /* do ping */
        nb_tx = 0;
        while (nb_tx < nb_pkts)
        {
            nb_tx += rte_eth_tx_burst(portid, 0, pkts + nb_tx, nb_pkts - nb_tx);
            alarm(time_out_value);
            delaytable_add(seq_nb, 0, time(NULL), get_usec(), S_SENT);
            packets_sent++;
        }

        /* wait for pong */
        nb_rx = 0;
        while (nb_rx < nb_pkts && !is_timed_out)
        {
            unsigned nb_rx_once = rte_eth_rx_burst(portid, 0, pkts_burst, MAX_PKT_BURST);
            if (nb_rx_once)
            {
                // if (nb_rx + nb_rx_once > nb_pkts)
                //     rte_log(RTE_LOG_WARNING, RTE_LOGTYPE_PINGPONG, "%u packets received, %u expected.\n", nb_rx + nb_rx_once, nb_pkts);
                for (int i = 0; i < nb_rx_once; ++i)
                {
                    pkts[nb_rx + i] = pkts_burst[i];
                    eth_hdr = rte_pktmbuf_mtod(pkts[nb_rx + i], struct rte_ether_hdr *);
                    /* compare mac, confirm it is a pong packet */
                    // assert(rte_is_same_ether_addr(&eth_hdr->dst_addr, &my_ether_addr));
                    
                    if(rte_is_same_ether_addr(&eth_hdr->dst_addr, &my_ether_addr)) {
                        rte_ether_addr_copy(&eth_hdr->src_addr, &eth_hdr->dst_addr);
                        rte_ether_addr_copy(&my_ether_addr, &eth_hdr->src_addr);
                        parse_client(pkts[nb_rx + i]);

                        if(correct_packet) {
                            correct_packet = true;
                            nb_rx ++;
                            packets_recv++;
                        }
                    }
                    
                }
            }
        }
        if(is_timed_out){
            printf("Request timed out\n");
            is_timed_out = false;
            update_seq_nb(pkts[0]);
        }
    }

    print_statistics(0);

    for (int i = 0; i < nb_pkts; ++i)
    {
        rte_pktmbuf_free(pkts[i]);
    }
    free(pkts);
}

/* main pong loop */
static void pong_main_loop(uint64_t nb_bytes)
{
    unsigned nb_rx, nb_tx;
    struct rte_mbuf *m = NULL;
    struct rte_ether_hdr *eth_hdr;
    struct rte_mbuf *pkts_burst[MAX_PKT_BURST];

    unsigned nb_pkts = (nb_bytes + RTE_ETHER_MTU - 1) / RTE_ETHER_MTU;
    struct rte_mbuf **pkts = malloc(nb_pkts * sizeof(struct rte_mbuf *));

    // /* wait for pong */
    // for (int step_idx = 0; 1; step_idx++)
    // {
    //     /* wait for ping */
    //     //      rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_PINGPONG, "waiting ping packets\n");
    //     nb_rx = 0;
    //     while (nb_rx < nb_pkts)
    //     {
    //         unsigned nb_rx_once = rte_eth_rx_burst(portid, 0, pkts_burst, MAX_PKT_BURST);
    //         if (nb_rx_once)
    //         {
    //             if (nb_rx + nb_rx_once > nb_pkts)
    //                 rte_log(RTE_LOG_WARNING, RTE_LOGTYPE_PINGPONG, "%u packets received, %u expected.\n", nb_rx + nb_rx_once, nb_pkts);
    //             for (int i = 0; i < nb_rx_once; ++i)
    //             {
    //                 pkts[nb_rx + i] = pkts_burst[i];
    //                 eth_hdr = rte_pktmbuf_mtod(pkts[nb_rx + i], struct rte_ether_hdr *);
    //                 /* compare mac, confirm it is a pong packet */
    //                 // assert(rte_is_same_ether_addr(&eth_hdr->dst_addr, &my_ether_addr));

    //                 rte_ether_addr_copy(&eth_hdr->src_addr, &eth_hdr->dst_addr);
    //                 rte_ether_addr_copy(&my_ether_addr, &eth_hdr->src_addr);

    //                 parser_server(pkts[nb_rx + i]);
    //             }
    //             nb_rx += nb_rx_once;
    //         }
    //     }

    //     /* do pong */
    //     //      rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_PINGPONG, "sending pong packets\n");
    //     nb_tx = 0;
    //     while (nb_tx < nb_pkts)
    //     {
    //         nb_tx += rte_eth_tx_burst(portid, 0, pkts + nb_tx, nb_pkts - nb_tx);
    //     }
    // }

    /* wait for pong */
    while (true)
    {
        /* wait for ping */
        unsigned nb_rx_once = rte_eth_rx_burst(portid, 0, pkts_burst, MAX_PKT_BURST);
        if (nb_rx_once)
        {
            for (int i = 0; i < nb_rx_once; ++i)
            {
                pkts[nb_rx + i] = pkts_burst[i];
                eth_hdr = rte_pktmbuf_mtod(pkts[nb_rx + i], struct rte_ether_hdr *);
                /* compare mac, confirm it is a pong packet */
                // assert(rte_is_same_ether_addr(&eth_hdr->dst_addr, &my_ether_addr));

                if(rte_is_same_ether_addr(&eth_hdr->dst_addr, &my_ether_addr)) {
                    rte_ether_addr_copy(&eth_hdr->src_addr, &eth_hdr->dst_addr);
                    rte_ether_addr_copy(&my_ether_addr, &eth_hdr->src_addr);

                    parser_server(pkts[nb_rx + i]);
                }
            }

            nb_tx = 0;
            while (nb_tx < nb_rx_once)
            {
                nb_tx += rte_eth_tx_burst(portid, 0, pkts + nb_tx, nb_pkts - nb_tx);
            }
        }
    }

    free(pkts);
}

static int ping_launch_one_lcore(__attribute__((unused)) void *dummy)
{
    unsigned lcore_id;
    lcore_id = rte_lcore_id();

    rte_log(RTE_LOG_INFO, RTE_LOGTYPE_PINGPONG,
            "entering ping loop on lcore %u\n", lcore_id);
    rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_PINGPONG,
            "target MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n",
            target_ether_addr.addr_bytes[0],
            target_ether_addr.addr_bytes[1],
            target_ether_addr.addr_bytes[2],
            target_ether_addr.addr_bytes[3],
            target_ether_addr.addr_bytes[4],
            target_ether_addr.addr_bytes[5]);
    // for (uint64_t nb_bytes = nb_bytes_min; nb_bytes <= nb_bytes_max; nb_bytes *= 2)
    //     ping_main_loop(nb_bytes);

    ping_main_loop(8);

    return 0;
}

static int pong_launch_one_lcore(__attribute__((unused)) void *dummy)
{
    unsigned lcore_id;
    lcore_id = rte_lcore_id();

    rte_log(RTE_LOG_INFO, RTE_LOGTYPE_PINGPONG, "entering pong loop on lcore %u\n", lcore_id);
    rte_log(RTE_LOG_INFO, RTE_LOGTYPE_PINGPONG, "waiting ping packets\n");
    // for (uint64_t nb_bytes = nb_bytes_min; nb_bytes <= nb_bytes_max; nb_bytes *= 2)
    //     pong_main_loop(nb_bytes);

    pong_main_loop(8);

    return 0;
}

int main(int argc, char **argv)
{
    int ret;
    uint16_t nb_ports;
    unsigned int nb_mbufs;
    unsigned int lcore_id;

    srand(time(NULL));
    // tsc_hz = rte_get_tsc_hz();

    /* init EAL */
    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
    argc -= ret;
    argv += ret;

    /* init log */
    RTE_LOGTYPE_PINGPONG = rte_log_register(APP);
    ret = rte_log_set_level(RTE_LOGTYPE_PINGPONG, PINGPONG_LOG_LEVEL);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Set log level to %u failed\n", PINGPONG_LOG_LEVEL);

    nb_ports = rte_eth_dev_count_avail();
    if (nb_ports == 0)
        rte_exit(EXIT_FAILURE, "No Ethernet ports, bye...\n");

    rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_PINGPONG, "%u port(s) available\n", nb_ports);

    /* parse application arguments (after the EAL ones) */
    ret = pingpong_parse_args(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Invalid pingpong arguments\n");
    rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_PINGPONG, "Enabled port: %u\n", portid);
    if (portid > nb_ports - 1)
        rte_exit(EXIT_FAILURE, "Invalid port id %u, port id should be in range [0, %u]\n", portid, nb_ports - 1);

    nb_mbufs = RTE_MAX((unsigned int)(nb_ports * (nb_rxd + nb_txd + MAX_PKT_BURST + MEMPOOL_CACHE_SIZE)), 8192U);
    pingpong_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", nb_mbufs,
                                                    MEMPOOL_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
                                                    rte_socket_id());
    if (pingpong_pktmbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");

    struct rte_eth_rxconf rxq_conf;
    struct rte_eth_txconf txq_conf;
    struct rte_eth_conf local_port_conf = port_conf;
    struct rte_eth_dev_info dev_info;

    rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_PINGPONG, "Initializing port %u...\n", portid);
    fflush(stdout);

    /* init port */
    rte_eth_dev_info_get(portid, &dev_info); // DEV_TX_OFFLOAD_MBUF_FAST_FREE
    if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
        local_port_conf.txmode.offloads |=
            // DEV_TX_OFFLOAD_MBUF_FAST_FREE;
            RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

    ret = rte_eth_dev_configure(portid, 1, 1, &local_port_conf);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
                 ret, portid);

    ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd,
                                           &nb_txd);
    if (ret < 0)
        rte_exit(EXIT_FAILURE,
                 "Cannot adjust number of descriptors: err=%d, port=%u\n",
                 ret, portid);

    ret = rte_eth_macaddr_get(portid, &my_ether_addr);
    rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_PINGPONG,
            "my MAC address:     %02X:%02X:%02X:%02X:%02X:%02X\n",
            my_ether_addr.addr_bytes[0],
            my_ether_addr.addr_bytes[1],
            my_ether_addr.addr_bytes[2],
            my_ether_addr.addr_bytes[3],
            my_ether_addr.addr_bytes[4],
            my_ether_addr.addr_bytes[5]);
    if (ret < 0)
        rte_exit(EXIT_FAILURE,
                 "Cannot get MAC address: err=%d, port=%u\n",
                 ret, portid);

    /* init one RX queue */
    fflush(stdout);
    rxq_conf = dev_info.default_rxconf;

    rxq_conf.offloads = local_port_conf.rxmode.offloads;
    ret = rte_eth_rx_queue_setup(portid, 0, nb_rxd,
                                 rte_eth_dev_socket_id(portid),
                                 &rxq_conf,
                                 pingpong_pktmbuf_pool);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
                 ret, portid);

    /* init one TX queue on each port */
    fflush(stdout);
    txq_conf = dev_info.default_txconf;
    txq_conf.offloads = local_port_conf.txmode.offloads;
    ret = rte_eth_tx_queue_setup(portid, 0, nb_txd,
                                 rte_eth_dev_socket_id(portid),
                                 &txq_conf);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
                 ret, portid);

    if (ret < 0)
        rte_exit(EXIT_FAILURE,
                 "Cannot set error callback for tx buffer on port %u\n",
                 portid);

    /* Start device */
    ret = rte_eth_dev_start(portid);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
                 ret, portid);

    rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_PINGPONG, "Initilize port %u done.\n", portid);

    lcore_id = rte_get_next_lcore(0, true, false);

    ret = 0;
    if (server_mode)
    {
        rte_eal_remote_launch(pong_launch_one_lcore, NULL, lcore_id);
    }
    else
    {
        rte_eal_remote_launch(ping_launch_one_lcore, NULL, lcore_id);
    }

    if (rte_eal_wait_lcore(lcore_id) < 0)
    {
        ret = -1;
    }

    rte_eth_dev_stop(portid);
    rte_eth_dev_close(portid);
    rte_log(RTE_LOG_DEBUG, RTE_LOGTYPE_PINGPONG, "Bye.\n");

    return 0;
}
