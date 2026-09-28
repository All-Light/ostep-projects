#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define MAXBUF (8192)

static double get_seconds(void) {
    struct timeval t;
    int rc = gettimeofday(&t, NULL);
    assert(rc == 0);
    return (double)t.tv_sec + (double)t.tv_usec / 1e6;
}

int main(void) {
    const char *query = getenv("QUERY_STRING");
    double spin_for = query == NULL ? 0.0 : (double)atoi(query);
    double start = get_seconds();

    while (get_seconds() - start < spin_for)
        sleep(1);

    double elapsed = get_seconds() - start;
    char content[MAXBUF];
    snprintf(content, sizeof(content),
             "<p>Welcome to the CGI program (%s)</p>\r\n"
             "<p>My only purpose is to waste time on the server!</p>\r\n"
             "<p>I spun for %.2f seconds</p>\r\n",
             query == NULL ? "" : query, elapsed);

    printf("Content-Length: %lu\r\n", (unsigned long)strlen(content));
    printf("Content-Type: text/html\r\n\r\n");
    printf("%s", content);
    return 0;
}
