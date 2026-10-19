/*
 * save.c - game saves on the memory card
 *
 * The save file: four game slots and a system block with the options and the story progress.
 * Packing a game in progress into a slot and unpacking it, the tasks of the save and load
 * screens with their card checks and questions, the story progress (the stages reached, the
 * characters unlocked) and saving it after a story stage is won.
 */
#include "common.h"
#include "topshop.h"

/* Card_SaveTask steps */
enum CardSaveStep
{
    CSAVE_CHECKING             =    0,  /* "Checking Memory Card... Please do not remove Memory Card." */
    CSAVE_WAIT_CHECKING        =    1,
    CSAVE_CHECK                =    2,  /* Card_Check */
    CSAVE_STATUS               =    3,  /* on g_cardStatus */
    CSAVE_WAIT_NO_CARD         =    4,
    CSAVE_NO_CARD              =  200,  /* "No Memory Card detected. Please insert Memory Card." Yes / Quit */
    CSAVE_NO_CARD_ANSWER       =  201,
    CSAVE_RECHECKING           =  210,  /* "Checking Memory Card..." */
    CSAVE_WAIT_RECHECKING      =  211,
    CSAVE_RECHECK_1            =  212,  /* (a frame) */
    CSAVE_RECHECK_2            =  213,  /* (a frame) */
    CSAVE_RECHECK_EVENTS       =  214,
    CSAVE_RECHECK              =  215,  /* Card_Check */
    CSAVE_LOAD_FAILED          =  220,  /* "Load failed." */
    CSAVE_WAIT_LOAD_FAILED     =  221,
    CSAVE_WAIT_LOAD_FAILED_MSG =  222,
    CSAVE_STILL_NO_CARD        =  225,
    CSAVE_STILL_NO_CARD_MSG    =  226,  /* "No Memory Card detected." */
    CSAVE_WAIT_STILL_NO_CARD   =  227,
    CSAVE_NOT_FORMATTED        =  230,
    CSAVE_NOT_FORMATTED_MSG    =  231,  /* "Memory Card is not formatted." */
    CSAVE_ASK_FORMAT           =  232,  /* "Do you wish to format?" */
    CSAVE_FORMAT_ANSWER        =  233,
    CSAVE_FORMATTING           =  234,  /* "Formatting... Please do not remove Memory Card." */
    CSAVE_WAIT_FORMATTING      =  235,
    CSAVE_FORMAT_CHECK         =  236,
    CSAVE_FORMAT               =  237,  /* Card_Format */
    CSAVE_FORMAT_DONE          =  238,
    CSAVE_FORMAT_FAILED        =  240,  /* "Format failed." */
    CSAVE_WAIT_FORMAT_FAILED   =  241,
    CSAVE_FORMAT_COMPLETE      =  250,  /* "Format complete." */
    CSAVE_OPEN_FILE            =  300,
    CSAVE_NO_FILE              =  310,  /* no save file yet: room for its 2 blocks? */
    CSAVE_NO_SPACE_MSG         =  311,  /* "Not enough space on Memory Card." */
    CSAVE_WAIT_NO_SPACE        =  312,
    CSAVE_READ_FILE            =  320,  /* the system block and the used slots into the save buffer */
    CSAVE_READ_DONE            =  321,
    CSAVE_READY                =  322,
    CSAVE_NEW_FILE             =  330,
    CSAVE_NO_DATA_MSG          =  331,  /* "No saved data found." */
    CSAVE_FRESH_BUFFER         =  332,  /* a new file: empty slots, the progress and the options */
    CSAVE_STILL_THERE          =  333,  /* the card still there? then CSAVE_READY */
    CSAVE_REMOVED              = 2331   /* the card was taken out */
};

/* Task: check the memory card before the board's save prompt saves. The card has to be there and
 * formatted (it offers to format it); the save file is read into the save buffer and checked,
 * or, when there is none, there has to be room for it and the buffer gets a fresh system block.
 * g_cardTaskState: 1 = ready to save, 0 = canceled or failed, 2 = the card was removed.
 * Returns 1 when done (0x80032954) */
