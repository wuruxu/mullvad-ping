#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <zip.h>

typedef struct {
  char* filename;
  char* content;
} mullvad_conf_t;

void* do_ping_thread(void* data) {
  char pingcmd[256] = {0};
  double packlost = 0.0, avg = 0.0;
  char* ipaddr;
  int ipv = 4;
  mullvad_conf_t* conf = (mullvad_conf_t *)data;
  char* wgconf = (char*)conf->filename;
  char* pubkey = NULL;

  char* key = strstr(conf->content, "PublicKey = ");
  if(key != NULL) {
    key = key + 12;
    *(key + 44) = '\0';
  }
  pubkey = strdup(key);

  char* ptr = strstr(key + 48 , "Endpoint = ");
  if(ptr != NULL) {
    //printf("STEP00 wgbuf = %s, ptr = %s\n", wgbuf, ptr);
    ptr = ptr + 11;
    if(*ptr == '[') {
      ptr ++;
      ipv = 6;
    }
    ipaddr = ptr;
    if(ipv == 6) {
      while(*ptr != ']') ptr++;
    } else {
      while(*ptr != ':') ptr++;
    }
    *ptr = '\0';
    ipaddr = strdup(ipaddr);
    //printf("wgconf = %s , ipaddr = %s key = %s\n", wgconf, ipaddr, key);
    //printf("CONTENT = %s\n", conf->content);
  }

  snprintf(pingcmd, 256, "ping -%d -c 12 %s", ipv, ipaddr);

  FILE* fp = popen(pingcmd, "r");
  if(fp != NULL) {
    char pingbuf[5120] = {0};
    int nread = fread(pingbuf, sizeof(char), 5120, fp);
    pclose(fp);

    char* ptr = strstr(pingbuf, " packet loss");
    char* next = ptr + 12;
    char* restore_ptr = NULL;
    do {
      ptr--;
      if(*ptr == '%') {
        *ptr = '\0';
        restore_ptr = ptr;
      }
    } while(*ptr != ' ');
    packlost = atof(ptr);
    //printf("packlost = %f\n", packlost);
    if(packlost == 0.0) {
      ptr = strstr(next, "max = ");
      if(ptr != NULL) {
        char* saveptr = NULL;
	    char* token = NULL;
        ptr = ptr + 7;
	    token = strtok_r(ptr, "/", &saveptr);
	    if(token != NULL) {
          token = strtok_r(NULL, "/", &saveptr);
	      if(token != NULL) {
            avg = atof(token);
            *restore_ptr = '%';
            printf("pingbuf = %s\n", pingbuf);
            printf("wgconf = %s , ping(%s).avg = %fms\n", wgconf, ipaddr, avg);
            printf("wg set wg0 peer %s endpoint %s:12202 persistent-keepalive 45 allowed-ips 0.0.0.0/0 allowed-ips ::0/0\n\n", pubkey, ipaddr);
          }
        }
      }
    } /** packlist == 0 */
  }

  free(conf->content);
  free(conf->filename);
  free(conf);
  return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <zipfile> <prefix>\n", argv[0]);
        return 1;
    }

    const char *zip_filename = argv[1];
    const char *prefix = argv[2]; // Prefix from command line
    size_t prefix_len = strlen(prefix);

    int err = 0;
    zip_t *zip = zip_open(zip_filename, 0, &err);

    if (zip == NULL) {
        fprintf(stderr, "Error opening zip archive '%s': %s\n", zip_filename, zip_strerror(zip));
        return 1;
    }

    int num_entries = zip_get_num_entries(zip, 0);

    for (int i = 0; i < num_entries; ++i) {
        mullvad_conf_t* conf = calloc(1, sizeof(mullvad_conf_t));
        const char *name = zip_get_name(zip, i, 0);

        if (name == NULL) {
            fprintf(stderr, "Error getting name for entry %d\n", i);
            continue;
        }

        if (strncmp(name, prefix, prefix_len) == 0) { // Check if the filename starts with the prefix
            zip_file_t *file = zip_fopen_index(zip, i, 0);

            if (file == NULL) {
                fprintf(stderr, "Error opening file '%s' in zip archive.\n", name);
                continue;
            }

            zip_stat_t stat;
            if (zip_stat_index(zip, i, 0, &stat) != 0) {
                fprintf(stderr, "Error getting file stats for '%s'.\n", name);
                zip_fclose(file);
                continue;
            }

            char *buffer = (char *)malloc(stat.size + 1);
            pthread_t tid;

            if (buffer == NULL) {
                fprintf(stderr, "Memory allocation error.\n");
                zip_fclose(file);
                continue;
            }

            zip_int64_t bytes_read = zip_fread(file, buffer, stat.size);

            if (bytes_read != stat.size) {
                fprintf(stderr, "Error reading file content. Read %ld bytes, expected %ld.\n", bytes_read, stat.size);
                free(buffer);
                zip_fclose(file);
                continue;
            }

            buffer[stat.size] = '\0';
            conf->filename = strdup(name);
            conf->content = buffer;

            pthread_create(&tid, NULL, do_ping_thread, (void *)conf);

            //printf("File: %s\n%s\n", name, buffer);
            //free(buffer);
            zip_fclose(file);
        }
    }

    zip_close(zip);

    sleep(60);
    return 0;
}
