#include "app_ipcam_ota.h"
#include "app_ipcam_otacgi.h"
#include "app_ipcam_ircut.h"
#include "app_ipcam_paramparse.h"
#include "app_ipcam_ircut.h"
#include "libhttpd.h"
#include <sys/prctl.h>

char *cgiRequestMethod;
char *cgiQueryString;
char cgiContentTypeData[1024];
char *cgiContentType = cgiContentTypeData;
char *cgiMultipartBoundary;
char *cgiCookie;
int cgiContentLength;
int cgiFileSize;

static FILE *s_httpConnFd = NULL;   //http conn_fd
static FILE *s_printfOutput = NULL; //log output
static FILE *s_otaStatusLog = NULL; //ota status log output to web

/* True if CGI environment was restored from a file. */
static int s_cgiRestored = 0;
/* The first form entry. */
static CgiFormEntry_S *cgiFormEntryFirst;

//用于接收ota.html的"upload"按钮上传的固件
void *app_ipcam_OTA_UploadFW(void *param)
{
    char *e;
    httpd_conn *tmpParam = param;

    CgiSetupConstants();
    CgiGetEnv(&cgiRequestMethod, "REQUEST_METHOD");
    CgiGetEnv(&cgiQueryString, "QUERY_STRING");

    e = tmpParam->contenttype;
    if (e)
    {
        if (strlen(e) < sizeof(cgiContentTypeData))
        {
            strcpy(cgiContentType, e);
        }
        else
        {
            /* Truncate safely in the event of what is almost certainly
				a hack attempt */
            strncpy(cgiContentType, e, sizeof(cgiContentTypeData));
            cgiContentType[sizeof(cgiContentTypeData) - 1] = '\0';
        }
    }
    else
    {
        cgiContentType[0] = '\0';
    }

    /* Never null */
    cgiMultipartBoundary = "";
    /* 2.0: parse semicolon-separated additional parameters of the
		content type. The one we're interested in is 'boundary'.
		We discard the rest to make cgiContentType more useful
		to the typical programmer. */
    if (strchr(cgiContentType, ';'))
    {
        char *sat = strchr(cgiContentType, ';');
        while (sat)
        {
            *sat = '\0';
            sat++;
            while (isspace(*sat))
            {
                sat++;
            }
            if (CgiStrBeginsNc(sat, "boundary="))
            {
                char *s;
                cgiMultipartBoundary = sat + strlen("boundary=");
                s = cgiMultipartBoundary;
                while ((*s) && (!isspace(*s)))
                {
                    s++;
                }
                *s = '\0';
                break;
            }
            else
            {
                sat = strchr(sat, ';');
            }
        }
    }
    cgiContentLength = tmpParam->contentlength;
    cgiCookie = tmpParam->cookie;
    if (tmpParam->method == METHOD_POST)
    {
        printf("method = %s\n", httpd_method_str(tmpParam->method));
        cgiRequestMethod = "post";
    }
    if (strlen(cgiCookie) != 0 && strchr(cgiCookie, '=') != NULL)
    {
        sscanf(cgiCookie, "filesize=%d", &cgiFileSize);
        printf("fileSize = %d\n", cgiFileSize);
    }
    CGICDEBUGLOG("Multipart boundary = %s , cookie = %s , connent length = %d , request method = %s , connect type = %s\n",
                 cgiMultipartBoundary, cgiCookie, cgiContentLength, cgiRequestMethod, cgiContentType);
    cgiFormEntryFirst = 0;
    // s_httpConnFd = stdin;
    s_httpConnFd = fdopen(tmpParam->conn_fd, "r");
    s_printfOutput = stdout;
    s_cgiRestored = 0;
    if(access(OTA_STATUS_LOG_DIR , R_OK) != 0)
    {
        printf("[%s][%d] %s is not R_OK.\n" , __func__ , __LINE__ , OTA_STATUS_LOG_DIR);
        if(mkdir(OTA_STATUS_LOG_DIR , 0755) == -1)
            printf("[%s][%d] create %s failed!\n" , __func__ , __LINE__ , OTA_STATUS_LOG_DIR);
    }
    s_otaStatusLog = fopen(OTA_STATUS_LOG_FILE, "w");
    if (s_otaStatusLog == NULL) {
        printf("fopen[%s]\n", OTA_STATUS_LOG_FILE);
        return NULL;
    }

    if (CgiStrEqNc(cgiRequestMethod, "post"))
    {
        CGICDEBUGLOG("POST recognized\n");
        if (CgiStrEqNc(cgiContentType, "application/x-www-form-urlencoded"))
        {
            CGICDEBUGLOG("Calling PostFormInput\n");
            if (CgiParsePostFormInput() != cgiParseSuccess)
            {
                CGICDEBUGLOG("PostFormInput failed\n");
                CgiHeaderStatus(500, "Error reading form data");
                CgiFreeResources();
                fclose(s_otaStatusLog);
                return NULL;
            }
            CGICDEBUGLOG("PostFormInput succeeded\n");
        }
        else if (CgiStrEqNc(cgiContentType, "multipart/form-data"))
        {
            CGICDEBUGLOG("Calling PostMultipartInput\n");
            if (CgiParsePostMultipartInput() != cgiParseSuccess)
            {
                CGICDEBUGLOG("PostMultipartInput failed\n");
                CgiHeaderStatus(500, "Error reading form data");
                CgiFreeResources();
                fclose(s_otaStatusLog);
                return NULL;
            }
            fprintf(s_otaStatusLog, "Upload Done\n");
            fflush(s_otaStatusLog);
            CGICDEBUGLOG("PostMultipartInput succeeded\n");
        }
    }
    else if (CgiStrEqNc(cgiRequestMethod, "get"))
    {
        /* The spec says this should be taken care of by
			the server, but... it isn't */
        cgiContentLength = strlen(cgiQueryString);
        if (CgiParseGetFormInput() != cgiParseSuccess)
        {
            CGICDEBUGLOG("GetFormInput failed\n");
            CgiHeaderStatus(500, "Error reading form data");
            CgiFreeResources();
            return NULL;
        }
        else
        {
            CGICDEBUGLOG("GetFormInput succeeded\n");
        }
    }

    return (void *)s_otaStatusLog;
}