/* 0x80032954 */
u32 Card_SaveTask(u16 *step)
{
    int result;
    int block;
    int i;
    MsgWork *msg;

    switch (*step)
    {
    case CSAVE_CHECKING:
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,0,"%CChecking Memory Card\201c%KPlease do not remove Memory Card.%E");
        break;
    case CSAVE_WAIT_CHECKING:
    case CSAVE_WAIT_RECHECKING:
    case CSAVE_WAIT_FORMATTING:
        Task_GetWork(g_uiTaskIds[0],&msg);
        if (g_textWins[msg->win].state != TEXTWIN_SHOWN)
        {
            return 0;
        }
        break;
    case CSAVE_CHECK:
        Card_EnableEvents();
        Card_Check();
        break;
    case CSAVE_STATUS:
        result = g_cardStatus;
        if (result == 0)
        {
            *step = CSAVE_OPEN_FILE;
            return 0;
        }
        if (result == 3)
        {
            *step = CSAVE_NOT_FORMATTED;
            return 0;
        }
        if ((result != 1) && (result != 2))
        {
            return 0;
        }
        Msg_Close();                    /* no card */
        break;
    case CSAVE_WAIT_NO_CARD:
        if (Msg_IsDone() != 0)
        {
            Card_DisableEvents();
            *step = CSAVE_NO_CARD;
        }
        return 0;

    /* no card: insert one or quit */
    case CSAVE_NO_CARD:
        Msg_AskYesNoAt(0,0,2,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,
                       "%CNo Memory Card detected.%KPlease insert Memory Card.%KYes%KQuit%E");
        break;
    case CSAVE_NO_CARD_ANSWER:
        if (Msg_IsAnswered() == 0)
        {
            return 0;
        }
        if (g_msgAnswer == 0)
        {
            *step = CSAVE_RECHECKING;
            return 0;
        }
        g_cardTaskState = CARDTASK_CANCELED;
        return 1;
    case CSAVE_RECHECKING:
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,0,"%CChecking Memory Card\201c%KPlease do not remove Memory Card.%E");
        break;
    case CSAVE_RECHECK_1:
    case CSAVE_RECHECK_2:
        break;
    case CSAVE_RECHECK_EVENTS:
        Card_EnableEvents();
        break;
    case CSAVE_RECHECK:
        result = Card_Check();
        if (result == 0)
        {
            *step = CSAVE_OPEN_FILE;
        }
        else if ((result == 1) || (result == 2))
        {
            *step = CSAVE_STILL_NO_CARD;
        }
        else if (result == 3)
        {
            *step = CSAVE_NOT_FORMATTED;
        }
        return 0;

    /* failures */
    case CSAVE_LOAD_FAILED:
        Msg_Close();
        break;
    case CSAVE_WAIT_LOAD_FAILED:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,MSG_ANY_BUTTON,"%CLoad failed.%E");
        break;
    case CSAVE_WAIT_LOAD_FAILED_MSG:
    case CSAVE_WAIT_STILL_NO_CARD:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        g_cardTaskState = CARDTASK_CANCELED;
        Card_DisableEvents();
        return 1;
    case CSAVE_STILL_NO_CARD:
        Msg_Close();
        break;
    case CSAVE_STILL_NO_CARD_MSG:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,MSG_ANY_BUTTON,"%CNo Memory Card detected.%E");
        break;

    /* not formatted: format it? */
    case CSAVE_NOT_FORMATTED:
        Msg_Close();
        Card_DisableEvents();
        break;
    case CSAVE_NOT_FORMATTED_MSG:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0x3c,MSG_ANY_BUTTON,"%CMemory Card is not formatted.%E");
        break;
    case CSAVE_ASK_FORMAT:
        Card_EnableEvents();
        result = Card_Info(0);
        Card_DisableEvents();
        if ((result != 0) && (result != 3))
        {
            Msg_CloseChoice();
            g_cardTaskState = CARDTASK_REMOVED;
            *step = CSAVE_REMOVED;
            return 0;
        }
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        Msg_AskYesNo(0,0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,"%CDo you wish to format?%KYes%KNo%E");
        Task_GetWork(g_uiTaskIds[0],&msg);
        TextWin_SetMenu(msg->win,1,2,1);
        break;
    case CSAVE_FORMAT_ANSWER:
        Card_EnableEvents();
        result = Card_Info(0);
        Card_DisableEvents();
        if (result != 0)
        {
            Msg_CloseChoice();
            g_cardTaskState = CARDTASK_REMOVED;
            *step = CSAVE_REMOVED;
            return 0;
        }
        if (Msg_IsAnswered() == 0)
        {
            return 0;
        }
        if (g_msgAnswer != 0)
        {
            g_cardTaskState = CARDTASK_CANCELED;
            return 1;
        }
        break;
    case CSAVE_FORMATTING:
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,0,"%CFormatting\201c%KPlease do not remove Memory Card.%E");
        break;
    case CSAVE_FORMAT_CHECK:
        Card_EnableEvents();
        Card_Check();
        result = g_cardStatus;
        if ((result == 1) || (result == 2))
        {
            Msg_Close();
            g_cardFormatResult = 1;
            *step = CSAVE_FORMAT_DONE;
            return 0;
        }
        if ((result != 0) && (result != 3))
        {
            return 0;
        }
        break;
    case CSAVE_FORMAT:
        Card_Format();
        Msg_Close();
        break;
    case CSAVE_FORMAT_DONE:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        if (g_cardFormatResult == 0)
        {
            *step = CSAVE_FORMAT_COMPLETE;
        }
        else if (g_cardFormatResult == 1)
        {
            *step = CSAVE_FORMAT_FAILED;
        }
        return 0;
    case CSAVE_FORMAT_FAILED:
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,MSG_ANY_BUTTON,"%CFormat failed.%E");
        break;
    case CSAVE_WAIT_FORMAT_FAILED:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        g_cardTaskState = CARDTASK_CANCELED;
        Card_DisableEvents();
        return 1;
    case CSAVE_FORMAT_COMPLETE:
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,MSG_ANY_BUTTON,"%CFormat complete.%E");
        *step = CSAVE_FRESH_BUFFER;
        return 0;

    /* the save file */
    case CSAVE_OPEN_FILE:
        result = Card_OpenFile("BASLUS-01333TOPSHOP",1);
        if (result == 0)
        {
            *step = CSAVE_READ_FILE;
        }
        else if (result == 1)
        {
            *step = CSAVE_LOAD_FAILED;
        }
        else if (result == 2)
        {
            *step = CSAVE_NO_FILE;      /* no save file yet */
        }
        return 0;
    case CSAVE_NO_FILE:
        if (1 < 15 - g_cardUsedBlocks)
        {
            *step = CSAVE_NEW_FILE;     /* room for the file's 2 blocks */
            return 0;
        }
        Msg_Close();
        break;
    case CSAVE_NO_SPACE_MSG:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,MSG_ANY_BUTTON,"%CNot enough space on Memory Card.%E");
        break;
    case CSAVE_WAIT_NO_SPACE:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        g_cardTaskState = CARDTASK_CANCELED;
        Card_DisableEvents();
        return 1;
    case CSAVE_READ_FILE:
        /* read the system block and every used slot into the save buffer (which mirrors the file) */
        block = SecHeap_Alloc(8);
        g_saveBuf = g_secHeapBlocks[(short)block].addr;
        g_cardBufBlock = block;
        g_cardBuf = (CardSystem *)(g_saveBuf + 0x200);
        g_saveSlots[0] = g_saveBuf + 0x280;
        g_saveSlots[1] = g_saveBuf + 0xc00;
        g_saveSlots[2] = g_saveBuf + 0x1580;
        g_saveSlots[3] = g_saveBuf + 0x1f00;
        if (Card_ReadFile((u8 *)g_cardBuf,0x200,0x80) != 0)
        {
            Card_CloseFile();
            *step = CSAVE_LOAD_FAILED;
            return 0;
        }
        if (g_cardBuf->checksum != Save_Checksum((u8 *)g_cardBuf,0x1a))
        {
            *step = CSAVE_LOAD_FAILED;
            return 0;
        }
        for (i = 0; i < 4; i++)
        {
            if ((g_cardBuf->slotUsed[i] != 0) &&
                ((Card_ReadFile(g_saveSlots[i],k_saveSlotOffsets[i],0x980) != 0) ||
                 (((SaveImage *)g_saveSlots[i])->checksum != Save_Checksum(g_saveSlots[i],0x974))))
            {
                Card_CloseFile();
                *step = CSAVE_LOAD_FAILED;
                return 0;
            }
        }
        break;
    case CSAVE_READ_DONE:
        Card_CloseFile();
        Msg_Close();
        break;
    case CSAVE_STILL_THERE:
        Card_EnableEvents();
        result = Card_Info(0);
        Card_DisableEvents();
        if ((result != 0) && (result != 3))
        {
            Msg_CloseChoice();
            g_cardTaskState = CARDTASK_REMOVED;
            *step = CSAVE_REMOVED;
            return 0;
        }
        /* fall through */
    case CSAVE_READY:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        g_cardTaskState = CARDTASK_READY;
        Card_DisableEvents();
        return 1;
    case CSAVE_NEW_FILE:
        Msg_Close();
        break;
    case CSAVE_NO_DATA_MSG:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,MSG_ANY_BUTTON,"%CNo saved data found.%E");
        break;
    case CSAVE_FRESH_BUFFER:
        /* a new file: empty slots, the current progress and options */
        block = SecHeap_Alloc(8);
        g_saveBuf = g_secHeapBlocks[(short)block].addr;
        g_cardBufBlock = block;
        g_cardBuf = (CardSystem *)(g_saveBuf + 0x200);
        g_saveSlots[0] = g_saveBuf + 0x280;
        g_saveSlots[1] = g_saveBuf + 0xc00;
        g_saveSlots[2] = g_saveBuf + 0x1580;
        g_saveSlots[3] = g_saveBuf + 0x1f00;
        for (i = 0; i < 4; i++)
        {
            g_cardBuf->slotUsed[i] = 0;
        }
        g_cardBuf->progress = g_progress;
        g_cardBuf->eventScenes = g_optEventScenes;
        g_cardBuf->mono = g_optMono;
        g_cardBuf->skipCpuTalk = g_optSkipCpuTalk;
        g_cardBuf->cpuFast = g_optCpuFast;
        break;

    /* the card was removed */
    case CSAVE_REMOVED:
        return Msg_IsAnswered() != 0;
    default:
        return 0;
    }
    *step += 1;
    return 0;
}

/* Card_ReadSavesTask steps */
enum CardReadSavesStep
{
    CREAD_CHECKING         =   0,  /* "Checking Memory Card... Please do not remove Memory Card." */
    CREAD_WAIT_CHECKING    =   1,
    CREAD_CHECK            =   2,  /* Card_Check */
    CREAD_STATUS           =   3,  /* on g_cardStatus */
    CREAD_NO_CARD          = 200,  /* "No Memory Card detected. Please insert Memory Card." Yes / Quit */
    CREAD_NO_CARD_ANSWER   = 201,
    CREAD_LOAD_FAILED      = 220,  /* "Load failed." */
    CREAD_WAIT_LOAD_FAILED = 221,
    CREAD_OPEN_FILE        = 300,
    CREAD_READ_FILE        = 320,
    CREAD_READ_DONE        = 321,
    CREAD_READY            = 322,
    CREAD_NO_DATA          = 330,  /* "No saved data found." */
    CREAD_WAIT_NO_DATA     = 331 
};

/* Task: read the save file into memory and check it, for the Load screen.
 * g_cardTaskState: 1 = read, 0 = no card, no file or a bad one. Returns 1 when done (0x80033724) */
