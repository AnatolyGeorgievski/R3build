#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <poll.h>
#include "zcip.h"

#ifndef INIT_PARSE_TEST
#include "r3_module.h"
#endif

#ifndef MS_MOVE
#define MS_MOVE 8192
#endif

static void say(const char *s)
{
	char buf[256];
	int fd, n;
	n = snprintf(buf, sizeof buf, "%s\n", s);
	if (n < 0) return;
	if (n >= (int)sizeof buf) n = (int)sizeof buf - 1;
	fd = open("/dev/kmsg", O_WRONLY | O_NOCTTY);
	if (fd >= 0) {
		write(fd, buf, (size_t)n);
		close(fd);
	}
	write(2, buf, (size_t)n);
}

/* root=UUID=… or root=/dev/…  from a cmdline buffer. */
int init_root_spec(const char *cmd, char *uuid, int nu, char *spec, int ns)
{
	const char *p;
	if (!cmd || !uuid || !spec || nu < 2 || ns < 2) return -1;
	uuid[0] = spec[0] = 0;
	for (p = cmd; *p; ) {
		const char *e;
		while (*p == ' ' || *p == '\t' || *p == '\n') p++;
		if (!*p) break;
		e = p;
		while (*e && *e != ' ' && *e != '\t' && *e != '\n') e++;
		if (e - p > 5 && strncmp(p, "root=", 5) == 0) {
			const char *v = p + 5;
			int n = (int)(e - v);
			if (n > 5 && strncmp(v, "UUID=", 5) == 0) {
				if (n - 5 >= nu) return -1;
				memcpy(uuid, v + 5, (size_t)(n - 5));
				uuid[n - 5] = 0;
			} else {
				if (n >= ns) return -1;
				memcpy(spec, v, (size_t)n);
				spec[n] = 0;
			}
			return 0;
		}
		p = e;
	}
	return -1;
}

#ifndef INIT_PARSE_TEST

static int ensure(const char *path, mode_t mode)
{
	if (mkdir(path, mode) == 0 || errno == EEXIST) return 0;
	say("init: mkdir failed");
	return -1;
}

static int mnt(const char *src, const char *tgt, const char *type, unsigned long fl, const void *data)
{
	if (mount(src, tgt, type, fl, data) == 0) return 0;
	if (errno == EBUSY) return 0;
	fprintf(stderr, "init: mount %s on %s: %s\n", src, tgt, strerror(errno));
	return -1;
}

static int early(void)
{
	if (ensure("/proc", 0755) || ensure("/sys", 0755) || ensure("/dev", 0755)
	    || ensure("/run", 0755) || ensure("/root", 0755))
		return -1;
	if (mnt("proc", "/proc", "proc", MS_NOSUID|MS_NODEV|MS_NOEXEC, 0)) return -1;
	if (mnt("sysfs", "/sys", "sysfs", MS_NOSUID|MS_NODEV|MS_NOEXEC, 0)) return -1;
	if (mnt("udev", "/dev", "devtmpfs", MS_NOSUID, "mode=0755")) return -1;
	ensure("/dev/pts", 0755);
	if (mnt("devpts", "/dev/pts", "devpts", MS_NOSUID|MS_NOEXEC, "mode=0600")) return -1;
	if (mnt("tmpfs", "/run", "tmpfs", MS_NOSUID|MS_NODEV|MS_NOEXEC, "mode=0755")) return -1;
	return 0;
}

static int read_cmd(char *buf, int n)
{
	int fd = open("/proc/cmdline", O_RDONLY);
	int r;
	if (fd < 0) return -1;
	r = (int)read(fd, buf, (size_t)(n - 1));
	close(fd);
	if (r < 0) return -1;
	buf[r] = 0;
	return 0;
}

