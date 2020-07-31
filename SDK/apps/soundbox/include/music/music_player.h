#ifndef _MUSIC_PLAYER_H
#define _MUSIC_PLAYER_H

#include "system/app_core.h"
#include "system/includes.h"
#include "server/server_core.h"
#include "media/audio_decoder.h"
#include "file_operate/file_operate.h"


#define MUSIC_DECRYPT_EN		0
#define MUSIC_DECRYPT_KEY		0x12345678

#define MUSIC_ID3_V1_EN			0
#define MUSIC_ID3_V2_EN			0

enum {
    MPLY_OK = 0,
    MPLY_ERR_POINT = (('M' << 24) | ('P' << 16) | ('\0' << 8) | '\0'),
    MPLY_ERR_MALLOC,
    MPLY_ERR_BP,
    MPLY_ERR_CMD_NO_SUPPORT,
    MPLY_ERR_SERVER_CMD,
};

typedef struct _MOPR_BP_ {
    u32 fsize;
    u32 sclust;
    struct audio_dec_breakpoint bp;
} MOPR_BP;
#define FILE_BP_LEN(p)		((p)->bp.len + ((u32)(p)->bp.data - (u32)(p)))

typedef struct _MOPR_DEC_ {
    void *decoder;
    void *event_handler;
    void *event_priv;
} MOPR_DEC;

typedef struct _MUSIC_PLAYER_ {
    FILE_OPERATE *fopr;
    MOPR_BP *mbp;
    MOPR_DEC *mdec;
} MUSIC_PLAYER;


void music_ply_save_bp(MUSIC_PLAYER *m_ply);
void music_ply_clear_bp(MUSIC_PLAYER *m_ply);

int music_ply_start(MUSIC_PLAYER *m_ply, struct audio_dec_breakpoint *bp);

int music_ply_stop(MUSIC_PLAYER *m_ply);

int music_ply_bp_file_open(MUSIC_PLAYER *m_ply, FILE_OPR_SEL_DEV *dev_sel, FILE_OPR_SEL_FILE *file_sel, MOPR_BP *mbp);
int music_ply_file_open(MUSIC_PLAYER *m_ply, FILE_OPR_SEL_DEV *dev_sel, FILE_OPR_SEL_FILE *file_sel);


void music_ply_set_recplay_status(MUSIC_PLAYER *m_ply, u8 status);
u8 music_ply_get_recplay_status(MUSIC_PLAYER *m_ply);

MUSIC_PLAYER *music_ply_create(void *priv, void (*handler)(void *, int argc, int *argv));
void music_ply_release(MUSIC_PLAYER *m_ply);


#endif