/* 0x80033724 */
u32 Card_ReadSavesTask(u16 *step)
{
    int result;
    int block;
    int i;
    MsgWork *msg;

    switch (*step)
    {
    case CREAD_CHECKING:
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,0,"%CChecking Memory Card\201c%KPlease do not remove Memory Card.%E");
        break;
    case CREAD_WAIT_CHECKING:
        Task_GetWork(g_uiTaskIds[0],&msg);
        if (g_textWins[msg->win].state != TEXTWIN_SHOWN)
        {
            return 0;
        }
        break;
    case CREAD_CHECK:
        Card_EnableEvents();
        Card_Check();
        break;
    case CREAD_STATUS:
        result = g_cardStatus;
        if (result == 0)
        {
            *step = CREAD_OPEN_FILE;
            return 0;
        }
        if ((result < 1) || (3 < result))
        {
            return 0;
        }
        Msg_Close();
        Card_DisableEvents();
        if (result == 1)
        {
            *step = CREAD_LOAD_FAILED;
        }
        else if (result == 2)
        {
            *step = CREAD_NO_CARD;
        }
        else
        {
            *step = CREAD_NO_DATA;      /* not formatted: no saves */
        }
        return 0;
    case CREAD_NO_CARD:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        Msg_AskYesNoAt(0,0,2,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,
                       "%CNo Memory Card detected.%KPlease insert Memory Card.%KYes%KQuit%E");
        break;
    case CREAD_NO_CARD_ANSWER:
        if (Msg_IsAnswered() == 0)
        {
            return 0;
        }
        if (g_msgAnswer == 0)
        {
            *step = CREAD_CHECKING;
            return 0;
        }
        g_cardTaskState = CARDTASK_CANCELED;
        return 1;
    case CREAD_LOAD_FAILED:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,MSG_ANY_BUTTON,"%CLoad failed.%E");
        break;
    case CREAD_WAIT_LOAD_FAILED:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        g_cardTaskState = CARDTASK_CANCELED;
        return 1;
    case CREAD_OPEN_FILE:
        result = Card_OpenFile("BASLUS-01333TOPSHOP",1);
        if (result == 0)
        {
            *step = CREAD_READ_FILE;
            return 0;
        }
        if ((result == 1) || (result == 2))
        {
            Msg_Close();
            Card_DisableEvents();
            *step = result == 1 ? CREAD_LOAD_FAILED : CREAD_NO_DATA;
        }
        return 0;
    case CREAD_READ_FILE:
        /* the save buffer mirrors the file: system block at 0x200, the slots at k_saveSlotOffsets */
        block = SecHeap_Alloc(8);
        g_saveBuf = g_secHeapBlocks[(short)block].addr;
        g_cardBufBlock = block;
        g_cardBuf = (CardSystem *)(g_saveBuf + 0x200);
        g_saveSlots[0] = g_saveBuf + 0x280;
        g_saveSlots[1] = g_saveBuf + 0xc00;
        g_saveSlots[2] = g_saveBuf + 0x1580;
        g_saveSlots[3] = g_saveBuf + 0x1f00;
        if ((Card_ReadFile((u8 *)g_cardBuf,0x200,0x80) != 0) ||
            (g_cardBuf->checksum != Save_Checksum((u8 *)g_cardBuf,0x1a)))
        {
            Card_CloseFile();
            Msg_Close();
            Card_DisableEvents();
            Save_FreeBuffer();
            *step = CREAD_LOAD_FAILED;
            return 0;
        }
        for (i = 0; i < 4; i++)
        {
            if ((g_cardBuf->slotUsed[i] != 0) &&
                ((Card_ReadFile(g_saveSlots[i],k_saveSlotOffsets[i],0x980) != 0) ||
                 (((SaveImage *)g_saveSlots[i])->checksum != Save_Checksum(g_saveSlots[i],0x974))))
            {
                Card_CloseFile();
                Msg_Close();
                Card_DisableEvents();
                Save_FreeBuffer();
                *step = CREAD_LOAD_FAILED;
                return 0;
            }
        }
        break;
    case CREAD_READ_DONE:
        Card_DisableEvents();
        Card_CloseFile();
        Msg_Close();
        break;
    case CREAD_READY:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        g_cardTaskState = CARDTASK_READY;
        return 1;
    case CREAD_NO_DATA:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,MSG_ANY_BUTTON,"%CNo saved data found.%E");
        g_cardTaskState = CARDTASK_CANCELED;
        break;
    case CREAD_WAIT_NO_DATA:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        return 1;
    default:
        return 0;
    }
    *step += 1;
    return 0;
}

/* Card_LoadSystemTask steps */
enum CardLoadSystemStep
{
    CLSYS_CHECKING         =   0,  /* "Checking Memory Card... Please do not remove Memory Card." */
    CLSYS_WAIT_CHECKING    =   1,
    CLSYS_CHECK            =   2,  /* Card_Check */
    CLSYS_STATUS           =   3,  /* on g_cardStatus */
    CLSYS_NO_CARD          = 100,  /* "No Memory Card detected." */
    CLSYS_WAIT_NO_CARD     = 101,
    CLSYS_LOAD_FAILED      = 220,  /* "Load failed." */
    CLSYS_WAIT_LOAD_FAILED = 221,
    CLSYS_OPEN_FILE        = 300,
    CLSYS_WAIT_CLOSE       = 320,
    CLSYS_LOADING          = 321,  /* "System Data loading... Please do not remove Memory Card." */
    CLSYS_WAIT_LOADING     = 322,
    CLSYS_LOADING_1        = 323,  /* (a frame) */
    CLSYS_LOADING_2        = 324,  /* (a frame) */
    CLSYS_READ             = 325,  /* the progress and the options */
    CLSYS_SHOWN            = 326,  /* 46 frames or a button */
    CLSYS_DONE             = 327,
    CLSYS_NO_DATA          = 400,  /* "No saved data found." */
    CLSYS_WAIT_NO_DATA     = 401 
};

/* Task, at new-game setup: read the progress and the options from the save file's system block.
 * g_cardTaskState = 1 when read. Returns 1 when done (0x80033CEC) */
