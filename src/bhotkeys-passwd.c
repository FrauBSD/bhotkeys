/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Set the greeter Super+/ unlock password.
 * Writes PREFIX/etc/bhotkeys.passwd (mode 0600). Not a login account.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "bhotkeys.h"

static int
read_secret(const char *prompt, char *dst, size_t n)
{
	struct termios old, neu;
	FILE *tty;
	int fd, echo_off = 0;

	tty = fopen("/dev/tty", "r+");
	if (tty == NULL)
		return (-1);
	fd = fileno(tty);
	fputs(prompt, tty);
	fflush(tty);
	if (tcgetattr(fd, &old) == 0) {
		neu = old;
		neu.c_lflag &= ~(tcflag_t)(ECHO | ECHOE | ECHOK | ECHONL);
		if (tcsetattr(fd, TCSAFLUSH, &neu) == 0)
			echo_off = 1;
	}
	if (fgets(dst, (int)n, tty) == NULL)
		dst[0] = '\0';
	else {
		char *nl = strchr(dst, '\n');

		if (nl != NULL)
			*nl = '\0';
	}
	if (echo_off)
		tcsetattr(fd, TCSAFLUSH, &old);
	fputc('\n', tty);
	fclose(tty);
	if (dst[0] == '\0')
		return (-1);
	return (0);
}

static void
make_salt(char *dst, size_t n)
{
	static const char set[] =
	    "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	unsigned char rnd[16];
	size_t i;

	arc4random_buf(rnd, sizeof(rnd));
	for (i = 0; i < sizeof(rnd) && i + 1 < n; i++)
		dst[i] = set[rnd[i] % (sizeof(set) - 1)];
	dst[i] = '\0';
}

int
main(int argc, char **argv)
{
	char a[128], b[128], salt[20], setting[32];
	const char *hash;
	FILE *fp;
	char tmp[] = BH_PASSWD_FILE ".XXXXXX";
	int fd;

	(void)argv;
	if (argc > 1) {
		fprintf(stderr, "Usage: bhotkeys-passwd\n");
		return (1);
	}
	if (geteuid() != 0) {
		fprintf(stderr, "bhotkeys-passwd: must be root\n");
		return (1);
	}
	if (read_secret("New password: ", a, sizeof(a)) != 0 ||
	    read_secret("Retype password: ", b, sizeof(b)) != 0) {
		fprintf(stderr, "bhotkeys-passwd: empty password\n");
		return (1);
	}
	if (strcmp(a, b) != 0) {
		explicit_bzero(a, sizeof(a));
		explicit_bzero(b, sizeof(b));
		fprintf(stderr, "bhotkeys-passwd: passwords do not match\n");
		return (1);
	}
	make_salt(salt, sizeof(salt));
	snprintf(setting, sizeof(setting), "$6$%s$", salt);
	hash = crypt(a, setting);
	explicit_bzero(a, sizeof(a));
	explicit_bzero(b, sizeof(b));
	if (hash == NULL) {
		fprintf(stderr, "bhotkeys-passwd: crypt failed\n");
		return (1);
	}
	fd = mkstemp(tmp);
	if (fd < 0) {
		perror("bhotkeys-passwd");
		return (1);
	}
	fp = fdopen(fd, "w");
	if (fp == NULL) {
		close(fd);
		unlink(tmp);
		return (1);
	}
	fprintf(fp, "bhotkeys:%s:0:0::0:0:%s:/var/empty:/usr/sbin/nologin\n",
	    hash, "bhotkeys panel");
	if (fclose(fp) != 0) {
		unlink(tmp);
		return (1);
	}
	if (rename(tmp, BH_PASSWD_FILE) != 0) {
		perror("bhotkeys-passwd");
		unlink(tmp);
		return (1);
	}
	chmod(BH_PASSWD_FILE, 0600);
	return (0);
}
