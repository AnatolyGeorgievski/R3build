/*! \brief Компактный zcip (RFC 3927) — link-local 169.254/16 без BusyBox
 *
 *  Использование:
 *    zcip eth0              # выбрать свободный 169.254.x.y, назначить
 *    zcip eth0 169.254.1.2  # желаемый адрес (если занят — другой)
 *
 *  gcc -DTEST_ZCIP -O2 -o zcip zcip.c
 *  sudo ./zcip eth0
 */
#include <stdint.h>
#include <stdio.h>
// #include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <linux/if_packet.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>
#include <poll.h>

#include "r3_runtime.h"
/*  Key RFCs for Zeroconf IP and NetworkingIPv4 Link-Local Addressing: 
RFC 3927 covers dynamic configuration of IPv4 link-local addresses (often called AutoIP or APIPA).
IPv6 Stateless Address Autoconfiguration (SLAAC): RFC 4862 specifies how IPv6 nodes automatically configure their link-local addresses.
Multicast DNS (mDNS): RFC 6762 handles zero-config hostname resolution on a local link.
Service Discovery: RFC 6763 defines DNS-based service discovery to find network printers or devices without a directory server
*/
/* 169.254.0.0/16, host 1..0xfe for each third octet 1..254 — как у zcip */
#define LL_NET   0xA9FE0000u  /* 169.254.0.0 */
#define LL_MASK  0xFFFF0000u

// рандомизация 64 бит алгоритм MWC64x
extern uint64_t __rand_state;
extern int rand(void);
static void _srand(void)
{
	uint64_t s = __rand_state;
	ssize_t res = getrandom(&s, sizeof(s), GRND_NONBLOCK);// может вернуть ошибку
	if (res<0)
	 	s ^= (uint32_t)time(NULL) ^ (uint32_t)getpid();
	__rand_state = s ? s : ~0x0FEE1BADULL;
	for (int i = 0; i < 4; i++) (void)rand();
}

/* случайный 169.254.x.y, x=1..254, y=1..254 */
static uint32_t pick_ll_ip(void)
{
	uint32_t a = rand() & 0xFFFF;
	if (( a    &0xFF)>253) a+=2;// BCD коррекция
	if (((a>>8)&0xFF)>253) a+=0x200;
	return LL_NET | (uint16_t)(a + 0x0101u); /* host order */
}

/* ---- ARP probe/announce via AF_PACKET ---- */
struct ether_arp {
	uint16_t hrd, pro;
	uint8_t  hln, pln;
	uint16_t op;
	uint8_t  sha[6];
	uint8_t  spa[4];
	uint8_t  tha[6];
	uint8_t  tpa[4];
} __attribute__((packed));

