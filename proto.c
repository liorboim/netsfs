/* proto.c: netsfs protocol handler
 *
 * Copyright (C) 2012 Beraldo Leal <beraldo@ime.usp.br>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * vim: tabstop=4:softtabstop=4:shiftwidth=4:expandtab
 *
 */

#include <linux/if_ether.h>
#include <linux/module.h>
#include <linux/icmp.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/ipv6.h>
#include <net/ipv6.h>
#include <linux/workqueue.h>
#include <linux/export.h>
#include <linux/kfifo.h>
#include <linux/if_arp.h>

#include "proto.h"
#include "internal.h"

struct netsfs_skb_info {
    struct work_struct my_work;
    struct sk_buff *skb;
};

enum {
    SRC_ADDRESS = 0,
    DST_ADDRESS = 1,
};

/* Return the Ethernet Packet "Type" field in string.
 */
char *get_ether_type(struct sk_buff *skb)
{
    switch (ntohs(eth_hdr(skb)->h_proto))
    {
        case ETH_P_IP:
            return "ipv4";
            break;
        case ETH_P_IPV6:
            return "ipv6";
            break;
        default:
            return "unknow";
            break;
    }
}

int get_ipv4_address(char *ip, struct sk_buff *skb, __u8 type)
{
    struct iphdr *iphdr;
    iphdr = ip_hdr(skb);

    switch (iphdr->protocol)
    {
        case IPPROTO_IP:
        case IPPROTO_ICMP:
        case IPPROTO_TCP:
        case IPPROTO_UDP:
            if (type == SRC_ADDRESS)
                sprintf(ip, "%pI4", &iphdr->saddr);
            else
                sprintf(ip, "%pI4", &iphdr->daddr);
            return 0;
            break;
        default:
            return -EINVAL;
            break;
    }
}

int get_ipv6_address(char *ip, struct sk_buff *skb, __u8 type)
{
    struct ipv6hdr *iphdr;
    iphdr = ipv6_hdr(skb);

    switch (iphdr->nexthdr)
    {
        case NEXTHDR_TCP:
        case NEXTHDR_UDP:
        case NEXTHDR_IPV6:
        case NEXTHDR_ICMP:
            if (type == SRC_ADDRESS)
                sprintf(ip, "%pI6c", &iphdr->saddr);
            else
                sprintf(ip, "%pI6c", &iphdr->daddr);
            return 0;
            return -EINVAL;
            break;
        default:
            return -EINVAL;
            break;
    }
}

int get_ip_address(char *ip, struct sk_buff *skb, __u8 type)
{

    switch (ntohs(eth_hdr(skb)->h_proto)) {
        case ETH_P_IP:
            get_ipv4_address(ip, skb, type);
            return 0;
            break;
            case ETH_P_IPV6:
            get_ipv6_address(ip, skb, type);
            return 0;
            break;
        default:
            return -EINVAL;
    }
}

char *get_ipv4_protocol(struct sk_buff *skb)
{
    switch (ip_hdr(skb)->protocol)
    {
        case IPPROTO_IP:
            return "ip";
            break;
        case IPPROTO_ICMP:
            return "icmp";
            break;
        case IPPROTO_IGMP:
            return "igmp";
            break;
        case IPPROTO_IPIP:
            return "ipip";
            break;
        case IPPROTO_TCP:
            return "tcp";
            break;
        case IPPROTO_UDP:
            return "udp";
            break;
        case IPPROTO_IPV6:
            return "ipv6"; /* ipv6 over ipv4 */
            break;
        case IPPROTO_SCTP:
            return "sctp";
            break;
        case IPPROTO_RAW:
            return "raw";
            break;
        default:
            return "unknow";
            break;
    }
}