static void CgiGetEnv(char **s, char *var)
{
    *s = getenv(var);
    if (!(*s))
    {
        *s = "";
    }
}

static CgiParseResultType_E CgiParsePostFormInput()
{
    char *input;
    CgiParseResultType_E result;
    if (!cgiContentLength)
    {
        return cgiParseSuccess;
    }
    input = (char *)malloc(cgiContentLength);
    if (!input)
    {
        return cgiParseMemory;
    }
    if (((int)fread(input, 1, cgiContentLength, s_httpConnFd)) != cgiContentLength)
    {
        free(input);
        return cgiParseIO;
    }
    result = CgiParseFormInput(input, cgiContentLength);
    free(input);
    return result;
}

int mpRead(MpStreamPtr_S mpp, char *buffer, int len)
{
    int ilen = len;
    int got = 0;
    /* Refuse to read past the declared length in order to
		avoid deadlock */
    if (len > (cgiContentLength - mpp->offset))
    {
        len = cgiContentLength - mpp->offset;
    }
    while (len)
    {
        if (mpp->readPos != mpp->writePos)
        {
            *buffer++ = mpp->putback[mpp->readPos++];
            mpp->readPos %= sizeof(mpp->putback);
            got++;
            len--;
        }
        else
        {
            break;
        }
    }
    if (len)
    {
        int fgot = fread(buffer, 1, len, s_httpConnFd);
        if (fgot >= 0)
        {
            mpp->offset += (got + fgot);
            return got + fgot;
        }
        else if (got > 0)
        {
            mpp->offset += got;
            return got;
        }
        else
        {
            /* EOF or error */
            return fgot;
        }
    }
    else if (got)
    {
        mpp->offset += got;
        return got;
    }
    else if (ilen)
    {
        return EOF;
    }
    else
    {
        /* 2.01 */
        return 0;
    }
}

void mpPutBack(MpStreamPtr_S mpp, char *data, int len)
{
    mpp->offset -= len;
    while (len)
    {
        mpp->putback[mpp->writePos++] = *data++;
        mpp->writePos %= sizeof(mpp->putback);
        len--;
    }
}