/* 0x80033CEC */
u32 Card_LoadSystemTask(u16 *step, CardLoadSystemWork *work)
{
    int result;
    int block;
    MsgWork *msg;

    switch (*step)
    {
    case CLSYS_CHECKING:
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,0,"%CChecking Memory Card\201c%KPlease do not remove Memory Card.%E");
        break;
    case CLSYS_WAIT_CHECKING:
    case CLSYS_WAIT_LOADING:
        Task_GetWork(g_uiTaskIds[0],&msg);
        if (g_textWins[msg->win].state != TEXTWIN_SHOWN)
        {
            return 0;
        }
        break;
    case CLSYS_CHECK:
        Card_EnableEvents();
        Card_Check();
        break;
    case CLSYS_STATUS:
        result = g_cardStatus;
        if ((result == 1) || (result == 2))
        {
            Card_DisableEvents();
            Msg_Close();
            *step = CLSYS_NO_CARD;
        }
        else if (result == 3)
        {
            Card_DisableEvents();
            Msg_Close();
            *step = CLSYS_NO_DATA;        /* not formatted: no data */
        }
        else
        {
            *step = CLSYS_OPEN_FILE;
        }
        return 0;
    case CLSYS_NO_CARD:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,300,MSG_ANY_BUTTON,"%CNo Memory Card detected.%E");
        break;
    case CLSYS_WAIT_NO_CARD:
    case CLSYS_WAIT_LOAD_FAILED:
    case CLSYS_WAIT_NO_DATA:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        return 1;
    case CLSYS_LOAD_FAILED:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,MSG_ANY_BUTTON,"%CLoad failed.%E");
        break;
    case CLSYS_OPEN_FILE:
        result = Card_OpenFile("BASLUS-01333TOPSHOP",1);
        if (result == 0)
        {
            Msg_Close();
            *step = CLSYS_WAIT_CLOSE;
        }
        else if (result == 1)
        {
            Card_DisableEvents();
            Msg_Close();
            *step = CLSYS_LOAD_FAILED;
        }
        else if (result == 2)
        {
            Msg_Close();
            Card_DisableEvents();
            *step = CLSYS_NO_DATA;        /* no save file */
        }
        return 0;
    case CLSYS_WAIT_CLOSE:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        break;
    case CLSYS_LOADING:
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,0,"%CSystem Data loading\201c%KPlease do not remove Memory Card.%E");
        break;
    case CLSYS_LOADING_1:
    case CLSYS_LOADING_2:
        break;
    case CLSYS_READ:
        block = SecHeap_Alloc(8);
        g_cardBufBlock = block;
        g_cardBuf = (CardSystem *)((u8 *)g_secHeapBlocks[(short)block].addr + 0x200);
        if (Card_ReadFile((u8 *)g_cardBuf,0x200,0x80) != 0)
        {
            Card_CloseFile();
            Msg_Close();
            Card_DisableEvents();
        }
        else
        {
            Card_CloseFile();
            Card_DisableEvents();
            if (g_cardBuf->checksum == Save_Checksum((u8 *)g_cardBuf,0x1a))
            {
                g_progress = g_cardBuf->progress;
                g_optEventScenes = g_cardBuf->eventScenes;
                g_optMono = g_cardBuf->mono;
                g_optSkipCpuTalk = g_cardBuf->skipCpuTalk;
                g_optCpuFast = g_cardBuf->cpuFast;
                Snd_SetMono(g_optMono);
                Save_FreeBuffer();
                work->retries = 0;
                break;
            }
            Msg_Close();
        }
        Save_FreeBuffer();
        *step = CLSYS_LOAD_FAILED;
        return 0;
    case CLSYS_SHOWN:
        /* "System data load complete." stays up for 46 frames or until a button */
        work->retries++;
        if ((work->retries < 0x2e) && (g_playerPads[6].pressed == 0))
        {
            return 0;
        }
        Msg_Close();
        break;
    case CLSYS_DONE:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        g_cardTaskState = CARDTASK_READY;
        return 1;
    case CLSYS_NO_DATA:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,300,MSG_ANY_BUTTON,"%CNo saved data found.%E");
        break;
    default:
        return 0;
    }
    *step += 1;
    return 0;
}

/* The save screen's slot list: per slot "New" or what it holds (mode, stage, year, month) as a
 * text in g_menuRowItems; empty[] = 1 for the free slots (0x800342F8) */
/* 0x800342F8 */
void Save_DescribeCurrent(u8 *empty)
{
    SaveImage *img;
    char *text;
    u8 stage;
    int year;
    int month;
    int playMode;
    int i;

    for (i = 0; i < 4; i++)
    {
        if (g_cardBuf->slotUsed[i] == 0)
        {
            empty[i] = 1;
            text = "             New            ";
        }
        else
        {
            Save_GetSlotInfo(&img,&stage,&year,&month,&playMode,i);
            empty[i] = 0;
            if (playMode == PLAY_STAGE)
            {
                memcpy(g_msgBuf, "Story Mode   ", 14);
            }
            else
            {
                memcpy(g_msgBuf, "Free Play    ", 14);
            }
            Str_FormatInt(stage + 1,2,g_msgBuf + strlen(g_msgBuf));
            strcat(g_msgBuf,"Stage ");
            Str_FormatInt(year + 1,2,g_msgBuf + strlen(g_msgBuf));
            strcat(g_msgBuf,"Year");
            Str_FormatInt((month + 3U) % 12 + 1,2,g_msgBuf + strlen(g_msgBuf));
            strcat(g_msgBuf,"month");
            text = g_msgBuf;
        }
        g_menuRowItems[i] = Text_Create(text);
    }
}

/* The load screen's slot list: per slot what it holds (mode, stage, year, month) or "No data."
 * as a text in g_menuRowItems; used[] = 1 for the slots with a save, *count of them (0x80034524) */
/* 0x80034524 */
void Save_DescribeSlot(u8 *used, char *count)
{
    SaveImage *img;
    u8 stage;
    int year;
    int month;
    int playMode;
    int i;

    for (i = 0; i < 4; i++)
    {
        if (g_cardBuf->slotUsed[i] == 0)
        {
            used[i] = 0;
            g_menuRowItems[i] = Text_Create("No data.");
        }
        else
        {
            Save_GetSlotInfo(&img,&stage,&year,&month,&playMode,i);
            used[i] = 1;
            if (playMode == PLAY_STAGE)
            {
                memcpy(g_msgBuf, "Story Mode   ", 14);
            }
            else
            {
                memcpy(g_msgBuf, "Free Play    ", 14);
            }
            Str_FormatInt(stage + 1,2,g_msgBuf + strlen(g_msgBuf));
            strcat(g_msgBuf,"Stage ");
            Str_FormatInt(year + 1,2,g_msgBuf + strlen(g_msgBuf));
            strcat(g_msgBuf,"Year");
            Str_FormatInt((month + 3U) % 12 + 1,2,g_msgBuf + strlen(g_msgBuf));
            strcat(g_msgBuf,"month");
            g_menuRowItems[i] = Text_Create(g_msgBuf);
            *count += 1;
        }
    }
}

/* Pack the board game (config, players, stores, store types) into save slot slot and write it
 * (Save_WriteSlot); 0 when saved, -1 when not (0x80034774) */
/* 0x80034774 */
int Save_Pack(u8 slot, char player)
{
    SaveImage *img;
    SavePlayer *sp;
    SaveStore *ss;
    Player *pl;
    Store *st;
    int i;
    int j;

    img = (SaveImage *)g_saveSlots[(char)slot];
    strcpy(img->title,g_players[player].title);
    img->playMode = g_gameConfig.playMode;
    img->unused01 = g_gameConfig.unused01;
    img->numPlayers = g_gameConfig.numPlayers;
    img->numHumans = g_gameConfig.numHumans;
    img->numCpus = g_gameConfig.numCpus;
    img->year = g_gameConfig.year;
    img->month = g_gameConfig.month;
    img->stage = g_gameConfig.stage;
    img->bankruptLimit = g_gameConfig.bankruptLimit;
    img->startGoods = g_gameConfig.startGoods;
    img->maxBuy = g_gameConfig.maxBuy;
    img->monthsElapsed = g_gameConfig.monthsElapsed;
    img->startMoney = g_gameConfig.startMoney;
    img->goalMoney = g_gameConfig.goalMoney;
    for (i = 0; i < GAME_MAX_PLAYERS; i++)
    {
        img->turnOrder[i] = g_gameConfig.turnOrder[i];
    }
    for (i = 0; i < BOARD_MAX_PLAYERS; i++)
    {
        pl = &g_players[i];
        sp = &img->players[i];
        sp->isCpu = pl->isCpu;
        sp->pad = pl->pad;
        sp->charId = pl->charId;
        sp->out = pl->out;
        strcpy(sp->name,pl->name);
        strcpy(sp->title,pl->title);
        sp->x = pl->x;
        sp->floor = pl->floor;
        sp->dirX = pl->dirX;
        sp->dirFloor = pl->dirFloor;
        sp->noTargeting = pl->noTargeting;
        sp->paydayEffect = pl->paydayEffect;
        sp->diceEffect = pl->diceEffect;
        sp->noElevator = pl->noElevator;
        sp->noDelivery = pl->noDelivery;
        sp->vandalized = pl->vandalized;
        sp->exempt = pl->exempt;
        sp->historyLen = pl->historyLen;
        sp->points = pl->points;
        sp->shoes = pl->shoes;
        sp->screenX = pl->screenX;
        sp->screenY = pl->screenY;
        sp->rankKey = pl->rankKey;
        sp->customers = pl->customers;
        sp->purchases = pl->purchases;
        sp->paydays = pl->paydays;
        sp->stepsSincePayday = pl->stepsSincePayday;
        sp->diceEffectUntil = pl->diceEffectUntil;
        sp->noElevatorUntil = pl->noElevatorUntil;
        sp->noDeliveryUntil = pl->noDeliveryUntil;
        sp->vandalizedUntil = pl->vandalizedUntil;
        sp->totalSteps = pl->totalSteps;
        sp->money = pl->money;
        sp->assets = pl->assets;
        sp->pieceState = pl->pieceState;
        for (j = 0; j < 13; j++)
        {
            sp->assetHistory[j] = pl->assetHistory[j];
        }
    }
    for (i = 0; i < MAX_STORES; i++)
    {
        st = &g_stores[i];
        ss = &img->stores[i];
        ss->owner = st->owner;
        ss->storeType = st->storeType;
        ss->tempClosed = st->tempClosed;
        ss->numGoods = st->numGoods;
        ss->maxGoods = st->maxGoods;
        ss->priceChanged = st->priceChanged;
        ss->pricePercent = st->pricePercent;
        ss->priceUntil = st->priceUntil;
        ss->closedUntil = st->closedUntil;
        for (j = 0; j < 8; j++)
        {
            ss->goods[j][0] = st->goods[j].active;
            ss->goods[j][1] = st->goods[j].level;
        }
    }
    for (i = 0; i < NUM_STORE_TYPES; i++)
    {
        img->storeTypes[i].available = g_storeTypes[i].available;
        img->storeTypes[i].store = g_storeTypes[i].store;
    }
    img->boardStage = g_boardStage;
    img->boardPlayers = g_numPlayers;
    img->numActivePlayers = g_numActivePlayers;
    img->curPlayer = g_curPlayer;
    img->checksum = Save_Checksum((u8 *)img,0x974);    /* everything before the checksum */
    if ((short)Save_WriteSlot((char)slot) < 0)
    {
        return -1;
    }
    return 0;
}