static int exec1(const char *path, char *const av[])
{
	pid_t pid;
	int st;
	pid = fork();
	if (pid < 0) return -1;
	if (pid == 0) {
		execv(path, av);
		_exit(127);
	}
	if (waitpid(pid, &st, 0) < 0) return -1;
	return (WIFEXITED(st) && WEXITSTATUS(st) == 0) ? 0 : -1;
}

static const char *modprobe_bin(void)
{
	static const char *bins[] = { "/sbin/modprobe", "/usr/sbin/modprobe", "/bin/modprobe", 0 };
	int i;
	for (i = 0; bins[i]; i++)
		if (access(bins[i], X_OK) == 0) return bins[i];
	return 0;
}

/* nvme на этом ядре модуль. Без оболочки. */
static void load_nvme(void)
{
	const char *bin = modprobe_bin();
	char *core[] = { "modprobe", "nvme_core", 0 };
	char *nv[] = { "modprobe", "nvme", 0 };
	if (!bin) {
		say("init: modprobe missing");
		return;
	}
	exec1(bin, core);
	exec1(bin, nv);
}

/* blkid is its own binary in the initrd. No shell. */
static int blkid_dev(const char *uuid, char *dev, int nd)
{
	int pfd[2], st;
	pid_t pid;
	char arg[80];
	ssize_t r;
	if (snprintf(arg, sizeof arg, "UUID=%s", uuid) >= (int)sizeof arg) return -1;
	if (pipe(pfd) != 0) return -1;
	pid = fork();
	if (pid < 0) return -1;
	if (pid == 0) {
		char *av[] = { "blkid", "-t", arg, "-o", "device", 0 };
		dup2(pfd[1], 1);
		close(pfd[0]);
		close(pfd[1]);
		execv("/sbin/blkid", av);
		execv("/bin/blkid", av);
		_exit(127);
	}
	close(pfd[1]);
	r = read(pfd[0], dev, (size_t)(nd - 1));
	close(pfd[0]);
	waitpid(pid, &st, 0);
	if (r <= 0 || !WIFEXITED(st) || WEXITSTATUS(st) != 0) return -1;
	dev[r] = 0;
	if (dev[r - 1] == '\n') dev[r - 1] = 0;
	return dev[0] ? 0 : -1;
}

/* by-uuid is a relative link (../../nvme0n1p3). mount needs an absolute path. */
static int abs_link(const char *link, char *dev, int nd)
{
	char raw[192], out[192];
	ssize_t n;
	const char *slash, *p;
	n = readlink(link, raw, sizeof raw - 1);
	if (n < 0) {
		snprintf(dev, (size_t)nd, "%s", link);
		return dev[0] ? 0 : -1;
	}
	raw[n] = 0;
	if (raw[0] == '/') {
		if (n >= nd) return -1;
		memcpy(dev, raw, (size_t)n + 1);
		return 0;
	}
	slash = strrchr(link, '/');
	if (!slash) return -1;
	if ((int)(slash - link) >= (int)sizeof out) return -1;
	memcpy(out, link, (size_t)(slash - link));
	out[slash - link] = 0;
	for (p = raw; *p; ) {
		const char *e = p;
		int len, L;
		while (*e && *e != '/') e++;
		len = (int)(e - p);
		if (len == 1 && p[0] == '.') {
		} else if (len == 2 && p[0] == '.' && p[1] == '.') {
			char *s = strrchr(out, '/');
			if (s && s != out) *s = 0;
		} else if (len > 0) {
			L = (int)strlen(out);
			if (L + 1 + len >= (int)sizeof out) return -1;
			if (!(L == 1 && out[0] == '/')) out[L++] = '/';
			memcpy(out + L, p, (size_t)len);
			out[L + len] = 0;
		}
		p = *e ? e + 1 : e;
	}
	if ((int)strlen(out) >= nd) return -1;
	memcpy(dev, out, strlen(out) + 1);
	return 0;
}