static CgiParseResultType_E readHeadForZip(void)
{
    int got = 0;
    MpStream_S mp;
    MpStreamPtr_S mpp = &mp;
    char cmpHeadData[] = "compressed\r\n\r\n";
    int cmpHeadLength = strlen(cmpHeadData);
    char d[100] = {0};
    memset(&mp, 0, sizeof(mp));

    printf("cmpHeadData = %s , HeadLength = %d\n", cmpHeadData, cmpHeadLength);
    int boffset = 0;
    int dcount = 0;

    while (1)
    {
        got = mpRead(mpp, d, 1);
        if (got != 1)
        {
            /* 2.01: cgiParseIO, not cgiFormIO */
            printf("mpRead return got = %d\n", got);
            return cgiParseIO;
        }
        if (dcount > 2000)
        {
            printf("We matched zip header failed!\n");
            return  cgiParseIO;
        }
        dcount++;

        if (d[0] == cmpHeadData[boffset])
        {
            printf("We matched zip header.boffset = %d , zipHeadLength = %d\n", boffset, cmpHeadLength);
            boffset++;
            if (boffset == cmpHeadLength)
            {
                break;
            }
        }
        else
            boffset = 0;
    }
    return cgiParseSuccess;
}
static CgiParseResultType_E CgiParsePostMultipartInput()
{
    int got;
    FILE *outf = NULL;

    int readcnt = 0, totalcnt = cgiFileSize;
    char fileBUF[4096] = {0};

    GetTempFile(&outf);
    if (outf == NULL)
    {
        printf("GetTempFile error!\n");
        return cgiParseIO;
    }
    else
    {
        //read boundary
        // fprintf(s_otaStatusLog, "check the http head.\n");
        // fflush(s_otaStatusLog);
        if (readHeadForZip() == cgiParseIO)
        {
            fprintf(s_otaStatusLog, "check http head failed!\n");
            fflush(s_otaStatusLog);
            return cgiParseIO;
        }
    }

    int readOnce = 4096;

    while (readcnt < totalcnt)
    {
        if (readOnce > totalcnt - readcnt)
            readOnce = totalcnt - readcnt;

        memset(fileBUF, 0, sizeof(fileBUF));
        got = fread(fileBUF, 1, readOnce, s_httpConnFd);
        if (readOnce < got)
        {
            printf("the buffer is too long! (%d)\n", got - readOnce);
            fileBUF[readOnce] = '\0';
            fwrite(fileBUF, 1, readOnce, outf);
            printf("read %d bytes got %d bytes , total = %d\n", readcnt, readOnce, readcnt + readOnce);
        }
        else
        {
            fwrite(fileBUF, 1, got, outf);
        }
        readcnt += got;
        printf("read %d bytes got %d bytes\n", readcnt, got);
    }
    printf("readcnt = %d , totalcnt = %d\n", readcnt, totalcnt);

    fclose(outf);

    return cgiParseSuccess;
}

static CgiParseResultType_E GetTempFile(FILE **tFile)
{
    /* tfileName must be 1024 bytes to ensure adequacy on
		win32 (1024 exceeds the maximum path length and
		certainly exceeds observed behavior of _tmpnam).
		May as well also be 1024 bytes on Unix, although actual
		length is strlen(cgiTempDir) + a short unique pattern. */
    char tfileName[1024];

#ifndef WIN32
    /* Unix. Use the robust 'mkstemp' function to create
		a temporary file that is truly unique, with
		permissions that are truly safe. The 
		fopen-for-write destroys any bogus information
		written by potential hackers during the brief
		window between the file's creation and the
		chmod call (glibc 2.0.6 and lower might
		otherwise have allowed this). */
    strcpy(tfileName, UPLOAD_FW_TEMP_DIR "/cgicXXXXXX");
    //check the tmp dir
    if(access(UPLOAD_FW_TEMP_DIR , R_OK) == -1)
    {
        if(mkdir(UPLOAD_FW_TEMP_DIR , 0755) == -1)
        {
            printf("[%s][%d] mkdir %s failed!\n" , __func__ , __LINE__ , UPLOAD_FW_TEMP_DIR);
            return cgiParseIO;
        }
    }
    //check the tmp file
    if(access(tfileName , F_OK) == 0)
    {
        if(remove(tfileName) == -1)
        {
            printf("[%s][%d] remove %s failed!\n" , __func__ , __LINE__ , tfileName);
            return cgiParseIO;
        }
    }
    /* Fix the permissions */
    if(creat(tfileName , 0600) == -1)
    {
        printf("[%s][%d] creat %s failed!\n" , __func__ , __LINE__ , tfileName);
        return cgiParseIO;
    }
#else
    /* Non-Unix. Do what we can. */
    if (!tmpnam(tfileName))
    {
        return cgiParseIO;
    }
#endif
    *tFile = fopen(tfileName, "w+b");
    // unlink(tfileName);
    setenv("OTA_FILE", tfileName, 1);
    return cgiParseSuccess;
}

