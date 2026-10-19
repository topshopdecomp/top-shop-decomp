/*
 * psyq.h - minimal stand-in for the PsyQ SDK declarations used by src/.
 *
 * Written from the PsyQ 4.x library documentation for syntax checking only.
 * Replace with the real LIBCD.H / LIBDS.H / LIBSND.H / LIBGPU.H etc. if you
 * have the SDK. Only what src/ actually uses is declared here.
 */
#ifndef PSYQ_H
#define PSYQ_H

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned long u_long;

#ifndef NULL
#define NULL ((void *)0)
#endif

/* libc */
void *memset(void *s, int c, unsigned long n); /* size_t on host; u_int on PSX */
void *memcpy(void *dst, const void *src, unsigned long n);
int memcmp(const void *a, const void *b, unsigned long n);
int rand(void);
void srand(unsigned int seed);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, unsigned long n);
char *strcat(char *dst, const char *src);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, unsigned long n);
unsigned long strlen(const char *s);   /* int on PSX; size_t-compatible for host checks */
int sprintf(char *buf, const char *fmt, ...);
int printf(const char *fmt, ...);
int abs(int x);

/* libcd */
typedef struct { u_char minute, second, sector, track; } CdlLOC;
typedef struct { u_char file, chan; u_short pad; } CdlFILTER;

#define CdlPause      0x09
#define CdlSetfilter  0x0D
#define CdlGetlocP    0x11
#define CdlSeekL      0x15
#define CdlReadS      0x1B

#define CdlModeSpeed  0x80
#define CdlModeRT     0x40
#define CdlModeSF     0x08

#define CdlNoIntr     0x00
#define CdlDataReady  0x01
#define CdlComplete   0x02
#define CdlDiskError  0x05

int CdControl(u_char com, u_char *param, u_char *result);
int CdControlF(u_char com, u_char *param);
int CdRead(int sectors, u_long *buf, int mode);
int CdRead2(long mode);
int CdReadSync(int mode, u_char *result);
int CdSync(int mode, u_char *result);
int CdReady(int mode, u_char *result);
int CdInit(void);
void CdFlush(void);
int CdReset(int mode);

/* libds */
typedef struct { u_char minute, second, sector, track; } DslLOC;
int DsPosToInt(DslLOC *p);

/* libsnd */
#define SS_SERIAL_A 0
void SsSetSerialVol(char s_num, short voll, short volr);
void SsSetSerialAttr(char s_num, char attr, char mode);
void SsSetTableSize(char *table, short s_max, short t_max);
void SsSetTickMode(long tick_mode);
short SsSeqOpen(u_long *addr, short vab_id);
void SsSeqClose(short seq_access_num);
void SsSeqPlay(short seq_access_num, char play_mode, short l_count);
void SsSeqStop(short seq_access_num);
void SsSeqPause(short seq_access_num);
void SsSeqReplay(short seq_access_num);
void SsSeqSetVol(short seq_access_num, short voll, short volr);
short SsIsEos(short access_num, short seq_num);
short SsVabOpenHead(u_char *addr, short vab_id);
short SsVabTransBody(u_char *addr, short vab_id);
short SsVabTransCompleted(short immediate_flag);
void SsVabClose(short vab_id);
void SsSetMVol(short voll, short volr);
void SsSetRVol(short voll, short volr);
short SsUtSetReverbType(short type);
void SsUtSetReverbDepth(short ldepth, short rdepth);
void SsUtReverbOn(void);
void SsUtReverbOff(void);
void SsSetMono(void);
void SsSetStereo(void);
short SsUtKeyOn(short vab_id, short prog, short tone, short note, short fine, short voll, short volr);
short SsUtKeyOff(short voice, short vab_id, short prog, short tone, short note);
short SsUtKeyOnV(short voice, short vab_id, short prog, short tone, short note, short fine,
                 short voll, short volr);
short SsUtKeyOffV(short voice);
void SsUtAllKeyOff(short mode);
void SsUtSetVVol(short vc, short voll, short volr);
short SsUtGetVabHdr(short vab_id, void *vabhdrptr);