char *get_ipv6_protocol(struct sk_buff *skb)
{
    switch (ipv6_hdr(skb)->nexthdr)
    {
        case NEXTHDR_HOP:
            return "hop";
            break;
        case NEXTHDR_TCP:
            return "tcp";
            break;
        case NEXTHDR_UDP:
            return "udp";
            break;
        case NEXTHDR_IPV6:
            return "ipv6";
            break;
        case NEXTHDR_ICMP:
            return "icmp";
            break;
        default:
            return "unknow";
            break;
    }
}

/* Return the IP "protocol" field in string.
 */
char *get_ip_protocol(struct sk_buff *skb)
{
    struct ethhdr *eth;

    eth = eth_hdr(skb);
    switch (ntohs(eth->h_proto)) {
    case ETH_P_IP:
        return get_ipv4_protocol(skb);
    case ETH_P_IPV6:
        return get_ipv6_protocol(skb);
    default:
        break;
    }
    return "unknow1";
}

int get_ipv4_icmp_string(char *str, struct icmphdr *icmph)
{
    sprintf(str, "[ TRANSPORT ] type: %u, code: %u", icmph->type, icmph->code);
    return 0;
}

int get_ipv4_udp_string(char *str, struct udphdr *udph)
{
    sprintf(str, "[ TRANSPORT ] %d -> %d", ntohs(udph->source),
                                           ntohs(udph->dest));
    return 0;
}

int get_ipv4_tcp_string(char *str, struct tcphdr *tcph)
{

    sprintf(str, "[ TRANSPORT ] %d -> %d", ntohs(tcph->source),
                                           ntohs(tcph->dest));

    if (tcph->fin == 1) strcat(str, "[FIN]");
    if (tcph->syn == 1) strcat(str, "[SYN]");
    if (tcph->rst == 1) strcat(str, "[RST]");
    if (tcph->psh == 1) strcat(str, "[PSH]");
    if (tcph->ack == 1) strcat(str, "[ACK]");
    if (tcph->urg == 1) strcat(str, "[URG]");
    if (tcph->ece == 1) strcat(str, "[ECE]");
    if (tcph->cwr == 1) strcat(str, "[CWR]");

    return 0;
}

int get_ipv4_string(char *str, struct sk_buff *skb)
{
    struct tcphdr *tcphdr;
    struct udphdr *udphdr;
    struct icmphdr *icmphdr;

    struct iphdr *iph;

    iph = ip_hdr(skb);

    switch (iph->protocol)
    {
        case IPPROTO_ICMP:
            icmphdr = (struct icmphdr *)((__u32 *)iph + iph->ihl);
            return get_ipv4_icmp_string(str, icmphdr);
            break;
        case IPPROTO_TCP:
            tcphdr = (struct tcphdr *)((__u32 *)iph + iph->ihl);
            return get_ipv4_tcp_string(str, tcphdr);
            break;
        case IPPROTO_UDP:
            udphdr = (struct udphdr *)((__u32 *)iph + iph->ihl);
            return get_ipv4_udp_string(str, udphdr);
            break;
        default:
            sprintf(str, "[ TRANPORT ] unknow");
            break;
    }
    return 0;
}

int get_transport_string(char *str, struct sk_buff *skb)
{

    struct ethhdr *eth;
    eth = eth_hdr(skb);

    switch (ntohs(eth->h_proto)) {
    case ETH_P_IP:
        return get_ipv4_string(str, skb);
    case ETH_P_IPV6:
    default:
        sprintf(str, "[ TRANSPORT ] unknow");
        break;
    }

    return 0;
}

int get_network_string(char *str, struct sk_buff *skb)
{

    struct iphdr *iphdr;
    char *src, *dst;

    iphdr = ip_hdr(skb);

    src = kmalloc(sizeof(char)*35, GFP_KERNEL);
    dst = kmalloc(sizeof(char)*35, GFP_KERNEL);

    get_ip_address(src, skb, SRC_ADDRESS);
    get_ip_address(dst, skb, DST_ADDRESS);

    sprintf(str, "[  NETWORK  ] version: %d, tos: %02x, id: %04x, protocol: %s, %s -> %s",
            iphdr->version,
            iphdr->tos,
            iphdr->id,
            get_ip_protocol(skb),
            src, dst);

    kfree(src);
    kfree(dst);
    return 0;
}