static int find_dev(const char *uuid, char *dev, int nd)
{
	char path[128];
	int i;
	load_nvme();
	snprintf(path, sizeof path, "/dev/disk/by-uuid/%s", uuid);
	for (i = 0; i < 50; i++) {
		if (access(path, F_OK) == 0) {
			if (abs_link(path, dev, nd) == 0) return 0;
		}
		if (blkid_dev(uuid, dev, nd) == 0) return 0;
		usleep(100000);
	}
	return -1;
}

static int move_mnt(const char *from, const char *to)
{
	char line[256];
	if (mount(from, to, 0, MS_MOVE, 0) == 0) return 0;
	snprintf(line, sizeof line, "init: move %s %s: %s", from, to, strerror(errno));
	say(line);
	return -1;
}

/* Слово switch в cmdline или аргумент. Ядро аргумент в /init не передаёт. */
static int cmd_has(const char *cmd, const char *word)
{
	const char *p;
	int n;
	if (!cmd || !word) return 0;
	n = (int)strlen(word);
	for (p = cmd; *p; ) {
		const char *e;
		while (*p == ' ' || *p == '\t' || *p == '\n') p++;
		if (!*p) break;
		e = p;
		while (*e && *e != ' ' && *e != '\t' && *e != '\n') e++;
		if ((int)(e - p) == n && strncmp(p, word, (size_t)n) == 0) return 1;
		p = e;
	}
	return 0;
}


/* Один дамп связей. Пишет init: link <имя> running|up|down. Возвращает 1, если есть не lo. */
static int dump_links(void)
{
	int fd, got;
	char buf[4096], line[128], name[32];
	struct {
		struct nlmsghdr nh;
		struct ifinfomsg ifi;
	} req;
	fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
	if (fd < 0) {
		snprintf(line, sizeof line, "init: link socket: %s", strerror(errno));
		say(line);
		return 0;
	}
	memset(&req, 0, sizeof req);
	req.nh.nlmsg_len = sizeof req;
	req.nh.nlmsg_type = RTM_GETLINK;
	req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	req.nh.nlmsg_seq = 1;
	req.ifi.ifi_family = AF_UNSPEC;
	if (send(fd, &req, sizeof req, 0) < 0) {
		snprintf(line, sizeof line, "init: link send: %s", strerror(errno));
		say(line);
		close(fd);
		return 0;
	}
	got = 0;
	for (;;) {
		struct nlmsghdr *nh;
		int len, rest;
		struct timeval tv;
		fd_set rfds;
		tv.tv_sec = 0;
		tv.tv_usec = 200000;
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		if (select(fd + 1, &rfds, 0, 0, &tv) <= 0) break;
		len = (int)recv(fd, buf, sizeof buf, 0);
		if (len <= 0) break;
		rest = len;
		for (nh = (struct nlmsghdr *)buf; NLMSG_OK(nh, rest); nh = NLMSG_NEXT(nh, rest)) {
			struct ifinfomsg *ifi;
			struct rtattr *rta;
			int alen;
			const char *st;
			if (nh->nlmsg_type == NLMSG_DONE) { close(fd); return got; }
			if (nh->nlmsg_type != RTM_NEWLINK) continue;
			ifi = NLMSG_DATA(nh);
			alen = nh->nlmsg_len - NLMSG_LENGTH(sizeof *ifi);
			name[0] = 0;
			for (rta = IFLA_RTA(ifi); RTA_OK(rta, alen); rta = RTA_NEXT(rta, alen)) {
				if (rta->rta_type != IFLA_IFNAME) continue;
				snprintf(name, sizeof name, "%s", (char *)RTA_DATA(rta));
			}
			if (!name[0]) continue;
			if (ifi->ifi_flags & IFF_RUNNING) st = "running";
			else if (ifi->ifi_flags & IFF_UP) st = "up";
			else st = "down";
			snprintf(line, sizeof line, "init: link %s %s", name, st);
			say(line);
			if (strcmp(name, "lo") != 0) got = 1;
		}
	}
	close(fd);
	return got;
}

