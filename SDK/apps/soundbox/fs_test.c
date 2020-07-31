#include "system/includes.h"
#include "app_config.h"

int SD_test(void)
{
#ifdef SD_DEV
#define FILE_NAME CONFIG_ROOT_PATH"abc.txt"
    log_d("SD driver test >>>> path: %s", FILE_NAME);
    FILE *fp = NULL;
    u8 str[] = "This is a test string.";
    u8 buf[10];
    u8 len;

    /* void *res = mount(SD_DEV, CONFIG_STORAGE_PATH, "fat", 3, NULL); */

    /* if (res == NULL) { */
    /* log_d("mount fat failed"); */
    /* goto _end; */
    /* } */
    fp = fopen(FILE_NAME, "w+");
    if (!fp) {
        log_d("open file ERR!");
        return -1;
    }

    len = fwrite(fp, str, sizeof(str));

    if (len != sizeof(str)) {
        log_d("write file ERR!");
        goto _end;
    }

    fseek(fp, 0, SEEK_SET);

    len = fread(fp, buf, sizeof(buf));

    if (len != sizeof(buf)) {
        log_d("read file ERR!");
        goto _end;
    }

    put_buf(buf, sizeof(buf));
    log_d("SD ok!");

_end:
    if (fp) {
        fclose(fp);
    }
#endif
    return 0;
}


#define SDFILE_NEW_FILE1 	SDFILE_RES_ROOT_PATH"tone/bt.wtg"
#define SDFILE_NEW_FILE2 	SDFILE_RES_ROOT_PATH"cfg_tool.bin"
#if (USE_SDFILE_NEW == 1)
#define SDFILE_NEW_FILE3 	SDFILE_RES_ROOT_PATH"btif"
#else
#define SDFILE_NEW_FILE3 	SDFILE_RES_ROOT_PATH"RESERVED_CONFIG/btif"
#endif
#define SDFILE_NEW_FILE4 	SDFILE_RES_ROOT_PATH"cfg_tool.bin"
#define SDFILE_READ_LEN 	0x20

void sdfile_test(void)
{
    FILE *fp = NULL;
    u8 buf[SDFILE_READ_LEN];
    u32 len;
    int ret;

    printf("sdfile test >>>>>>>>");
    fp = fopen(SDFILE_NEW_FILE1, "r");

    if (!fp) {
        printf("file open fail");
    }

    printf("file open succ");

    ret = fread(fp, buf, SDFILE_READ_LEN);
    if (ret == SDFILE_READ_LEN) {
        printf("file read succ");
        put_buf(buf, SDFILE_READ_LEN);
    }

    fclose(fp);

    fp = fopen(SDFILE_NEW_FILE4, "r");

    if (!fp) {
        printf("file open fail");
    }

    printf("file open succ4444");

    ret = fread(fp, buf, SDFILE_READ_LEN);
    if (ret == SDFILE_READ_LEN) {
        printf("file read succ444");
        put_buf(buf, SDFILE_READ_LEN);
    }

    fclose(fp);

//////////////////////////////////
    fp = fopen(SDFILE_NEW_FILE2, "r");

    if (!fp) {
        printf("file open fail2");
    }

    ret = fread(fp, buf, SDFILE_READ_LEN);
    if (ret == SDFILE_READ_LEN) {
        printf("file read succ2");
        put_buf(buf, SDFILE_READ_LEN);
    }


    ret = fread(fp, buf, SDFILE_READ_LEN);
    if (ret == SDFILE_READ_LEN) {
        printf("file read succ2");
        put_buf(buf, SDFILE_READ_LEN);
    }

    fseek(fp, 0x200, SEEK_SET);

    ret = fread(fp, buf, SDFILE_READ_LEN);
    if (ret == SDFILE_READ_LEN) {
        printf("file read succ3");
        put_buf(buf, SDFILE_READ_LEN);
    }

    printf("file open succ2");
    fclose(fp);

//////////////////////////////////
    fp = fopen(SDFILE_NEW_FILE3, "r");

    if (!fp) {
        printf("file open fail3");
    }
    printf("file open succ3");

    ret = fread(fp, buf, SDFILE_READ_LEN);
    if (ret == SDFILE_READ_LEN) {
        printf("file read succ4");
        put_buf(buf, SDFILE_READ_LEN);
    }

    char str_buf[] = "test btif write";

    fseek(fp, 0, SEEK_SET);
    ret = fwrite(fp, str_buf, sizeof(str_buf));
    if (ret == sizeof(str_buf)) {
        printf("file write succ");
    }

    fseek(fp, 0, SEEK_SET);
    printf("file will read");
    ret = fread(fp, buf, sizeof(str_buf));
    if (ret == sizeof(str_buf)) {
        printf("file read succ5");
        put_buf(buf, sizeof(str_buf));
    }

    fclose(fp);
}