/* Write the save buffer (progress, options, slot) to the save file: 0 when written, -1 on a
 * card error, 2 when the card holds something else (0x80034C70) */
/* 0x80034C70 */
int Save_WriteSlot(short slot)
{
    int result;
    int i;

    Card_EnableEvents();
    Card_Check();
    if (g_cardStatus == 0)
    {
        result = Card_OpenFile("BASLUS-01333TOPSHOP",1);
        if (result != 1)
        {
            if (result < 0)
            {
                /* The original returns whatever is in v0 here: 0x80100000, the high half of an address.
                 * Save_Pack only looks at the low 16 bits, so this counts as written. */
                return 0x80100000;
            }
            if (result >= 2)
            {
                if (result != 2)
                {
                    return 2;
                }
                if ((short)Save_CreateFile() == -1)
                {
                    Card_DisableEvents();
                    return -1;
                }
            }
            if (Card_WriteFile(g_saveSlots[slot],k_saveSlotOffsets[slot],0x980) != 0)
            {
                Card_CloseFile();
                Card_DisableEvents();
                g_cardBuf->slotUsed[slot] = 0;
                return -1;
            }
            /* the system block: slots in use, options, and the best of the card's progress and this
             * game's */
            g_cardBuf->slotUsed[slot] = 1;
            g_cardBuf->eventScenes = g_optEventScenes;
            g_cardBuf->mono = g_optMono;
            g_cardBuf->skipCpuTalk = g_optSkipCpuTalk;
            g_cardBuf->cpuFast = g_optCpuFast;
            if (g_cardBuf->progress.storyStage < g_progress.storyStage)
            {
                g_cardBuf->progress.storyStage = g_progress.storyStage;
            }
            if (g_cardBuf->progress.freeStages < g_progress.freeStages)
            {
                g_cardBuf->progress.freeStages = g_progress.freeStages;
            }
            if (g_cardBuf->progress.clears < g_progress.clears)
            {
                g_cardBuf->progress.clears = g_progress.clears;
            }
            for (i = 0; i < 11; i++)
            {
                g_cardBuf->progress.charOpen[i] = 1;
            }
            if (g_progress.charOpen[11] == 1)
            {
                g_cardBuf->progress.charOpen[11] = 1;
            }
            if (g_progress.charOpen[12] == 1)
            {
                g_cardBuf->progress.charOpen[12] = 1;
            }
            if (g_progress.charOpen[13] == 1)
            {
                g_cardBuf->progress.charOpen[13] = 1;
            }
            g_cardBuf->checksum = Save_Checksum((u8 *)g_cardBuf,0x1a);
            if (Card_WriteFile((u8 *)g_cardBuf,0x200,0x80) != 0)
            {
                Card_CloseFile();
                Card_DisableEvents();
                g_cardBuf->slotUsed[slot] = 0;
                return -1;
            }
            Card_CloseFile();
            Card_DisableEvents();
            return 0;
        }
    }
    Card_DisableEvents();
    return -1;
}

/* Create the save file BASLUS-01333TOPSHOP on the memory card: 0, or -1 when it failed
 * (0x80034FC4) */
/* 0x80034FC4 */
int Save_CreateFile(void)
{
    /* the standard card file header: "SC", icon flag, blocks, title, icon palette and frames */
    memset(g_saveBuf,0,0x80);
    g_saveBuf[0] = k_saveMagic[0];
    g_saveBuf[1] = k_saveMagic[1];
    g_saveBuf[2] = k_saveIconFlag;
    g_saveBuf[2] = 0x13;                /* three icon frames (overrides the line above) */
    g_saveBuf[3] = 2;                   /* blocks used */
    memcpy(g_saveBuf + 4,k_saveTitle,0x11);
    Mem_Copy(g_saveBuf + 0x60,k_saveIconClut,0x20);
    Mem_Copy(g_saveBuf + 0x80,k_saveIconFrame0,0x80);
    Mem_Copy(g_saveBuf + 0x100,k_saveIconFrame1,0x80);
    Mem_Copy(g_saveBuf + 0x180,k_saveIconFrame2,0x80);
    Card_CreateFile("BASLUS-01333TOPSHOP",2,g_saveBuf);
    if (g_cardCreateResult == 0)
    {
        return 0;
    }
    return -1;
}

