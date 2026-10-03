/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Greeter unlock secret. The file is not master.passwd.
 * One record:
 *   bhotkeys:<crypt>:0:0::0:0:bhotkeys panel:/var/empty:/usr/sbin/nologin
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "bhotkeys.h"

int
bh_passwd_ok(const char *pw)
{
	FILE *fp;
	char line[512];
	char *hash, *end;
	const char *got;

	if (pw == NULL || pw[0] == '\0')
		return (0);
	fp = fopen(BH_PASSWD_FILE, "r");
	if (fp == NULL)
		return (-1);
	if (fgets(line, sizeof(line), fp) == NULL) {
		fclose(fp);
		return (0);
	}
	fclose(fp);
	if (strncmp(line, "bhotkeys:", 9) != 0)
		return (0);
	hash = line + 9;
	end = strchr(hash, ':');
	if (end == NULL)
		return (0);
	*end = '\0';
	if (hash[0] == '\0')
		return (0);
	got = crypt(pw, hash);
	if (got == NULL)
		return (0);
	return (strcmp(got, hash) == 0);
}
