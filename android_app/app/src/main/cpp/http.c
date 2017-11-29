
#include <stdio.h>
#include <stdlib.h> /* malloc() */
#include <string.h> /* strncpy() */
#include <strings.h> /* strncasecmp() */
#include <ctype.h> /* isblank() */


static const char http_503[] =
        "HTTP/1.1 503 Service Temporarily Unavailable\r\n"
                "Content-Type: text/html\r\n"
                "Connection: close\r\n\r\n"
                "Backend not available";


/*
 * Parses a HTTP request for the Host: header
 *
 * Returns:
 *  >=0  - length of the hostname and updates *hostname
 *         caller is responsible for freeing *hostname
 *  -1   - Incomplete request
 *  -2   - No Host header included in this request
 *  -3   - Invalid hostname pointer
 *  -4   - malloc failure
 *  < -4 - Invalid HTTP request
 *
 */

#include "tun2http.h"

int
get_header(const char *header, const char *data, size_t data_len, char *value) {
    int len, header_len;

    header_len = strlen(header);

    /* loop through headers stopping at first blank line */
    while ((len = next_header(&data, &data_len)) != 0)
        if (len > header_len && strncasecmp(header, data, header_len) == 0) {
            /* Eat leading whitespace */
            while (header_len < len && isblank(data[header_len]))
                header_len++;

            if (value == NULL)
                return -4;

            strncpy(value, data + header_len, len - header_len);
            value[len - header_len] = '\0';

            return len - header_len;
        }

    /* If there is no data left after reading all the headers then we do not
     * have a complete HTTP request, there must be a blank line */
    if (data_len == 0)
        return -1;

    return -2;
}

int
next_header(const char **data, size_t *len) {
    int header_len;

    /* perhaps we can optimize this to reuse the value of header_len, rather
     * than scanning twice.
     * Walk our data stream until the end of the header */
    while (*len > 2 && (*data)[0] != '\r' && (*data)[1] != '\n') {
        (*len)--;
        (*data)++;
    }

    /* advanced past the <CR><LF> pair */
    *data += 2;
    *len -= 2;

    /* Find the length of the next header */
    header_len = 0;
    while (*len > header_len + 1
           && (*data)[header_len] != '\r'
           && (*data)[header_len + 1] != '\n')
        header_len++;

    return header_len;
}

uint8_t *find_data(uint8_t *data, size_t data_len, char *value) {

    int found = 0;
    int value_length = strlen(value);

    while (!found && data_len > 2) {
        while (data[0] != value[0] && data_len > 2) {
            data++;
            data_len--;
        }
        if (strncasecmp(value, data, value_length) == 0) {
            found = 1;
        } else {
            data++;
            data_len--;
        }
    }
    if (found) {
        return data;
    }

    return 0;
}


uint8_t *patch_http_url(uint8_t *data, size_t *data_len) {
    char hostname[512];
    uint8_t *host = find_data(data, *data_len, "Host: ");
    int length = 0;
    if (host) {
        host += 6;
        while (*host != '\r' && length < 511) {
            hostname[length] = *host;
            host++;
            length++;
        }
        hostname[length] = '\0';
    } else {
        return 0;
    }

    //GET POST PUT DELETE HEAD
    char *word;
    uint8_t *pos = 0;
    if (pos = find_data(data, *data_len, "GET ")) {
        word = "GET ";
    } else if (pos = find_data(data, *data_len, "POST ")) {
        word = "POST ";
    } else if (pos = find_data(data, *data_len, "PUT ")) {
        word = "PUT ";
    } else if (pos = find_data(data, *data_len, "DELETE ")) {
        word = "DELETE ";
    } else if (pos = find_data(data, *data_len, "HEAD ")) {
        word = "HEAD ";
    }

    if (!pos) {
        return 0;
    }


    int http_len = strlen("http://");
    int word_len = strlen(word);
    int pos1 = pos - data + word_len;

    if (data[pos1] == 'h' &&
        data[pos1 + 1] == 't' &&
        data[pos1 + 2] == 't' &&
        data[pos1 + 3] == 'p' &&
        data[pos1 + 4] == ':') {
        return 0;
    }

    uint8_t *new_data = malloc(2*(*data_len + http_len + length));

    uint8_t *d = new_data;

    memset(new_data, 0, *data_len + http_len + length);
    memcpy(d, data, pos1);
    d += pos1;
    memcpy(d, "http://", http_len);
    d += http_len;
    memcpy(d, hostname, length);
    d += length;
    memcpy(d, data + pos1, *data_len - pos1);

    *data_len += http_len + length;

    return new_data;
};