/* Restore the board game from the loaded save record (inverse of Save_Pack) (0x80035108) */
/* 0x80035108 */
int Save_Unpack(void)
{
    SaveImage *img;
    SavePlayer *sp;
    SaveStore *ss;
    Player *pl;
    Store *st;
    int i;
    int j;

    img = &g_saveImage;
    g_gameConfig.playMode = img->playMode;
    g_gameConfig.unused01 = img->unused01;
    g_gameConfig.numPlayers = img->numPlayers;
    g_gameConfig.numHumans = img->numHumans;
    g_gameConfig.numCpus = img->numCpus;
    g_gameConfig.year = img->year;
    g_gameConfig.month = img->month;
    g_gameConfig.stage = img->stage;
    g_gameConfig.bankruptLimit = img->bankruptLimit;
    g_gameConfig.startGoods = img->startGoods;
    g_gameConfig.maxBuy = img->maxBuy;
    g_gameConfig.monthsElapsed = img->monthsElapsed;
    g_gameConfig.startMoney = img->startMoney;
    g_gameConfig.goalMoney = img->goalMoney;
    for (i = 0; i < GAME_MAX_PLAYERS; i++)
    {
        g_gameConfig.turnOrder[i] = img->turnOrder[i];
    }
    for (i = 0; i < BOARD_MAX_PLAYERS; i++)
    {
        pl = &g_players[i];
        sp = &img->players[i];
        pl->isCpu = sp->isCpu;
        pl->pad = sp->pad;
        pl->charId = sp->charId;
        pl->out = sp->out;
        strcpy(pl->name,sp->name);
        strcpy(pl->title,sp->title);
        pl->x = sp->x;
        pl->floor = sp->floor;
        pl->dirX = sp->dirX;
        pl->dirFloor = sp->dirFloor;
        pl->noTargeting = sp->noTargeting;
        pl->paydayEffect = sp->paydayEffect;
        pl->diceEffect = sp->diceEffect;
        pl->noElevator = sp->noElevator;
        pl->noDelivery = sp->noDelivery;
        pl->vandalized = sp->vandalized;
        pl->exempt = sp->exempt;
        pl->historyLen = sp->historyLen;
        pl->points = sp->points;
        pl->shoes = sp->shoes;
        pl->screenX = sp->screenX;
        pl->screenY = sp->screenY;
        pl->rankKey = sp->rankKey;
        pl->customers = sp->customers;
        pl->purchases = sp->purchases;
        pl->paydays = sp->paydays;
        pl->stepsSincePayday = sp->stepsSincePayday;
        pl->diceEffectUntil = sp->diceEffectUntil;
        pl->noElevatorUntil = sp->noElevatorUntil;
        pl->noDeliveryUntil = sp->noDeliveryUntil;
        pl->vandalizedUntil = sp->vandalizedUntil;
        pl->totalSteps = sp->totalSteps;
        pl->money = sp->money;
        pl->assets = sp->assets;
        pl->pieceState = sp->pieceState;
        for (j = 0; j < 13; j++)
        {
            pl->assetHistory[j] = sp->assetHistory[j];
        }
    }
    for (i = 0; i < MAX_STORES; i++)
    {
        st = &g_stores[i];
        ss = &img->stores[i];
        st->owner = ss->owner;
        st->storeType = ss->storeType;
        st->tempClosed = ss->tempClosed;
        st->numGoods = ss->numGoods;
        st->maxGoods = ss->maxGoods;
        st->priceChanged = ss->priceChanged;
        st->pricePercent = ss->pricePercent;
        st->priceUntil = ss->priceUntil;
        st->closedUntil = ss->closedUntil;
        for (j = 0; j < 8; j++)
        {
            st->goods[j].active = ss->goods[j][0];
            st->goods[j].level = ss->goods[j][1];
        }
        if (st->owner == -1)
        {
            st->type = NULL;
        }
        else
        {
            st->type = &g_storeTypes[(char)st->storeType];
        }
    }
    for (i = 0; i < NUM_STORE_TYPES; i++)
    {
        g_storeTypes[i].available = img->storeTypes[i].available;
        g_storeTypes[i].store = img->storeTypes[i].store;
    }
    g_boardStage = img->boardStage;
    g_numPlayers = img->boardPlayers;
    g_numActivePlayers = img->numActivePlayers;
    g_curPlayer = img->curPlayer;
    return 0;
}

/* Count a story stage clear in g_progress (GameConfig.stage is already the next stage): how far
 * story mode got, the stages and characters that unlocks, the clear count. Inline twice in
 * Card_SaveProgressTask in the original. */
static void Progress_RecordClear(void)
{
    if (g_progress.storyStage < (char)g_gameConfig.stage)
    {
        g_progress.storyStage = g_gameConfig.stage;
        if (g_gameConfig.stage == 9)
        {
            if (g_progress.freeStages < 9)
            {
                g_progress.freeStages = 9;
            }
            g_progress.charOpen[11] = 1;
        }
        else if (g_gameConfig.stage == 10)
        {
            if (g_progress.freeStages < 10)
            {
                g_progress.freeStages = 10;
            }
            g_progress.charOpen[12] = 1;
        }
    }
    if ((15 <= g_progress.clears) || (15 <= ++g_progress.clears))
    {
        if (g_progress.storyStage == 10)
        {
            g_progress.charOpen[13] = 1;
        }
    }
}

/* Card_SaveProgressTask steps */
enum CardSaveProgressStep
{
    CPROG_RECORD               =   0,  /* count the stage clear in g_progress */
    CPROG_ASK                  =  10,  /* "Save?" */
    CPROG_ANSWER               =  11,
    CPROG_CHECKING             = 100,  /* "Checking Memory Card..." */
    CPROG_WAIT_CHECKING        = 101,
    CPROG_EVENTS               = 102,
    CPROG_CHECK                = 103,  /* Card_Check */
    CPROG_NO_CARD_WAIT         = 110,  /* then ask for a card */
    CPROG_NO_CARD              = 200,  /* "No Memory Card detected. Please insert Memory Card." Yes / Quit */
    CPROG_NO_CARD_ANSWER       = 201,
    CPROG_RECHECKING           = 210,  /* "Checking Memory Card..." */
    CPROG_WAIT_RECHECKING      = 211,
    CPROG_RECHECK_1            = 212,  /* (a frame) */
    CPROG_RECHECK_2            = 213,  /* (a frame) */
    CPROG_RECHECK_EVENTS       = 214,
    CPROG_RECHECK              = 215,  /* Card_Check */
    CPROG_LOAD_FAILED          = 220,  /* "Load failed." */
    CPROG_WAIT_LOAD_FAILED     = 221,
    CPROG_STILL_NO_CARD        = 225,  /* "No Memory Card detected." */
    CPROG_WAIT_STILL_NO_CARD   = 226,  /* then "Save?" again */
    CPROG_NOT_FORMATTED        = 230,
    CPROG_NOT_FORMATTED_MSG    = 231,  /* "Memory Card is not formatted." */
    CPROG_ASK_FORMAT           = 232,  /* "Do you wish to format?" */
    CPROG_FORMAT_ANSWER        = 233,
    CPROG_FORMATTING           = 234,  /* "Formatting... Please do not remove Memory Card." */
    CPROG_WAIT_FORMATTING      = 235,
    CPROG_FORMAT_CHECK         = 236,
    CPROG_FORMAT               = 237,  /* Card_Format */
    CPROG_FORMAT_DONE          = 238,
    CPROG_FORMAT_FAILED        = 240,  /* "Format failed." */
    CPROG_WAIT_FORMAT_FAILED   = 241,
    CPROG_FORMAT_COMPLETE      = 250,  /* "Format complet." (sic) */
    CPROG_WAIT_FORMAT_COMPLETE = 251,
    CPROG_OPEN_FILE            = 300,
    CPROG_NO_FILE              = 310,  /* no save file yet: room for its 2 blocks? */
    CPROG_NO_SPACE_MSG         = 311,  /* "Not enough space on Memory Card." */
    CPROG_WAIT_NO_SPACE        = 312,
    CPROG_OVERWRITE            = 320,  /* "Overwrite?" (once) */
    CPROG_OVERWRITE_ANSWER     = 321,
    CPROG_READ_SYSTEM          = 322,  /* the card keeps the best of its progress and this game's */
    CPROG_SAVING               = 323,  /* "Saving... Please do not remove Memoy Card." (sic) */
    CPROG_WAIT_SAVING          = 324,
    CPROG_REMOVED              = 325,  /* "Memory Card was removed." */
    CPROG_WAIT_REMOVED         = 326,
    CPROG_NEW_FILE_SAVING      = 330,  /* a new file: "Saving..." */
    CPROG_WAIT_NEW_FILE_SAVING = 331,
    CPROG_CREATE_FILE          = 332,
    CPROG_NEW_SLOTS            = 333,
    CPROG_PACK                 = 340,  /* the progress and the options into the system block */
    CPROG_WRITE                = 341,
    CPROG_SAVED                = 350,  /* "Save complete." */
    CPROG_WAIT_SAVED           = 351,
    CPROG_SAVE_FAILED          = 360,  /* "Save failed." */
    CPROG_WAIT_SAVE_FAILED     = 361,
    CPROG_DONE                 = 400 
};

/* Task after a story stage clear (GameConfig.stage already advanced): count the clear in
 * g_progress, then offer to save the progress to the memory card's system block, with the card
 * checks, formatting and "Overwrite?" prompts. Returns 1 when done (0x8003558C) */