/* Имя первого не-lo iface: eth, en, usb. */
static int pick_iface(char *out, int n)
{
	int fd;
	char buf[4096], name[32];
	struct {
		struct nlmsghdr nh;
		struct ifinfomsg ifi;
	} req;
	fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
	if (fd < 0) return -1;
	memset(&req, 0, sizeof req);
	req.nh.nlmsg_len = sizeof req;
	req.nh.nlmsg_type = RTM_GETLINK;
	req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	req.nh.nlmsg_seq = 1;
	req.ifi.ifi_family = AF_UNSPEC;
	if (send(fd, &req, sizeof req, 0) < 0) { close(fd); return -1; }
	for (;;) {
		struct nlmsghdr *nh;
		int len, rest;
		struct timeval tv;
		fd_set rfds;
		tv.tv_sec = 0;
		tv.tv_usec = 200000;
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		if (select(fd + 1, &rfds, 0, 0, &tv) <= 0) break;
		len = (int)recv(fd, buf, sizeof buf, 0);
		if (len <= 0) break;
		rest = len;
		for (nh = (struct nlmsghdr *)buf; NLMSG_OK(nh, rest); nh = NLMSG_NEXT(nh, rest)) {
			struct ifinfomsg *ifi;
			struct rtattr *rta;
			int alen;
			if (nh->nlmsg_type == NLMSG_DONE) { close(fd); return -1; }
			if (nh->nlmsg_type != RTM_NEWLINK) continue;
			ifi = NLMSG_DATA(nh);
			alen = nh->nlmsg_len - NLMSG_LENGTH(sizeof *ifi);
			name[0] = 0;
			for (rta = IFLA_RTA(ifi); RTA_OK(rta, alen); rta = RTA_NEXT(rta, alen)) {
				if (rta->rta_type != IFLA_IFNAME) continue;
				snprintf(name, sizeof name, "%s", (char *)RTA_DATA(rta));
			}
			if (!name[0] || strcmp(name, "lo") == 0) continue;
			/* USB-LAN / бортовой eth; wlan пока не трогаем без fw+модулей */
			if (strncmp(name, "eth", 3) == 0 || strncmp(name, "en", 2) == 0
			    || strncmp(name, "usb", 3) == 0) {
				snprintf(out, n, "%s", name);
				close(fd);
				return 0;
			}
		}
	}
	close(fd);
	return -1;
}

static void see_links(void)
{
	int i;
	char ifc[32], line[64];
	/* USB eth часто позже 3с — ждём до ~20с */
	for (i = 0; i < 80; i++) {
		dump_links();
		if (pick_iface(ifc, sizeof ifc) == 0) {
			snprintf(line, sizeof line, "init: pick %s", ifc);
			say(line);
			return;
		}
		usleep(250000);
	}
	say("init: link none");
}

static unsigned short icmp_cksum(const void *data, int len)
{
	const unsigned short *p = data;
	unsigned long sum = 0;
	while (len > 1) { sum += *p++; len -= 2; }
	if (len) sum += *(const unsigned char *)p;
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return (unsigned short)~sum;
}