static CgiParseResultType_E CgiParseGetFormInput()
{
    return CgiParseFormInput(cgiQueryString, cgiContentLength);
}

static CgiParseResultType_E CgiParseFormInput(char *data, int length)
{
    /* Scan for pairs, unescaping and storing them as they are found. */
    int pos = 0;
    CgiFormEntry_S *n;
    CgiFormEntry_S *l = 0;
    while (pos != length)
    {
        int foundAmp = 0;
        int start = pos;
        int len = 0;
        char *attr;
        char *value;
        while (pos != length)
        {
            if (data[pos] == '&')
            {
                /* Tolerate attr name without a value. This will fall through
					and give us an empty value */
                break;
            }
            if (data[pos] == '=')
            {
                pos++;
                break;
            }
            pos++;
            len++;
        }
        if (!len)
        {
            break;
        }
        if (CgiUnescapeChars(&attr, data + start, len) != cgiUnescapeSuccess)
        {
            return cgiParseMemory;
        }
        start = pos;
        len = 0;
        while (pos != length)
        {
            if (data[pos] == '&')
            {
                foundAmp = 1;
                pos++;
                break;
            }
            pos++;
            len++;
        }
        /* The last pair probably won't be followed by a &, but
			that's fine, so check for that after accepting it */
        if (CgiUnescapeChars(&value, data + start, len) != cgiUnescapeSuccess)
        {
            free(attr);
            return cgiParseMemory;
        }
        /* OK, we have a new pair, add it to the list. */
        n = (CgiFormEntry_S *)malloc(sizeof(CgiFormEntry_S));
        if (!n)
        {
            free(attr);
            free(value);
            return cgiParseMemory;
        }
        n->attr = attr;
        n->value = value;
        n->valueLength = strlen(n->value);
        n->fileName = (char *)malloc(1);
        if (!n->fileName)
        {
            free(attr);
            free(value);
            free(n);
            return cgiParseMemory;
        }
        n->fileName[0] = '\0';
        n->contentType = (char *)malloc(1);
        if (!n->contentType)
        {
            free(attr);
            free(value);
            free(n->fileName);
            free(n);
            return cgiParseMemory;
        }
        n->contentType[0] = '\0';
        n->next = 0;
        if (!l)
        {
            cgiFormEntryFirst = n;
        }
        else
        {
            l->next = n;
        }
        l = n;
        if (!foundAmp)
        {
            break;
        }
    }
    return cgiParseSuccess;
}

static int cgiHexValue[256];

CgiUnescapeResultType_E CgiUnescapeChars(char **sp, char *cp, int len)
{
    char *s;
    CgiEscapeState_E escapeState = cgiEscapeRest;
    int escapedValue = 0;
    int srcPos = 0;
    int dstPos = 0;
    s = (char *)malloc(len + 1);
    if (!s)
    {
        return cgiUnescapeMemory;
    }
    while (srcPos < len)
    {
        int ch = cp[srcPos];
        switch (escapeState)
        {
        case cgiEscapeRest:
            if (ch == '%')
            {
                escapeState = cgiEscapeFirst;
            }
            else if (ch == '+')
            {
                s[dstPos++] = ' ';
            }
            else
            {
                s[dstPos++] = ch;
            }
            break;
        case cgiEscapeFirst:
            escapedValue = cgiHexValue[ch] << 4;
            escapeState = cgiEscapeSecond;
            break;
        case cgiEscapeSecond:
            escapedValue += cgiHexValue[ch];
            s[dstPos++] = escapedValue;
            escapeState = cgiEscapeRest;
            break;
        }
        srcPos++;
    }
    s[dstPos] = '\0';
    *sp = s;
    return cgiUnescapeSuccess;
}