/* 0x8003558C */
u32 Card_SaveProgressTask(short *step)
{
    int result;
    int block;
    int i;
    MsgWork *msg;

    FntPrint("twsave : %d\n", (u16)*step);
    switch (*step)
    {
    case CPROG_RECORD:
        g_gameConfig.stage++;
        Progress_RecordClear();
        *step = CPROG_ASK;
        return 0;
    case CPROG_ASK:
        g_saveConfirmed = 0;
        Msg_AskYesNo(0,0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,"Save?%KYes%KNo%E");
        break;
    case CPROG_ANSWER:
        if (Msg_IsAnswered() == 0)
        {
            return 0;
        }
        *step = g_msgAnswer != 0 ? CPROG_DONE : CPROG_CHECKING;
        return 0;
    case CPROG_CHECKING:
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,0,"%CChecking Memory Card\201c%KPlease do not remove Memory Card.%E");
        break;
    case CPROG_WAIT_CHECKING:
    case CPROG_WAIT_RECHECKING:
    case CPROG_WAIT_FORMATTING:
    case CPROG_WAIT_NEW_FILE_SAVING:
        Task_GetWork(g_uiTaskIds[0],&msg);
        if (g_textWins[msg->win].state != TEXTWIN_SHOWN)
        {
            return 0;
        }
        break;
    case CPROG_EVENTS:
    case CPROG_RECHECK_EVENTS:
        Card_EnableEvents();
        break;
    case CPROG_CHECK:
        result = Card_Check();
        if (result == 0)
        {
            *step = CPROG_OPEN_FILE;
        }
        else if (result == 1)
        {
            Msg_Close();
            Card_DisableEvents();
            *step = CPROG_LOAD_FAILED;
        }
        else if (result == 2)
        {
            Msg_Close();
            Card_DisableEvents();
            *step = CPROG_NO_CARD_WAIT;
        }
        else if (result == 3)
        {
            *step = CPROG_NOT_FORMATTED;
        }
        /* fall through, as in the original: once the message is done this asks for a card */
    case CPROG_NO_CARD_WAIT:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        *step = CPROG_NO_CARD;
        return 0;

    /* no card: insert one or quit */
    case CPROG_NO_CARD:
        Msg_AskYesNoAt(0,0,2,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,
                       "%CNo Memory Card detected.%KPlease insert Memory Card.%KYes%KQuit%E");
        break;
    case CPROG_NO_CARD_ANSWER:
        if (Msg_IsAnswered() == 0)
        {
            return 0;
        }
        if (g_msgAnswer == 0)
        {
            *step = CPROG_RECHECKING;
            return 0;
        }
        *step = CPROG_DONE;
        return 1;
    case CPROG_RECHECKING:
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,0,"Checking Memory Card\201c%KPlease do not remove Memory Card.%E");
        break;
    case CPROG_RECHECK_1:
    case CPROG_RECHECK_2:
        break;
    case CPROG_RECHECK:
        result = Card_Check();
        if (result == 0)
        {
            *step = CPROG_OPEN_FILE;
        }
        else if ((result == 1) || (result == 2))
        {
            Msg_Close();
            Card_DisableEvents();
            *step = CPROG_STILL_NO_CARD;
        }
        else if (result == 3)
        {
            *step = CPROG_NOT_FORMATTED;
        }
        return 0;

    /* failures: say so and go back to "Save?" */
    case CPROG_LOAD_FAILED:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,MSG_ANY_BUTTON,"%CLoad failed.%E");
        break;
    case CPROG_WAIT_LOAD_FAILED:
    case CPROG_WAIT_STILL_NO_CARD:
    case CPROG_WAIT_NO_SPACE:
    case CPROG_WAIT_REMOVED:
    case CPROG_WAIT_SAVE_FAILED:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        *step = CPROG_ASK;
        return 0;
    case CPROG_STILL_NO_CARD:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,MSG_ANY_BUTTON,"%CNo Memory Card detected.%E");
        break;

    /* not formatted: format it? */
    case CPROG_NOT_FORMATTED:
        Card_DisableEvents();
        Msg_Close();
        break;
    case CPROG_NOT_FORMATTED_MSG:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0x3c,MSG_ANY_BUTTON,"%CMemory Card is not formatted.%E");
        break;
    case CPROG_ASK_FORMAT:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        Msg_AskYesNo(0,0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,"%CDo you wish to format?%KYes%KNo%E");
        Task_GetWork(g_uiTaskIds[0],&msg);
        TextWin_SetMenu(msg->win,1,2,1);
        break;
    case CPROG_FORMAT_ANSWER:
        Card_EnableEvents();
        result = Card_Info(0);
        Card_DisableEvents();
        if ((result != 0) && (result != 3))
        {
            Msg_CloseChoice();
            *step = CPROG_REMOVED;
            return 0;
        }
        if (Msg_IsAnswered() == 0)
        {
            return 0;
        }
        if (g_msgAnswer != 0)
        {
            *step = CPROG_ASK;
            return 0;
        }
        break;
    case CPROG_FORMATTING:
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,0,"%CFormatting\201c%KPlease do not remove Memory Card.%E");
        break;
    case CPROG_FORMAT_CHECK:
        Card_EnableEvents();
        Card_Check();
        result = g_cardStatus;
        if ((result == 1) || (result == 2))
        {
            Msg_Close();
            g_cardFormatResult = 1;
            *step = CPROG_FORMAT_DONE;
            return 0;
        }
        if ((result != 0) && (result != 3))
        {
            return 0;
        }
        break;
    case CPROG_FORMAT:
        Card_Format();
        Msg_Close();
        break;
    case CPROG_FORMAT_DONE:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        if (g_cardFormatResult == 0)
        {
            *step = CPROG_FORMAT_COMPLETE;
        }
        else if (g_cardFormatResult == 1)
        {
            *step = CPROG_FORMAT_FAILED;
        }
        return 0;
    case CPROG_FORMAT_FAILED:
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,MSG_ANY_BUTTON,"%CFormat failed.%E");
        break;
    case CPROG_WAIT_FORMAT_FAILED:
        Card_EnableEvents();
        result = Card_Info(0);
        Card_DisableEvents();
        if ((result != 0) && (result != 3))
        {
            Msg_CloseChoice();
            *step = CPROG_REMOVED;
            return 0;
        }
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        Card_DisableEvents();
        *step = CPROG_ASK;
        return 0;
    case CPROG_FORMAT_COMPLETE:
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,MSG_ANY_BUTTON,"%CFormat complet.%E");
        break;
    case CPROG_WAIT_FORMAT_COMPLETE:
        Card_EnableEvents();
        result = Card_Info(0);
        Card_DisableEvents();
        if ((result != 0) && (result != 3))
        {
            Msg_Close();
            *step = CPROG_REMOVED;
            return 0;
        }
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        *step = CPROG_CHECKING;
        return 0;

    /* the save file */
    case CPROG_OPEN_FILE:
        result = Card_OpenFile("BASLUS-01333TOPSHOP",1);
        if (result == 0)
        {
            Msg_Close();
            Card_DisableEvents();
            *step = CPROG_OVERWRITE;
        }
        else if (result == 1)
        {
            Msg_Close();
            Card_DisableEvents();
            *step = CPROG_LOAD_FAILED;
        }
        else if (result == 2)
        {
            *step = CPROG_NO_FILE;      /* no save file yet */
        }
        return 0;
    case CPROG_NO_FILE:
        if (1 < 15 - g_cardUsedBlocks)
        {
            Msg_Close();
            *step = CPROG_NEW_FILE_SAVING;  /* room for the file's 2 blocks */
            return 0;
        }
        Card_DisableEvents();
        Msg_Close();
        break;
    case CPROG_NO_SPACE_MSG:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,MSG_ANY_BUTTON,"%CNot enough space on Memory Card.%E");
        break;
    case CPROG_OVERWRITE:
        if (g_saveConfirmed == 1)
        {
            *step = CPROG_READ_SYSTEM;
        }
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        Msg_AskYesNo(0,0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,"%COverwrite?%KYes%KNo%E");
        break;
    case CPROG_OVERWRITE_ANSWER:
        Card_EnableEvents();
        result = Card_Info(0);
        Card_DisableEvents();
        if ((result != 0) && (result != 3))
        {
            Msg_CloseChoice();
            *step = CPROG_REMOVED;
            return 0;
        }
        if (Msg_IsAnswered() == 0)
        {
            return 0;
        }
        if (g_msgAnswer == 0)
        {
            g_saveConfirmed = 1;
            *step = CPROG_CHECKING;
            return 0;
        }
        *step = CPROG_ASK;
        return 0;
    case CPROG_READ_SYSTEM:
        /* read the system block; the card keeps the best of its progress and this game's */
        block = SecHeap_Alloc(8);
        g_cardBufBlock = block;
        g_cardBuf = (CardSystem *)((u8 *)g_secHeapBlocks[(short)block].addr + 0x200);
        if (Card_ReadFile((u8 *)g_cardBuf,0x200,0x80) == 0)
        {
            if (g_cardBuf->checksum == Save_Checksum((u8 *)g_cardBuf,0x1a))
            {
                if (g_progress.storyStage < g_cardBuf->progress.storyStage)
                {
                    g_progress.storyStage = g_cardBuf->progress.storyStage;
                }
                if (g_progress.freeStages < g_cardBuf->progress.freeStages)
                {
                    g_progress.freeStages = g_cardBuf->progress.freeStages;
                }
                if (g_progress.clears < g_cardBuf->progress.clears)
                {
                    g_progress.clears = g_cardBuf->progress.clears;
                }
                memset(g_progress.charOpen, 1, 11);
                if (g_cardBuf->progress.charOpen[11] == 1)
                {
                    g_progress.charOpen[11] = 1;
                }
                if (g_cardBuf->progress.charOpen[12] == 1)
                {
                    g_progress.charOpen[12] = 1;
                }
                if (g_cardBuf->progress.charOpen[13] == 1)
                {
                    g_progress.charOpen[13] = 1;
                }
                Msg_Close();
                break;
            }
            Card_CloseFile();
        }
        else
        {
            Card_CloseFile();
        }
        Card_DisableEvents();
        SecHeap_Free(g_cardBufBlock);
        Msg_Close();
        *step = CPROG_LOAD_FAILED;
        return 0;
    case CPROG_SAVING:
    case CPROG_NEW_FILE_SAVING:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,0,"%CSaving\201c%KPlease do not remove Memoy Card.%E");
        break;
    case CPROG_WAIT_SAVING:
        Task_GetWork(g_uiTaskIds[0],&msg);
        if (g_textWins[msg->win].state != TEXTWIN_SHOWN)
        {
            return 0;
        }
        *step = CPROG_PACK;
        return 0;
    case CPROG_REMOVED:
        if (Msg_IsAnswered() == 0)
        {
            return 0;
        }
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,PADRright | PADRdown,"%CMemory Card was removed.%E");
        break;
    case CPROG_CREATE_FILE:
        /* a new save file */
        result = Card_Check();
        if ((0 < result) && (result < 3))
        {
            /* no card; the original goes on to create the file all the same */
            Card_DisableEvents();
            SecHeap_Free(g_cardBufBlock);
            Msg_Close();
            *step = CPROG_LOAD_FAILED;
        }
        block = SecHeap_Alloc(8);
        g_saveBuf = g_secHeapBlocks[(short)block].addr;
        g_cardBufBlock = block;
        g_cardBuf = (CardSystem *)(g_saveBuf + 0x200);
        if ((short)Save_CreateFile() == -1)
        {
            Card_DisableEvents();
            SecHeap_Free(g_cardBufBlock);
            Msg_Close();
            *step = CPROG_LOAD_FAILED;
            return 0;
        }
        break;
    case CPROG_NEW_SLOTS:
        for (i = 0; i < 4; i++)
        {
            g_cardBuf->slotUsed[i] = 0;
        }
        Progress_RecordClear();         /* a second time on this path (as in the original) */
        *step = CPROG_PACK;
        return 0;
    case CPROG_PACK:
        g_cardBuf->progress = g_progress;
        g_cardBuf->eventScenes = g_optEventScenes;
        g_cardBuf->mono = g_optMono;
        g_cardBuf->skipCpuTalk = g_optSkipCpuTalk;
        g_cardBuf->cpuFast = g_optCpuFast;
        g_cardBuf->checksum = Save_Checksum((u8 *)g_cardBuf,0x1a);
        break;
    case CPROG_WRITE:
        *step = Card_WriteFile((u8 *)g_cardBuf,0x200,0x80) == 0 ? CPROG_SAVED : CPROG_SAVE_FAILED;
        Card_CloseFile();
        Card_DisableEvents();
        SecHeap_Free(g_cardBufBlock);
        Msg_Close();
        return 0;
    case CPROG_SAVED:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,MSG_ANY_BUTTON,"%CSave complete.%E");
        break;
    case CPROG_WAIT_SAVED:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        *step = CPROG_DONE;
        return 0;
    case CPROG_SAVE_FAILED:
        if (Msg_IsDone() == 0)
        {
            return 0;
        }
        Msg_Show(0,MSG_AUTO_POS,MSG_AUTO_POS,MSG_AUTO_SIZE,MSG_AUTO_SIZE,0,MSG_ANY_BUTTON,"%CSave failed.%E");
        break;
    case CPROG_DONE:
        Task_SetUserData(-1,0);
        return 1;
    default:
        return 0;
    }
    *step += 1;
    return 0;
}