/* Один ICMP echo; ответ "ok ms=N" / "fail". */
static void mcp_ping(int c, const char *target)
{
	int s, n;
	struct sockaddr_in dst, from;
	socklen_t fl = sizeof from;
	char pkt[64], rbuf[128], out[96];
	struct icmphdr *ih;
	struct timeval tv0, tv1;
	long ms;
	memset(&dst, 0, sizeof dst);
	dst.sin_family = AF_INET;
	if (inet_pton(AF_INET, target, &dst.sin_addr) != 1) {
		write(c, "fail bad-ip\n", 12);
		return;
	}
	s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (s < 0) {
		write(c, "fail raw\n", 9);
		return;
	}
	memset(pkt, 0, sizeof pkt);
	ih = (struct icmphdr *)pkt;
	ih->type = ICMP_ECHO;
	ih->un.echo.id = htons(0x4e54);
	ih->un.echo.sequence = htons(1);
	ih->checksum = icmp_cksum(pkt, 8);
	gettimeofday(&tv0, 0);
	if (sendto(s, pkt, 8, 0, (struct sockaddr *)&dst, sizeof dst) < 0) {
		close(s);
		write(c, "fail send\n", 10);
		return;
	}
	{
		struct pollfd p = { .fd = s, .events = POLLIN };
		if (poll(&p, 1, 1500) <= 0) {
			close(s);
			write(c, "fail timeout\n", 13);
			return;
		}
	}
	n = (int)recvfrom(s, rbuf, sizeof rbuf, 0, (struct sockaddr *)&from, &fl);
	close(s);
	gettimeofday(&tv1, 0);
	if (n < (int)(sizeof(struct iphdr) + 8)) {
		write(c, "fail short\n", 11);
		return;
	}
	ms = (tv1.tv_sec - tv0.tv_sec) * 1000L + (tv1.tv_usec - tv0.tv_usec) / 1000L;
	n = snprintf(out, sizeof out, "ok ms=%ld\n", ms);
	write(c, out, (size_t)n);
}

/* Тонкий mDNS A-анонс на 224.0.0.251:5353 (имя board.local). */
static void mdns_announce(uint32_t ip_hbo, const char *ifname)
{
	int fd, one = 1;
	struct sockaddr_in a, dst;
	unsigned char pdu[128];
	int n = 0;
	const char *host = "r3board";
	char line[96];
	struct ip_mreq m;
	(void)ifname;
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) return;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
	memset(&a, 0, sizeof a);
	a.sin_family = AF_INET;
	a.sin_port = htons(5353);
	a.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(fd, (struct sockaddr *)&a, sizeof a) != 0) { close(fd); return; }
	memset(&m, 0, sizeof m);
	m.imr_multiaddr.s_addr = inet_addr("224.0.0.251");
	m.imr_interface.s_addr = htonl(ip_hbo);
	setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &m, sizeof m);
	setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &m.imr_interface, sizeof m.imr_interface);
	/* header */
	pdu[n++] = 0; pdu[n++] = 0; /* id */
	pdu[n++] = 0x84; pdu[n++] = 0; /* response, authoritative */
	pdu[n++] = 0; pdu[n++] = 0; /* qd */
	pdu[n++] = 0; pdu[n++] = 1; /* an=1 */
	pdu[n++] = 0; pdu[n++] = 0; pdu[n++] = 0; pdu[n++] = 0;
	pdu[n++] = (unsigned char)strlen(host);
	memcpy(pdu + n, host, strlen(host)); n += (int)strlen(host);
	pdu[n++] = 5; memcpy(pdu + n, "local", 5); n += 5;
	pdu[n++] = 0;
	pdu[n++] = 0; pdu[n++] = 1; /* A */
	pdu[n++] = 0; pdu[n++] = 1; /* IN */
	pdu[n++] = 0; pdu[n++] = 0; pdu[n++] = 0; pdu[n++] = 120; /* ttl */
	pdu[n++] = 0; pdu[n++] = 4;
	pdu[n++] = (ip_hbo >> 24) & 255;
	pdu[n++] = (ip_hbo >> 16) & 255;
	pdu[n++] = (ip_hbo >> 8) & 255;
	pdu[n++] = ip_hbo & 255;
	memset(&dst, 0, sizeof dst);
	dst.sin_family = AF_INET;
	dst.sin_port = htons(5353);
	dst.sin_addr.s_addr = inet_addr("224.0.0.251");
	sendto(fd, pdu, (size_t)n, 0, (struct sockaddr *)&dst, sizeof dst);
	snprintf(line, sizeof line, "init: mdns r3board.local");
	say(line);
	/* держим fd в ребёнке mcp — закрываем здесь; анонс одноразовый */
	close(fd);
}