static int open_arp(const char *ifname, int *ifindex, uint8_t mac[6])
{
	int fd = socket(AF_PACKET, SOCK_DGRAM, _htons(ETH_P_ARP));
	if (fd < 0) return -1;

	struct ifreq ifr;
	__builtin_bzero(&ifr, sizeof(ifr));
	snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
	if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) { close(fd); return -1; }
	*ifindex = ifr.ifr_ifindex;
	if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) { close(fd); return -1; }
	__builtin_memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);

	struct sockaddr_ll sll;
	__builtin_bzero(&sll, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_protocol = _htons(ETH_P_ARP);
	sll.sll_ifindex = *ifindex;
	if (bind(fd, (struct sockaddr *)&sll, sizeof(sll)) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

/* op=1 request; spa=0 for probe, spa=ip for announce; tpa=target */
static int arp_send(int fd, int ifindex, const uint8_t mac[6],
                    uint32_t spa_hbo, uint32_t tpa_hbo, int announce)
{
	struct ether_arp pkt;
	__builtin_bzero(&pkt, sizeof(pkt));
	pkt.hrd = _htons(ARPHRD_ETHER);
	pkt.pro = _htons(ETH_P_IP);
	pkt.hln = 6;
	pkt.pln = 4;
	pkt.op  = _htons(1); /* request */
	__builtin_memcpy(pkt.sha, mac, 6);
	uint32_t spa = _htonl(spa_hbo);
	uint32_t tpa = _htonl(tpa_hbo);
	__builtin_memcpy(pkt.spa, &spa, 4);
	__builtin_bzero (pkt.tha, 6);
	__builtin_memcpy(pkt.tpa, &tpa, 4);

	struct sockaddr_ll sll;
	__builtin_bzero(&sll, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_protocol = _htons(ETH_P_ARP);
	sll.sll_ifindex = ifindex;
	sll.sll_halen = 6;
	__builtin_memset(sll.sll_addr, 0xff, 6); /* broadcast */

	return (int)sendto(fd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&sll, sizeof(sll));
}

/* ждать ARP, где кто-то заявляет tpa_hbo (conflict). timeout_ms */
static int arp_conflict(int fd, uint32_t tpa_hbo, int timeout_ms)
{
	uint32_t want = _htonl(tpa_hbo);
	struct pollfd pfd = { .fd = fd, .events = POLLIN };
	int left = timeout_ms;
	while (left > 0) {
		struct timespec t0, t1;
		clock_gettime(CLOCK_MONOTONIC, &t0);
		int pr = poll(&pfd, 1, left);
		clock_gettime(CLOCK_MONOTONIC, &t1);
		int elapsed = (int)((t1.tv_sec - t0.tv_sec) * 1000 +
		                    (t1.tv_nsec - t0.tv_nsec) / 1000000);
		if (elapsed < 1) elapsed = 1;
		left -= elapsed;
		if (pr <= 0) continue;

		uint8_t buf[128];
		ssize_t n = recv(fd, buf, sizeof(buf), 0);
		if (n < (ssize_t)sizeof(struct ether_arp)) continue;
		struct ether_arp *a = (struct ether_arp *)buf;
		/* conflict: spa == our candidate */
		if (__builtin_memcmp(a->spa, &want, 4) == 0)
			return 1;
		/* reply claiming our tpa */
		if (_ntohs(a->op) == 2 && __builtin_memcmp(a->spa, &want, 4) == 0)
			return 1;
	}
	return 0;
}

/* ---- netlink: add /24 address ---- */
static int nl_addr_add(int ifindex, uint32_t ip_hbo, int prefix)
{
	int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (fd < 0) return -1;

	struct {
		struct nlmsghdr nh;
		struct ifaddrmsg ifa;
		char attrbuf[64];
	} req;
	__builtin_bzero(&req, sizeof(req));
	req.nh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
	req.nh.nlmsg_type = RTM_NEWADDR;
	req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL | NLM_F_ACK;
	req.nh.nlmsg_seq = 1;
	req.ifa.ifa_family = AF_INET;
	req.ifa.ifa_prefixlen = (uint8_t)prefix;
	req.ifa.ifa_index = (uint32_t)ifindex;
	req.ifa.ifa_scope = RT_SCOPE_LINK;

	struct rtattr *rta = (struct rtattr *)(((char *)&req) + NLMSG_ALIGN(req.nh.nlmsg_len));
	rta->rta_type = IFA_LOCAL;
	rta->rta_len = RTA_LENGTH(4);
	uint32_t ip_nbo = _htonl(ip_hbo);
	__builtin_memcpy(RTA_DATA(rta), &ip_nbo, 4);
	req.nh.nlmsg_len = NLMSG_ALIGN(req.nh.nlmsg_len) + RTA_ALIGN(rta->rta_len);

	/* IFA_ADDRESS same */
	rta = (struct rtattr *)(((char *)&req) + NLMSG_ALIGN(req.nh.nlmsg_len));
	rta->rta_type = IFA_ADDRESS;
	rta->rta_len = RTA_LENGTH(4);
	__builtin_memcpy(RTA_DATA(rta), &ip_nbo, 4);
	req.nh.nlmsg_len = NLMSG_ALIGN(req.nh.nlmsg_len) + RTA_ALIGN(rta->rta_len);

	struct sockaddr_nl sa;
	__builtin_bzero(&sa, sizeof(sa));
	sa.nl_family = AF_NETLINK;
	if (sendto(fd, &req, req.nh.nlmsg_len, 0, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		close(fd);
		return -1;
	}

	/* optional ACK */
	char abuf[256];
	ssize_t n = recv(fd, abuf, sizeof(abuf), 0);
	close(fd);
	if (n < (ssize_t)sizeof(struct nlmsghdr)) return 0;
	struct nlmsghdr *nh = (struct nlmsghdr *)abuf;
	if (nh->nlmsg_type == NLMSG_ERROR) {
		struct nlmsgerr *e = (struct nlmsgerr *)NLMSG_DATA(nh);
		if (e->error) {
			errno = -e->error;
			return -1;
		}
	}
	return 0;
}

static int if_up(const char *ifname)
{
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) return -1;
	struct ifreq ifr;
	__builtin_bzero(&ifr, sizeof(ifr));
	snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
	if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) { 
		close(fd); return -1; 
	}
	ifr.ifr_flags |= IFF_UP; // проверка выставленного флага
	int res = ioctl(fd, SIOCSIFFLAGS, &ifr);
	close(fd);
	return res;
}

/* Первый 169.254/16 на ifindex → host order, иначе 0 */
/*! \brief netlink find link-local */
static uint32_t nl_find_ll(int ifindex)
{
	int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (fd < 0) return 0;

	struct {
		struct nlmsghdr nh;
		struct ifaddrmsg ifa;
	} req;
	__builtin_bzero(&req, sizeof(req));
	req.nh.nlmsg_len = sizeof(req);
	req.nh.nlmsg_type = RTM_GETADDR;
	req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	req.nh.nlmsg_seq = 1;
	req.ifa.ifa_family = AF_INET;

	struct sockaddr_nl sa;
	__builtin_bzero(&sa, sizeof(sa));
	sa.nl_family = AF_NETLINK;
	if (sendto(fd, &req, sizeof(req), 0, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		close(fd);
		return 0;
	}

	uint32_t found = 0;
	uint8_t buf[8192];
	for (;;) {
		ssize_t n = recv(fd, buf, sizeof(buf), 0);
		if (n < 0) break;
		size_t len = (size_t)n;
		for (struct nlmsghdr *nh = (struct nlmsghdr *)buf;
		     NLMSG_OK(nh, len);
		     nh = NLMSG_NEXT(nh, len)) {
			if (nh->nlmsg_type == NLMSG_DONE)  goto out;
			if (nh->nlmsg_type == NLMSG_ERROR) goto out;
			if (nh->nlmsg_type != RTM_NEWADDR) continue;
			struct ifaddrmsg *ifa = NLMSG_DATA(nh);
			if ((int)ifa->ifa_index != ifindex || ifa->ifa_family != AF_INET)
				continue;
			struct rtattr *rta = IFA_RTA(ifa);
			int rtl = IFA_PAYLOAD(nh);
			for (; RTA_OK(rta, rtl); rta = RTA_NEXT(rta, rtl)) {
				if (rta->rta_type != IFA_LOCAL && rta->rta_type != IFA_ADDRESS)
					continue;
				uint32_t nbo;
				__builtin_memcpy(&nbo, RTA_DATA(rta), 4);
				uint32_t hbo = _ntohl(nbo);
				if ((hbo & LL_MASK) == LL_NET) {
					found = hbo;
					goto out;
				}
			}
		}
	}
out:
	close(fd);
	return found;
}

/*! 
 * \return host-order IP or 0 on failure.
 *  Если адрес из сети 169.254.0.0 уже есть на if — только ARP announce, новый адрес не добавляем.
 */
uint32_t zcip_run(const char *ifname, uint32_t prefer_hbo)
{
	if (if_up(ifname) < 0)
		perror("if_up");

	int ifindex;
	uint8_t mac[6];
	int afd = open_arp(ifname, &ifindex, mac);
	if (afd < 0) {
		perror("open_arp");
		return 0;
	}

	/* Уже есть LL → announce и выход */
	uint32_t existing = nl_find_ll(ifindex);
	if (existing) {
		if (prefer_hbo && (prefer_hbo & LL_MASK) == LL_NET && prefer_hbo != existing) {
			/* prefer другой — не трогаем существующий (без del), только announce */
		}
		for (int a = 0; a < 2; a++) {
			arp_send(afd, ifindex, mac, existing, existing, 1);
			usleep(200000);
		}
		close(afd);
		return existing;
	}

	_srand(); // добавляем энтропии с канала
	uint32_t ip = prefer_hbo;
	if ((ip & LL_MASK) != LL_NET)
		ip = pick_ll_ip();

	for (int attempt = 0; attempt < 10; attempt++) {
		if (attempt) ip = pick_ll_ip();

		int conflict = 0;
		for (int p = 0; p < 3; p++) {
			usleep(100000 + (rand() % 200000));
			arp_send(afd, ifindex, mac, 0, ip, 0);
			if (arp_conflict(afd, ip, 200)) {
				conflict = 1;
				break;
			}
		}
		if (conflict) continue;

		for (int a = 0; a < 2; a++) {
			arp_send(afd, ifindex, mac, ip, ip, 1);
			usleep(200000);
		}

		if (nl_addr_add(ifindex, ip, 16) < 0) {
			if (errno == EEXIST) {
				/* гонка — перечитать */
				uint32_t again = nl_find_ll(ifindex);
				close(afd);
				return again ? again : ip;
			}
			perror("nl_addr_add");
			close(afd);
			return 0;
		}
		close(afd);
		return ip;
	}
	close(afd);
	fprintf(stderr, "zcip: no free address on %s\n", ifname);
	return 0;
}

#ifdef TEST_ZCIP
int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s <ifname> [169.254.x.y]\n", argv[0]);
		return 1;
	}
	uint32_t pref = 0;
	if (argc >= 3) {
		unsigned a, b, c, d;
		if (sscanf(argv[2], "%u.%u.%u.%u", &a, &b, &c, &d) == 4)
			pref = (a << 24) | (b << 16) | (c << 8) | d;
	}
	uint32_t ip = zcip_run(argv[1], pref);
	if (!ip) return 1;
	printf("zcip %s %u.%u.%u.%u/16\n",
	       argv[1],
	       (ip >> 24) & 255, (ip >> 16) & 255,
	       (ip >> 8) & 255, ip & 255);
	return 0;
}
#endif