/* libspu */
void SpuInitHot(void);

/* libds */
typedef struct { u_char val0, val1, val2, val3; } DslATV;
int DsMix(DslATV *vol);

/* libapi / libetc */
int ResetCallback(void);
void SetMem(int megabytes);
int VSync(int mode);
long OpenEvent(unsigned long desc, long spec, long mode, long (*func)());
long CloseEvent(long event);
long EnableEvent(long event);
long DisableEvent(long event);
long TestEvent(long event);
struct DIRENTRY
{
    char name[20];
    long attr;
    long size;
    struct DIRENTRY *next;
    long head;
    char system[4];
};
long open(char *devname, unsigned long flag);
long close(long fd);
long read(long fd, void *buf, long n);
long write(long fd, void *buf, long n);
long lseek(long fd, long offset, long flag);
long erase(char *name);
struct DIRENTRY *firstfile(char *name, struct DIRENTRY *dir);
struct DIRENTRY *nextfile(struct DIRENTRY *dir);
long _card_info(long chan);
long _card_load(long chan);
long _card_clear(long chan);
long _card_format(long chan);
void InitCARD(long pad_enable);
long StartCARD(void);
long StopCARD(void);
void _bu_init(void);
void InitTAP(char *bufA, long lenA, char *bufB, long lenB);
void StartTAP(void);
void ChangeClearPAD(long val);
#define SwCARD      0xf4000001
#define HwCARD      0xf0000011
#define EvSpIOE     0x0004
#define EvSpERROR   0x8000
#define EvSpTIMOUT  0x0100
#define EvSpNEW     0x2000
#define EvMdNOINTR  0x2000
int VSyncCallback(void (*f)(void));
long SetVideoMode(long mode);
void EnterCriticalSection(void);
void ExitCriticalSection(void);

/* controller button bits (PsyQ PAD* values; match the game's PadState words) */
#define PADL2      0x0001
#define PADR2      0x0002
#define PADL1      0x0004
#define PADR1      0x0008
#define PADRup     0x0010   /* triangle */
#define PADRright  0x0020   /* circle */
#define PADRdown   0x0040   /* cross */
#define PADRleft   0x0080   /* square */
#define PADselect  0x0100
#define PADstart   0x0800
#define PADLup     0x1000
#define PADLright  0x2000
#define PADLdown   0x4000
#define PADLleft   0x8000

/* libgpu */
typedef struct { short x, y, w, h; } RECT;
typedef struct { u_long tag; u_long code[15]; } DR_ENV;
typedef struct
{
    RECT clip;
    short ofs[2];
    RECT tw;
    u_short tpage;
    u_char dtd, dfe, isbg, r0, g0, b0;
    DR_ENV dr_env;
} DRAWENV;                                   /* size 0x5C */
typedef struct
{
    RECT disp;
    RECT screen;
    u_char isinter, isrgb24, pad0, pad1;
} DISPENV;
int DrawSync(int mode);
int LoadImage(RECT *rect, u_long *p);
int StoreImage(RECT *rect, u_long *p);
int MoveImage(RECT *rect, int x, int y);
int ClearImage(RECT *rect, u_char r, u_char g, u_char b);
int ResetGraph(int mode);
void SetDispMask(int mask);
DR_ENV *SetDrawEnv(DR_ENV *dr_env, DRAWENV *env);
void AddPrim(void *ot, void *p);
void DrawOTag(u_long *p);
u_long *ClearOTag(u_long *ot, int n);
u_long *ClearOTagR(u_long *ot, int n);
DRAWENV *PutDrawEnv(DRAWENV *env);
DISPENV *PutDispEnv(DISPENV *env);
u_short GetTPage(int tp, int abr, int x, int y);
u_short GetClut(int x, int y);
void FntLoad(int tx, int ty);
int FntOpen(int x, int y, int w, int h, int isbg, int n);
void SetDumpFnt(int id);
int FntPrint(const char *fmt, ...);
u_long *FntFlush(int id);
typedef struct { u_long tag; u_long code[2]; } DR_AREA;
void SetDrawArea(DR_AREA *p, RECT *r);
/* textured quad packet: color, then 4 vertices with texture coordinates */
typedef struct
{
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char u0, v0; u_short clut;
    short x1, y1;
    u_char u1, v1; u_short tpage;
    short x2, y2;
    u_char u2, v2; u_short pad1;
    short x3, y3;
    u_char u3, v3; u_short pad2;
} POLY_FT4;
/* textured triangle packet */
typedef struct
{
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char u0, v0; u_short clut;
    short x1, y1;
    u_char u1, v1; u_short tpage;
    short x2, y2;
    u_char u2, v2; u_short pad1;
} POLY_FT3;
/* free-size sprite packet */
typedef struct
{
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char u0, v0; u_short clut;
    short w, h;
} SPRT;
void SetPolyFT3(void *p);
void SetPolyFT4(void *p);
void SetSprt(void *p);
void SetSemiTrans(void *p, int abe);
typedef struct
{
    u_long tag;
    u_long code[2];
} DR_TWIN;                                   /* texture window setting, size 12 */