static void CgiSetupConstants()
{
    int i;
    for (i = 0; (i < 256); i++)
    {
        cgiHexValue[i] = 0;
    }
    cgiHexValue['0'] = 0;
    cgiHexValue['1'] = 1;
    cgiHexValue['2'] = 2;
    cgiHexValue['3'] = 3;
    cgiHexValue['4'] = 4;
    cgiHexValue['5'] = 5;
    cgiHexValue['6'] = 6;
    cgiHexValue['7'] = 7;
    cgiHexValue['8'] = 8;
    cgiHexValue['9'] = 9;
    cgiHexValue['A'] = 10;
    cgiHexValue['B'] = 11;
    cgiHexValue['C'] = 12;
    cgiHexValue['D'] = 13;
    cgiHexValue['E'] = 14;
    cgiHexValue['F'] = 15;
    cgiHexValue['a'] = 10;
    cgiHexValue['b'] = 11;
    cgiHexValue['c'] = 12;
    cgiHexValue['d'] = 13;
    cgiHexValue['e'] = 14;
    cgiHexValue['f'] = 15;
}

static void CgiFreeResources()
{
    CgiFormEntry_S *c = cgiFormEntryFirst;
    CgiFormEntry_S *n;
    while (c)
    {
        n = c->next;
        free(c->attr);
        free(c->value);
        free(c->fileName);
        free(c->contentType);
        if (c->tFile)
        {
            fclose(c->tFile);
        }
        free(c);
        c = n;
    }
    /* If the cgi environment was restored from a saved environment,
		then these are in allocated space and must also be freed */
    if (s_cgiRestored)
    {
        free(cgiRequestMethod);
        free(cgiQueryString);
        free(cgiContentType);
    }
    /* 2.0: to clean up the environment for cgiReadEnvironment,
		we must set these correctly */
    cgiFormEntryFirst = 0;
    s_cgiRestored = 0;
}

void cgiHeaderLocation(char *redirectUrl)
{
    fprintf(s_printfOutput, "Location: %s\r\n\r\n", redirectUrl);
}

static void CgiHeaderStatus(int status, char *statusMessage)
{
    fprintf(s_printfOutput, "Status: %d %s\r\n\r\n", status, statusMessage);
}

static int CgiStrEqNc(char *s1, char *s2)
{
    while (1)
    {
        if (!(*s1))
        {
            if (!(*s2))
            {
                return 1;
            }
            else
            {
                return 0;
            }
        }
        else if (!(*s2))
        {
            return 0;
        }
        if (isalpha(*s1))
        {
            if (tolower(*s1) != tolower(*s2))
            {
                return 0;
            }
        }
        else if ((*s1) != (*s2))
        {
            return 0;
        }
        s1++;
        s2++;
    }
}

static int CgiStrBeginsNc(char *s1, char *s2)
{
    while (1)
    {
        if (!(*s2))
        {
            return 1;
        }
        else if (!(*s1))
        {
            return 0;
        }
        if (isalpha(*s1))
        {
            if (tolower(*s1) != tolower(*s2))
            {
                return 0;
            }
        }
        else if ((*s1) != (*s2))
        {
            return 0;
        }
        s1++;
        s2++;
    }
}