int get_mac_string(char *str, struct sk_buff *skb)
{
    sprintf(str, "[    MAC    ] ts: %llu, dev: %s, len: %d, type: %s",
            skb->tstamp.tv64,
            skb->dev->name,
            skb->len,
            get_ether_type(skb));
    return 0;
}


int get_src_mac(char *source, struct sk_buff *skb)
{
    struct ethhdr *eth;

    switch (skb->dev->type) {
        case ARPHRD_ETHER:
        case ARPHRD_LOOPBACK:
            eth = eth_hdr(skb);
            sprintf(source, "%02x:%02x:%02x:%02x:%02x:%02x",
                    eth->h_source[0],
                    eth->h_source[1],
                    eth->h_source[2],
                    eth->h_source[3],
                    eth->h_source[4],
                    eth->h_source[5]);
            return 0;
            break;
        default:
            printk(KERN_WARNING "device type not supported: %d\n",
                   skb->dev->type);
    }
    return -EINVAL;
}

int get_dst_mac(char *dst, struct sk_buff *skb)
{
    struct ethhdr *eth;

    switch (skb->dev->type) {
        case ARPHRD_ETHER:
        case ARPHRD_LOOPBACK:
            eth = eth_hdr(skb);
            sprintf(dst, "%02x:%02x:%02x:%02x:%02x:%02x",
                    eth->h_dest[0],
                    eth->h_dest[1],
                    eth->h_dest[2],
                    eth->h_dest[3],
                    eth->h_dest[4],
                    eth->h_dest[5]);
            return 0;
            break;
        default:
            printk(KERN_WARNING "device type not supported: %d\n",
                   skb->dev->type);
    }
    return -EINVAL;
}





const char *get_application_protocol(struct sk_buff *skb)
{
    struct tcphdr *tcp_hdr;
    struct iphdr *ip_hdr;

    if (!skb)
        return "unknow";

    /* Get ip header
     */
    ip_hdr = (struct iphdr *)skb_network_header(skb);
    if (!ip_hdr)
        return "unknow";

    /* Get tcp header and checks if is TCP or UDP
     */
    tcp_hdr = (struct tcphdr *)((__u32 *)ip_hdr + ip_hdr->ihl);
    if ((!ip_hdr->protocol==IPPROTO_TCP) &&
        (!ip_hdr->protocol==IPPROTO_UDP))
        return "unknow";

    /* TODO: Remove the hardcoded ports */
    if ((ntohs(tcp_hdr->dest) == 22) ||
        (ntohs(tcp_hdr->source) == 22))
        return "ssh";
    else if ((ntohs(tcp_hdr->dest) == 80) ||
        (ntohs(tcp_hdr->source) == 80))
        return "http";
    else if ((ntohs(tcp_hdr->dest) == 443) ||
        (ntohs(tcp_hdr->source) == 443))
        return "https";
    else if ((ntohs(tcp_hdr->dest) == 3306) ||
        (ntohs(tcp_hdr->source) == 3306))
        return "mysql";
    else {
        printk("netsfs: Unknow app protocol: %d, %d\n", ntohs(tcp_hdr->source), ntohs(tcp_hdr->dest));
        return "unknow";
    }
}

/* Return skb len.
 */
unsigned int get_skb_len(struct sk_buff *skb)
{
    return skb->len;
}