void SetTexWindow(void *p, RECT *tw);
void SetDrawOffset(void *p, u_short *ofs);
typedef struct { u_long tag; u_long code[2]; } DR_MODE;
void SetDrawMode(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw);
typedef struct { u_long tag; u_long code[1]; } DR_TPAGE;
void SetDrawTPage(DR_TPAGE *p, int dfe, int dtd, int tpage);

/* libgte */
int rsin(int a);
int rcos(int a);
int ratan2(int y, int x);

/* libgs */
typedef struct { u_long bits; } GsOT_TAG;   /* PsyQ: unsigned p:24, num:8 */
typedef struct
{
    u_long length;
    GsOT_TAG *org;
    u_long offset;
    u_long point;
    GsOT_TAG *tag;
} GsOT;
typedef struct
{
    u_long pmode;
    short px, py;
    u_short pw, ph;
    u_long *pixel;
    short cx, cy;
    u_short cw, ch;
    u_long *clut;
} GsIMAGE;
void GsGetTimInfo(u_long *im, GsIMAGE *tim);
void GsClearOt(unsigned short offset, unsigned short point, GsOT *otp);
void GsInitGraph(unsigned short w, unsigned short h, unsigned short intmode,
                 unsigned short dith, unsigned short varmmode);
void GsDefDispBuff(unsigned short x0, unsigned short y0, unsigned short x1, unsigned short y1);
void GsSwapDispBuff(void);
int GsGetActiveBuff(void);
void GsSetWorkBase(void *base);
void GsDrawOt(GsOT *ot);
GsOT *GsSortOt(GsOT *src, GsOT *dst);
void GsSortClear(u_char r, u_char g, u_char b, GsOT *ot);
typedef struct
{
    u_long attribute;
    short x0, y0;
    short x1, y1;
    u_char r, g, b;
} GsLINE;
void GsSortLine(GsLINE *lp, GsOT *otp, unsigned short pri);
typedef struct
{
    u_long attribute;
    short x, y;
    u_short w, h;
    u_short tpage;
    u_char u, v;
    short cx, cy;
    u_char r, g, b;
    short mx, my;
    short scalex, scaley;
    long rotate;
} GsSPRITE;                                  /* size 0x24 */
typedef struct
{
    u_long attribute;
    short x, y;
    u_short w, h;
    u_char r, g, b;
} GsBOXF;                                    /* size 0x10 */
void GsSortSprite(GsSPRITE *sp, GsOT *otp, unsigned short pri);
void GsSortFastSprite(GsSPRITE *sp, GsOT *otp, unsigned short pri);
void GsSortBoxFill(GsBOXF *bp, GsOT *otp, unsigned short pri);

/* libsnd (more) */
void SsInit(void);

#endif /* PSYQ_H */