//用于解压upgrade.zip包，以及进行升级
void *app_ipcam_OTA_UnpackAndUpdate(void *param)
{
    char *tmpFile = getenv("OTA_FILE");
    CVI_CHAR TaskName[64] = {0};
    sprintf(TaskName, "%s", "OTAUnpackAndUpdate");
    prctl(PR_SET_NAME, TaskName, 0, 0, 0);

    fprintf(s_otaStatusLog, "<hr>OTA Unpack Zip %s Start ...\n", tmpFile);
    fflush(s_otaStatusLog);
    CGICDEBUGLOG("OTA Unpack Zip(%s) start...\n", tmpFile);

    int ret = app_ipcam_OTA_UnpackZip(tmpFile, s_otaStatusLog);
    if (ret != OTA_RET_OK)
    {
        CGICDEBUGLOG("unpack Zip(%s) failed! ret = %d\n", tmpFile, ret);
        goto FAIL;
    }

    fprintf(s_otaStatusLog, "<hr>OTA Unpack Zip %s Success\n", tmpFile);
    fprintf(s_otaStatusLog, "<hr>OTA Update Firmware Start...\n");
    fflush(s_otaStatusLog);

    CGICDEBUGLOG("OTA Update Firmware Start ...\n");
    ret = app_ipcam_OTA_UpdataFW(s_otaStatusLog);
    if (ret != OTA_RET_OK)
    {
        CGICDEBUGLOG("OTA Update Firmware Fail ...\n");
        goto FAIL;
    }

    if (tmpFile)
    {
        printf("[%d] remove tmp file...\n" , __LINE__);
        remove(tmpFile);
    }
    printf("[%d] reboot the board...\n" , __LINE__);
    goto END;
FAIL:
    fprintf(s_otaStatusLog, "<hr>OTA Upload Fail\n");
    fflush(s_otaStatusLog);
    CGICDEBUGLOG("OTA Update Fail.\n");
    return NULL;
END:
    fprintf(s_otaStatusLog, "<hr>OTA Upload Success\n");
    fflush(s_otaStatusLog);
    CGICDEBUGLOG("OTA Update Success.\n");

    fprintf(s_otaStatusLog, "<hr>system reboot ...\n");
    fflush(s_otaStatusLog);
    system("/bin/sync;/bin/sleep 1;/sbin/reboot -f");
    return NULL;
}

//用于ota.html上的显示系统版本号
int app_ipcam_OTA_GetSystemVersion(char *buf, int bufLen)
{
    char readBuf[512] = {0};

    if (buf == NULL)
        return OTA_RET_FAIL;

    FILE *fp = popen("cat /proc/version", "r");
    if (fp != NULL)
    {
        while (fgets(readBuf + strlen(readBuf), sizeof(readBuf) - strlen(readBuf), fp) != NULL);
        pclose(fp);
    }
    snprintf(buf, bufLen, "\r\n<br/> %s \r\n", readBuf);

    return OTA_RET_OK;
}

//用于ota.html上获取实时的升级状态
int app_ipcam_OTA_GetUpgradeStatus(char *buf, int bufLen)
{
    char readBuf[512] = {0};
    if (buf == NULL)
        return OTA_RET_FAIL;

    FILE *fp = popen("cat " OTA_STATUS_LOG_FILE, "r");
    if (fp != NULL)
    {
        while (fgets(readBuf + strlen(readBuf), sizeof(readBuf) - strlen(readBuf), fp) != NULL);
        pclose(fp);
    }
    snprintf(buf, bufLen, "\r\n<br/> %s \r\n", readBuf);
    return 0;
}

//烧录firmware之前需要把其他线程给关闭掉，防止Squashfs Error
int app_ipcam_OTA_CloseThreadBeforeUpgrade(void)
{
    CVI_S32 ret = OTA_RET_OK;

    #ifdef RECORD_SUPPORT
    APP_CHK_RET(app_ipcam_Record_UnInit(), "running SD Record");
    #endif
    APP_CHK_RET(app_ipcam_IrCut_DeInit(), "IrCut Stop");

    #ifdef OSD_SUPPORT
    APP_CHK_RET(app_ipcam_Osdc_DeInit(), "OsdC DeInit");
    #endif

    #ifdef AI_SUPPORT
    APP_CHK_RET(app_ipcam_Ai_PD_Stop(), "PD Stop");

    APP_CHK_RET(app_ipcam_Ai_MD_Stop(), "MD Stop");

    #ifdef FACE_SUPPORT
    APP_CHK_RET(app_ipcam_Ai_FD_Stop(), "FD Stop");
    #endif
    #endif

    #ifdef AUDIO_SUPPORT
    APP_CHK_RET(app_ipcam_Audio_UnInit(), "Audio Stop");
    #endif

    APP_CHK_RET(app_ipcam_Vpss_DeInit(), "Vpss DeInit");

    APP_CHK_RET(app_ipcam_Venc_Stop(APP_VENC_ALL), "Venc Stop");

    APP_CHK_RET(app_ipcam_Vi_DeInit(), "Vi DeInit");
    
    APP_CHK_RET(app_ipcam_Sys_DeInit(), "System DeInit");

    APP_CHK_RET(app_ipcam_rtsp_Server_Destroy(), "RTSP Server Destroy");

    return ret;
}