/* Слушает MCP на link-local. Команды: addr | ifaces | ping <ip> | peers */
static void mcp_serve(uint32_t ip, const char *ifname)
{
	int fd, one, c, n;
	struct sockaddr_in a;
	char line[128], req[160], ans[192];
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		snprintf(line, sizeof line, "init: mcp socket: %s", strerror(errno));
		say(line);
		_exit(1);
	}
	one = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
	memset(&a, 0, sizeof a);
	a.sin_family = AF_INET;
	a.sin_port = htons(8443);
	a.sin_addr.s_addr = htonl(ip);
	if (bind(fd, (struct sockaddr *)&a, sizeof a) != 0) {
		snprintf(line, sizeof line, "init: mcp bind: %s", strerror(errno));
		say(line);
		_exit(1);
	}
	if (listen(fd, 4) != 0) {
		snprintf(line, sizeof line, "init: mcp listen: %s", strerror(errno));
		say(line);
		_exit(1);
	}
	snprintf(line, sizeof line, "init: mcp %u.%u.%u.%u:8443",
		(ip >> 24) & 255, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255);
	say(line);
	mdns_announce(ip, ifname);
	for (;;) {
		c = accept(fd, 0, 0);
		if (c < 0) continue;
		say("init: mcp peer");
		n = (int)recv(c, req, sizeof req - 1, 0);
		if (n <= 0) { close(c); continue; }
		req[n] = 0;
		while (n > 0 && (req[n - 1] == '\n' || req[n - 1] == '\r')) req[--n] = 0;
		if (strcmp(req, "addr") == 0) {
			n = snprintf(ans, sizeof ans, "%u.%u.%u.%u %s\n",
				(ip >> 24) & 255, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255, ifname);
			write(c, ans, (size_t)n);
		} else if (strcmp(req, "ifaces") == 0) {
			dump_links();
			write(c, "ok\n", 3);
		} else if (strncmp(req, "ping ", 5) == 0) {
			mcp_ping(c, req + 5);
		} else if (strcmp(req, "peers") == 0) {
			write(c, "peers: use mdns r3board.local / arp 169.254/16\n", 47);
		} else {
			write(c, "cmds: addr|ifaces|ping <ip>|peers\n", 34);
		}
		close(c);
	}
}

static void see_mcp(void)
{
	uint32_t ip;
	pid_t pid;
	char line[128], ifc[32];
	if (pick_iface(ifc, sizeof ifc) != 0) {
		say("init: mcp none");
		return;
	}
	snprintf(line, sizeof line, "init: zcip %s", ifc);
	say(line);
	ip = zcip_run(ifc, 0);
	if (!ip) {
		say("init: mcp none");
		return;
	}
	snprintf(line, sizeof line, "init: addr %u.%u.%u.%u/16",
		(ip >> 24) & 255, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255);
	say(line);
	pid = fork();
	if (pid < 0) {
		snprintf(line, sizeof line, "init: mcp fork: %s", strerror(errno));
		say(line);
		return;
	}
	if (pid == 0) mcp_serve(ip, ifc);
}

/* Свой run-init. rootfs не принимает pivot_root: корень абсолютный.
 * Сдвигаем смонтированный раздел на / и делаем chroot. */
static int run_init(const char *root, const char *init)
{
	char line[256];
	say("init: before move root");
	if (move_mnt("/run", "/root/run")) return -1;
	if (move_mnt("/sys", "/root/sys")) return -1;
	if (move_mnt("/proc", "/root/proc")) return -1;
	if (move_mnt("/dev", "/root/dev")) return -1;
	if (chdir(root) != 0) {
		say("init: chdir root failed");
		return -1;
	}
	if (mount(".", "/", 0, MS_MOVE, 0) != 0) {
		snprintf(line, sizeof line, "init: move root: %s", strerror(errno));
		say(line);
		return -1;
	}
	if (chroot(".") != 0) {
		snprintf(line, sizeof line, "init: chroot: %s", strerror(errno));
		say(line);
		return -1;
	}
	if (chdir("/") != 0) return -1;
	say("init: root moved");
	see_links();
	see_mcp();
	snprintf(line, sizeof line, "init: exec %s", init);
	say(line);
	execl(init, init, (char *)0);
	snprintf(line, sizeof line, "init: exec %s: %s", init, strerror(errno));
	say(line);
	return -1;
}

