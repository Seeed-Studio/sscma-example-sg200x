#include <stdio.h>
#include <string.h>

int exec_cmd(const char* cmd, char* result, const char* param)
{
    FILE* fp;
    char full_cmd[CMD_BUF_SIZE] = "";
    size_t len;

    if (NULL == cmd) {
        return -1;
    }

    snprintf(full_cmd, sizeof(full_cmd), "%s %s", cmd, (param == NULL) ? "" : param);

    fp = popen(full_cmd, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to run `%s`(%s)\n", cmd, strerror(errno));
        return -1;
    }

    if (result != NULL) {
        if (fgets(result, CMD_BUF_SIZE, fp) != NULL) {
            len = strlen(result);
            if (len > 0 && result[len - 1] == '\n') {
                result[len - 1] = '\0';
            }
        } else {
            *result = '\0';
        }
    }
    pclose(fp);

    return 0;
}