unsigned int netsfs_do(struct dentry *where, struct sk_buff *skb)
{
    struct sk_buff *copy, *temp_skb;
    struct netsfs_dir_private *d_private;
    unsigned int len;
    int ret = 0;

    len = get_skb_len(skb);

    /* Update counters */
    netsfs_inc_inode_size(where->d_inode, len);

    if (where != get_root())
        netsfs_create_files(where);

    /* Make a skb copy to pass to kfifo */
    copy = skb_copy(skb, GFP_KERNEL);
    if (copy == NULL)
        return -ENOMEM;

    spin_lock(&where->d_inode->i_lock);

    /* If kfifo is full, dequeue */
    d_private = where->d_inode->i_private;
    if (cq_is_full(&d_private->queue_skbuff)) {
        temp_skb = cq_get(&d_private->queue_skbuff);
        kfree_skb(temp_skb);
    }
    /* Put skb in kfifo, this skb will be free in read function, or here
     * in this same function when kfifo is full */
    ret = cq_put(&d_private->queue_skbuff, copy);

    spin_unlock(&where->d_inode->i_lock);

    if (ret == 0)
        printk("[netsfs] Probably preempted and queue is full\n");

    return 0;
}


/* Top Halve.
 * Work scheduled earlier is done now, here.
 */
static void netsfs_top(struct work_struct *work)
{
    struct netsfs_skb_info *netsfsinfo;
    struct dentry *network_dentry, *transport_dentry, *app_dentry;
    struct dentry *netsfs_root;

    netsfsinfo = container_of(work, struct netsfs_skb_info, my_work);

    netsfs_root = get_root();
    netsfs_do(netsfs_root, netsfsinfo->skb);

    /* Create L3 dir */
    netsfs_create_dir(get_ether_type(netsfsinfo->skb), netsfs_root, &network_dentry);


    if (network_dentry) {
        /* Create L3 files */
        netsfs_do(network_dentry, netsfsinfo->skb);

        /* Create L4 dir */
        netsfs_create_dir(get_ip_protocol(netsfsinfo->skb), network_dentry, &transport_dentry);
        if (transport_dentry) {
            /* Create L4 files */
            netsfs_do(transport_dentry, netsfsinfo->skb);

            /* TODO: Currently, L5 only for TCP. */
            if ((ip_hdr(netsfsinfo->skb)->protocol) == IPPROTO_TCP) {
                netsfs_create_dir(get_application_protocol(netsfsinfo->skb), transport_dentry, &app_dentry);
                if (app_dentry)
                    netsfs_do(app_dentry, netsfsinfo->skb);
            }
        }
    }

    kfree_skb(netsfsinfo->skb);
    kfree( (void *) work);
}

/* Bottom Halve.
 * Grab the packet in Interrupt Context, we need be fast here.
 * Only filter some packet types and sends to a work queue do the job.
 */
int netsfs_packet_handler(struct sk_buff *skb, struct net_device *dev, struct packet_type *pkt,
                          struct net_device *dev2)
{
    unsigned int len;
    struct netsfs_skb_info *netsfsinfo;

    len = get_skb_len(skb);

    /* IEEE 802.3 Ethernet magic. */
    if (len > ETH_DATA_LEN) {
        dev_kfree_skb(skb);
        return -ENOMEM;
    }

    /* Currently we are only watching ETH_P_IP and ETH_P_IPV6.
     */
    switch (ntohs(eth_hdr(skb)->h_proto))
    {
        case ETH_P_IP:
        case ETH_P_IPV6:
            /* Put a work in a shared workqueue provided by the kernel */
            netsfsinfo = kzalloc(sizeof(struct netsfs_skb_info), GFP_ATOMIC);
            if (!netsfsinfo) {
                dev_kfree_skb(skb);
                return -ENOMEM;
            }

            netsfsinfo->skb = skb;
            INIT_WORK(&netsfsinfo->my_work, netsfs_top);
            schedule_work(&netsfsinfo->my_work);

            break;
        default:
            printk(KERN_INFO "%s: Unknow packet (0x%.4X, 0x%.4X)\n",
                    THIS_MODULE->name,
                    ntohs(pkt->type),
                    ntohs(eth_hdr(skb)->h_proto));
            dev_kfree_skb(skb);
            return -ENOMEM;
    }
    return 0;
}
