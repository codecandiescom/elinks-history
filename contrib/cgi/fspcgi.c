/* CGI script for FSP protocol support */
/* $Id: fspcgi.c,v 1.2 2005/06/27 19:09:31 witekfl Exp $ */
#include <fsplib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>

char *pname, *query;

struct fq {
	char *password;
	char *host;
	char *path;
	unsigned short port;
} data;

void
error(const char *str)
{
	printf("Content-Type: text/plain\r\nConnection: close\r\n\r\n");
	puts(str);
	printf("%s\n", query);
	exit(1);
}

void
process_directory(FSP_SESSION *ses)
{
	char buf[1024];
	FSP_DIR *dir;
	/* TODO: password */

	snprintf(buf, 1024, "file://%s?%s:%d%s/", pname, data.host, data.port, data.path);
	printf("Content-Type: text/html\r\n\r\n");
	printf("<html><head><title>%s</title></head><body>\n", buf);
	dir = fsp_opendir(ses, data.path);
	if (dir) {
		FSP_RDENTRY fentry, *fresult;

		fsp_rewinddir(dir);
		while (!fsp_readdir_native(dir, &fentry, &fresult)) {
			if (!fresult) break;
			printf("<a href=\"%s%s\">%s</a><br>\n", buf, fentry.name, fentry.name);
		}
		fsp_closedir(dir);
	}	
	puts("</body></html>");
	fsp_close_session(ses);
	exit(0);
}

void
process_data(void)
{
	FSP_SESSION *ses = fsp_open_session(data.host, data.port, data.password);
	struct stat sb;

	if (!ses) error("Session initialization failed.");
	if (fsp_stat(ses, data.path, &sb)) error("File not found.");
	if (S_ISDIR(sb.st_mode)) process_directory(ses);
	else { /* regular file */
		char buf[4096];
		FSP_FILE *file = fsp_fopen(ses, data.path, "r");
		int r;

		if (!file) error("fsp_fopen error.");
		printf("Content-Type: application/octet-stream\r\nContent-Length: %d\r\n"
			"Connection: close\r\n\r\n", sb.st_size);
		while ((r = fsp_fread(buf, 1, 4096, file)) > 0) fwrite(buf, 1, r, stdout);
		fsp_fclose(file);
		fsp_close_session(ses);
		exit(0);
	}
}

void
process_query(void)
{
	char *at = strchr(query, '@');
	char *colon;
	char *slash;

	if (at) {
		*at = '\0';
		data.password = strdup(query);
		query = at + 1;
	}
	colon = strchr(query, ':');
	if (colon) {
		*colon = '\0';
		data.host = strdup(query);
		data.port = atoi(colon + 1);
		slash = strchr(colon + 1, '/');
		if (slash) {
			data.path = strdup(slash);
		} else {
			data.path = "/";
		}
	} else {
		data.port = 21;
		slash = strchr(query, '/');
		if (slash) {
			*slash = '\0';
			data.host = strdup(query);
			*slash = '/';
			data.path = strdup(slash);
		} else {
			data.host = strdup(query);
			data.path = "/";
		}
	}
	process_data();
}


int
main(int argc, char **argv)
{
	char *q = getenv("QUERY_STRING");

	if (!q) return 1;
	pname = argv[0];
	query = strdup(q);
	if (!query) return 2;
	process_query();
	return 0;
}