int applet_init(int argc, char **argv)
{
	char cmd[512], uuid[80], spec[128], dev[128], line[256];
	const char *src;
	int i, held, do_switch, stay, have_root;
	if (early()) return 1;
	cmd[0] = 0;
	uuid[0] = spec[0] = 0;
	have_root = (read_cmd(cmd, sizeof cmd) == 0) && (init_root_spec(cmd, uuid, 80, spec, 128) == 0);
	do_switch = (argc > 1 && strcmp(argv[1], "switch") == 0) || cmd_has(cmd, "switch");
	/* umbar / boot.img: уже в initramfs — не монтировать отдельный root */
	stay = (argc > 1 && strcmp(argv[1], "stay") == 0) || cmd_has(cmd, "stay")
		|| (have_root && !uuid[0] && (strcmp(spec, "/dev/ram0") == 0 || strcmp(spec, "ram0") == 0
			|| strcmp(spec, "/dev/ram") == 0));
	if (stay) {
		say("init: stay on initramfs");
		see_links();
		see_mcp();
		say("init: waiting (MCP :8443 if link-local ok)");
		for (;;) pause();
	}
	if (!have_root) {
		say("init: no root=");
		return 1;
	}
	snprintf(line, sizeof line, "init: parsed uuid='%s' spec='%s'", uuid, spec);
	say(line);
	if (uuid[0]) {
		if (find_dev(uuid, dev, sizeof dev)) {
			snprintf(line, sizeof line, "init: uuid %s not found", uuid);
			say(line);
			return 1;
		}
		src = dev;
	} else {
		src = spec;
	}
	snprintf(line, sizeof line, "init: device=%s", src);
	say(line);
	held = 0;
	for (i = 0; i < 50; i++) {
		if (mount(src, "/root", "ext4", MS_RDONLY, 0) == 0) {
			held = 1;
			snprintf(line, sizeof line, "init: mount %s ok", src);
			say(line);
			break;
		}
		if (errno == EBUSY) {
			held = 2;
			snprintf(line, sizeof line, "init: mount %s busy", src);
			say(line);
			break;
		}
		snprintf(line, sizeof line, "init: mount %s: %s", src, strerror(errno));
		say(line);
		if (i == 49) return 1;
		usleep(100000);
	}
	if (!do_switch) {
		if (held == 2)
			snprintf(line, sizeof line, "init: %s already mounted, stop before move", src);
		else
			snprintf(line, sizeof line, "init: root %s on /root, stop before move", src);
		say(line);
		for (;;) pause();
	}
	say("init: entering switch");
	if (run_init("/root", "/sbin/init")) {
		say("init: switch failed");
		for (;;) pause();
	}
	return 0;
}
MODULE("init", applet_init);

#endif

#ifdef INIT_PARSE_TEST
int main(void)
{
	char u[80], s[128];
	const char *c = "BOOT_IMAGE=/vmlinuz root=UUID=83991ec6-663b-4b56-ac12-94741cbd5c9d ro";
	if (init_root_spec(c, u, 80, s, 128) || strcmp(u, "83991ec6-663b-4b56-ac12-94741cbd5c9d") || s[0])
		return 1;
	if (init_root_spec("root=/dev/nvme0n1p3 ro", u, 80, s, 128) || u[0] || strcmp(s, "/dev/nvme0n1p3"))
		return 2;
	if (init_root_spec("quiet", u, 80, s, 128) == 0) return 3;
	return 0;
}
#endif