/* Story progress of a new console session: story mode at stage 0, free play offers 8 stages,
 * characters 0-10 can be picked. Called once after the logos. (0x800365E0) */
/* 0x800365E0 */
void Progress_Reset(void)
{
    g_progress.storyStage = 0;
    g_progress.freeStages = 8;
    g_progress.clears = 0;
    g_progress.charOpen[0] = 1;
    g_progress.charOpen[1] = 1;
    g_progress.charOpen[2] = 1;
    g_progress.charOpen[3] = 1;
    g_progress.charOpen[4] = 1;
    g_progress.charOpen[5] = 1;
    g_progress.charOpen[6] = 1;
    g_progress.charOpen[7] = 1;
    g_progress.charOpen[8] = 1;
    g_progress.charOpen[9] = 1;
    g_progress.charOpen[10] = 1;
    g_progress.charOpen[11] = 0;
    g_progress.charOpen[12] = 0;
    g_progress.charOpen[13] = 0;
}

/* Free the memory card buffer (0x80036638) */
/* 0x80036638 */
void Save_FreeBuffer(void)
{
    SecHeap_Free(g_cardBufBlock);
}

/* What the slot list shows of save slot slot: the slot's image, its board stage, year, month
 * and play mode (0x8003665C) */
/* 0x8003665C */
void Save_GetSlotInfo(SaveImage **image, u8 *stage, int *year, int *month, int *playMode, short slot)
{
    SaveImage *img;

    img = (SaveImage *)g_saveSlots[slot];
    *image = img;
    *stage = img->boardStage;
    *year = (char)img->year;
    *month = (char)img->month;
    *playMode = (char)img->playMode;
}

/* Checksum of a save block: inverted sum of the bytes, every even one inverted (0x800366D0) */
/* 0x800366D0 */
u32 Save_Checksum(u8 *data, int len)
{
    u32 sum;
    int i;

    sum = 0;
    for (i = 0; i < len; i++)
    {
        sum += (i & 1) == 0 ? ~data[i] & 0xff : data[i];
    }
    return ~sum;
}

/* Copy save slot slot into the working save record (0x800FE1A8); slot 4 = the system block
 * (progress table and options) (0x80036720) */
/* 0x80036720 */
u32 Save_SelectSlot(char slot)
{
    if (slot == 4)
    {
        g_progress = g_cardBuf->progress;
        g_optEventScenes = g_cardBuf->eventScenes;
        g_optMono = g_cardBuf->mono;
        g_optSkipCpuTalk = g_cardBuf->skipCpuTalk;
        g_optCpuFast = g_cardBuf->cpuFast;
        Snd_SetMono(g_optMono);
    }
    else
    {
        g_saveImage = *(SaveImage *)g_saveSlots[slot];
    }
    return 0;
}
