/**
 * @file          test_fsmgr.c
 * @brief         Host unit tests for _LIB/FileSysManager against
 *                _DOC/FileSysManager/API_REFERENCE.md: mount and format, the
 *                message queue and its API, every command in every state, path
 *                building, writes (buffered and direct), reads, listing,
 *                deletion, rename and the portable status codes. The volume is
 *                the in-memory one of shim/fs_sim.c.
 *
 *                Built twice by _TEST/CMakeLists.txt: with the write buffer
 *                (FSMGR_WRITE_BUFFER_SIZE=512, as CONFIG_FSMGR_BUFFERED_WRITE)
 *                and without (0, every payload written directly).
 *
 * @date          07/10/2026
 * @author        Shivam Chudasama
 * @copyright     Shivam Chudasama
 * @license       MIT
 */

/* SPDX-License-Identifier: MIT */

#include "FileSysManager.c"
#include "FileSysManagerFSM.c"
#include "unity.h"

#define ROOT                 FSMGR_MOUNT_POINT

/******************************************************************************/
/*  Shim hooks                                                                */
/******************************************************************************/
/* gi_FSMGR_Call() blocks on its semaphore: run the manager thread meanwhile */
void gv_SimOnBlock(struct k_sem *stpt_sem)
{
   (void)stpt_sem;
   while (sb_ProcessNext(K_NO_WAIT)) {}
}

void gv_SimRegisterTimer(struct k_timer *t)
{
   (void)t;
}

/******************************************************************************/
/*  Result recorder                                                           */
/******************************************************************************/
typedef struct
{
   FileSysCommand_E e_command;
   int i_status;
   bool b_final;
   uint8_t u8_entryType;
   uint32_t u32_entrySize;
   uint8_t u8ar_data[FSMGR_MAX_PATH_LEN];
   uint32_t u32_len;
   uint32_t u32_total;
} Rec_T;

static Rec_T sstar_rec[32];
static uint32_t su32_recCount;
static uint32_t su32_finals;
static int si_user;

static void sv_Record(const FsmgrResult_T *r, void *vpt_user)
{
   Rec_T *p = &sstar_rec[su32_recCount % ARRAY_SIZE(sstar_rec)];

   TEST_ASSERT_EQUAL_PTR(&si_user, vpt_user);
   p->e_command = r->e_command;
   p->i_status = r->i_status;
   p->b_final = r->b_final;
   p->u8_entryType = r->u8_entryType;
   p->u32_entrySize = r->u32_entrySize;
   p->u32_len = r->u32_len;
   p->u32_total = r->u32_total;
   TEST_ASSERT_TRUE(r->u32_len <= sizeof(p->u8ar_data));
   if (r->u32_len > 0U)
   {
      TEST_ASSERT_NOT_NULL(r->u8pt_data);
      (void)memcpy(p->u8ar_data, r->u8pt_data, r->u32_len);
   }
   p->u8ar_data[MIN(r->u32_len, sizeof(p->u8ar_data) - 1U)] = 0U;
   su32_recCount++;
   if (r->b_final) { su32_finals++; }
}

static const Rec_T *spt_Last(void)
{
   TEST_ASSERT_TRUE(su32_recCount > 0U);
   return &sstar_rec[(su32_recCount - 1U) % ARRAY_SIZE(sstar_rec)];
}

/** Run one message to its end; returns the final status. Exactly one final. */
static int si_Exec(FileSysCommand_E e_cmd, const void *vpt_data, uint32_t u32_len, uint8_t u8_flags)
{
   FileSysMessage_T st_msg;

   (void)memset(&st_msg, 0, sizeof(st_msg));
   st_msg.e_command = e_cmd;
   st_msg.u8_flags = u8_flags;
   st_msg.u32_sizeOfData = u32_len;
   if ((vpt_data != NULL) && (u32_len > 0U))
   {
      (void)memcpy(st_msg.u8_data, vpt_data, MIN(u32_len, (uint32_t)FS_MAX_CHUNK_SIZE));
   }
   su32_recCount = 0U;
   su32_finals = 0U;
   TEST_ASSERT_EQUAL_INT(0, gi_FSMGR_Submit(&st_msg, sv_Record, &si_user, K_NO_WAIT));
   TEST_ASSERT_TRUE(sb_ProcessNext(K_NO_WAIT));
   TEST_ASSERT_EQUAL_UINT32_MESSAGE(1U, su32_finals, "exactly one final result");
   TEST_ASSERT_TRUE(spt_Last()->b_final);
   TEST_ASSERT_EQUAL_INT(e_cmd, spt_Last()->e_command);
   return spt_Last()->i_status;
}

static int si_ExecStr(FileSysCommand_E e_cmd, const char *cpt_arg)
{
   return si_Exec(e_cmd, cpt_arg, (uint32_t)strlen(cpt_arg), 0U);
}

static int si_Read(uint32_t u32_n)
{
   return si_Exec(eFSC_READ_FILE, NULL, u32_n, 0U);
}

static FsmgrState_E se_State(void)
{
   return (FsmgrState_E)(sst_FSMGRContext.smf.current - &scstar_fsmStates[0]);
}

static void sv_AssertFile(const char *cpt_path, const void *vpt_exp, uint32_t u32_len)
{
   uint32_t u32_size = 0U;
   const uint8_t *u8pt = gu8pt_SimFsData(cpt_path, &u32_size);

   TEST_ASSERT_NOT_NULL_MESSAGE(u8pt, cpt_path);
   TEST_ASSERT_EQUAL_UINT32_MESSAGE(u32_len, u32_size, cpt_path);
   if (u32_len > 0U) { TEST_ASSERT_EQUAL_HEX8_ARRAY(vpt_exp, u8pt, u32_len); }
}

/** Fresh manager on a formatted, unmounted volume; run the thread's start. */
static void sv_Boot(void)
{
   k_msgq_purge(&sst_fsmgrMsgQ);
   sb_mounted = false;
   (void)memset(&sst_FSMGRContext, 0, sizeof(sst_FSMGRContext));
   gv_SimLogClear();
   gv_SimRunThread(sv_FSMGR_Thread);
}

void setUp(void)
{
   gv_SimFsReset();
   gv_SimFsFormat(false);
   atomic_set(&st_started, 0);
   sv_Boot();
   TEST_ASSERT_TRUE(gb_FSMGR_IsMounted());
}

void tearDown(void)
{
   // No handle leaks once back in IDLE
   if (se_State() == eFSM_IDLE)
   {
      TEST_ASSERT_EQUAL_UINT32_MESSAGE(0U, gu32_SimFsOpenHandles(), "handle leak");
   }
}

/******************************************************************************/
/*  Mount, start, queue                                                       */
/******************************************************************************/
static void test_MountFormatsUnformattedVolume(void)
{
   gv_SimFsReset();                            /* unformatted */
   sv_Boot();
   TEST_ASSERT_TRUE(gb_FSMGR_IsMounted());
   TEST_ASSERT_EQUAL_UINT32(1U, gu32_SimFsCalls(eSFS_MKFS));
   TEST_ASSERT_EQUAL_UINT32(2U, gu32_SimFsCalls(eSFS_MOUNT));
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("formatting"));
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("FATFS Mounted! /FLASH_DISK: on external flash (SPI)"));
}

static void test_MountFormattedVolumeDoesNotFormat(void)
{
   TEST_ASSERT_EQUAL_UINT32(0U, gu32_SimFsCalls(eSFS_MKFS));
   TEST_ASSERT_EQUAL_UINT32(1U, gu32_SimFsCalls(eSFS_MOUNT));
}

static void test_MkfsFailureLeavesUnmounted(void)
{
   gv_SimFsReset();
   gv_SimFsFailOnce(eSFS_MKFS, 0U, -EIO);
   sv_Boot();
   TEST_ASSERT_FALSE(gb_FSMGR_IsMounted());
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("Format failed (-5)"));
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("FATFS Mounting Failed!"));
}

static void test_MountFailureAfterFormat(void)
{
   gv_SimFsReset();
   gv_SimFsFailOnce(eSFS_MOUNT, 1U, -EIO);
   sv_Boot();
   TEST_ASSERT_FALSE(gb_FSMGR_IsMounted());
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("Mount failed after format"));
}

static void test_UnmountedCommandsFailNoDev(void)
{
   FileSysCommand_E e;

   gv_SimFsReset();
   gv_SimFsFailOnce(eSFS_MKFS, 0U, -EIO);
   sv_Boot();
   for (e = eFSC_OPEN_DIR; e < eFSC_COUNT; e++)
   {
      TEST_ASSERT_EQUAL_INT(-ENODEV, si_Exec(e, "x", 1U, 0U));
   }
   TEST_ASSERT_EQUAL_UINT32(0U, gu32_SimFsCalls(eSFS_OPEN));
}

static void test_StartOnce(void)
{
   TEST_ASSERT_EQUAL_INT(0, gi_FSMGR_Start());
   TEST_ASSERT_EQUAL_INT(-EALREADY, gi_FSMGR_Start());
}

static void test_SubmitValidation(void)
{
   FileSysMessage_T st_msg;
   uint32_t i;

   (void)memset(&st_msg, 0, sizeof(st_msg));
   TEST_ASSERT_EQUAL_INT(-EINVAL, gi_FSMGR_Submit(NULL, NULL, NULL, K_NO_WAIT));
   st_msg.e_command = eFSC_COUNT;
   TEST_ASSERT_EQUAL_INT(-EINVAL, gi_FSMGR_Submit(&st_msg, NULL, NULL, K_NO_WAIT));
   st_msg.e_command = (FileSysCommand_E)-1;
   TEST_ASSERT_EQUAL_INT(-EINVAL, gi_FSMGR_Submit(&st_msg, NULL, NULL, K_NO_WAIT));

   // A payload longer than FS_MAX_CHUNK_SIZE is refused, except a read's byte count
   st_msg.e_command = eFSC_WRITE_DATA;
   st_msg.u32_sizeOfData = FS_MAX_CHUNK_SIZE + 1U;
   TEST_ASSERT_EQUAL_INT(-EINVAL, gi_FSMGR_Submit(&st_msg, NULL, NULL, K_NO_WAIT));
   st_msg.u32_sizeOfData = FS_MAX_CHUNK_SIZE;
   TEST_ASSERT_EQUAL_INT(0, gi_FSMGR_Submit(&st_msg, NULL, NULL, K_NO_WAIT));
   st_msg.e_command = eFSC_READ_FILE;
   st_msg.u32_sizeOfData = 100000U;
   TEST_ASSERT_EQUAL_INT(0, gi_FSMGR_Submit(&st_msg, NULL, NULL, K_NO_WAIT));

   // A full queue refuses
   for (i = 2U; i < FSMGR_QUEUE_DEPTH; i++)
   {
      TEST_ASSERT_EQUAL_INT(0, gi_FSMGR_Submit(&st_msg, NULL, NULL, K_NO_WAIT));
   }
   TEST_ASSERT_NOT_EQUAL_INT(0, gi_FSMGR_Submit(&st_msg, NULL, NULL, K_NO_WAIT));

   // Messages without a callback run all the same (both are refused in IDLE)
   while (sb_ProcessNext(K_NO_WAIT)) {}
   TEST_ASSERT_EQUAL_INT(eFSM_IDLE, se_State());
}

static void test_CallWaitsForFinalResult(void)
{
   FileSysMessage_T st_msg;

   (void)memset(&st_msg, 0, sizeof(st_msg));
   st_msg.e_command = eFSC_MAKE_DIR;
   st_msg.u32_sizeOfData = 2U;
   (void)memcpy(st_msg.u8_data, "FW", 2U);
   TEST_ASSERT_EQUAL_INT(0, gi_FSMGR_Call(&st_msg));
   TEST_ASSERT_TRUE(gb_SimFsExists(ROOT "/FW"));

   st_msg.e_command = eFSC_OPEN_DIR;
   (void)memcpy(st_msg.u8_data, "NO", 2U);
   TEST_ASSERT_EQUAL_INT(-ENOENT, gi_FSMGR_Call(&st_msg));

   // A listing's entries are not final; Call returns once the listing ends
   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPut(ROOT "/a.bin", "x", 1U));
   st_msg.e_command = eFSC_DEBUG_LIST_DRIVE;
   st_msg.u32_sizeOfData = 0U;
   TEST_ASSERT_EQUAL_INT(0, gi_FSMGR_Call(&st_msg));

   st_msg.e_command = eFSC_COUNT;
   TEST_ASSERT_EQUAL_INT(-EINVAL, gi_FSMGR_Call(&st_msg));
}

/******************************************************************************/
/*  Directories and paths                                                     */
/******************************************************************************/
static void test_MakeDirBecomesCurrent(void)
{
   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_MAKE_DIR, "FW"));
   TEST_ASSERT_EQUAL_STRING(ROOT "/FW", (const char *)spt_Last()->u8ar_data);
   TEST_ASSERT_EQUAL_STRING(ROOT "/FW", sst_FSMGRContext.as8_currentDir);
   TEST_ASSERT_TRUE(gb_SimFsExists(ROOT "/FW"));

   // An existing directory is fine, and a relative directory is taken from the root
   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_MAKE_DIR, "FW"));
   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_MAKE_DIR, "LOG"));
   TEST_ASSERT_EQUAL_STRING(ROOT "/LOG", sst_FSMGRContext.as8_currentDir);
   TEST_ASSERT_EQUAL_INT(eFSM_IDLE, se_State());
}

static void test_MakeDirKeepDirFlag(void)
{
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_MAKE_DIR, ROOT "/FW", (uint32_t)strlen(ROOT "/FW"),
      FSMGR_MSG_KEEP_DIR));
   TEST_ASSERT_TRUE(gb_SimFsExists(ROOT "/FW"));
   TEST_ASSERT_EQUAL_STRING(ROOT, sst_FSMGRContext.as8_currentDir);
}

static void test_MakeDirMissingParentFails(void)
{
   TEST_ASSERT_EQUAL_INT(-ENOENT, si_ExecStr(eFSC_MAKE_DIR, "A/B"));
   TEST_ASSERT_EQUAL_INT(eFSM_IDLE, se_State());
   TEST_ASSERT_EQUAL_STRING(ROOT, sst_FSMGRContext.as8_currentDir);

   gv_SimFsFailOnce(eSFS_MKDIR, 0U, -EIO);
   TEST_ASSERT_EQUAL_INT(-EIO, si_ExecStr(eFSC_MAKE_DIR, "C"));
}

static void test_OpenDir(void)
{
   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPutDir(ROOT "/FW"));
   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPut(ROOT "/f.txt", "x", 1U));

   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_OPEN_DIR, "/FW"));
   TEST_ASSERT_EQUAL_STRING(ROOT "/FW", sst_FSMGRContext.as8_currentDir);
   TEST_ASSERT_EQUAL_INT(-ENOENT, si_ExecStr(eFSC_OPEN_DIR, "NONE"));
   TEST_ASSERT_EQUAL_INT(-ENOTDIR, si_ExecStr(eFSC_OPEN_DIR, "f.txt"));
   // A failed cd keeps the current directory
   TEST_ASSERT_EQUAL_STRING(ROOT "/FW", sst_FSMGRContext.as8_currentDir);
}

static void test_PathRules(void)
{
   char car_long[FS_MAX_CHUNK_SIZE];

   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPutDir(ROOT "/FW"));
   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_OPEN_DIR, "FW"));

   // Relative file names are taken from the current directory
   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_OPEN_FILE_WRITE, "a.txt"));
   TEST_ASSERT_EQUAL_STRING(ROOT "/FW/a.txt", (const char *)spt_Last()->u8ar_data);
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_ABORT, NULL, 0U, 0U));

   // "/x" from the root, "/DRIVE:/x" as is
   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_OPEN_FILE_WRITE, "/b.txt"));
   TEST_ASSERT_EQUAL_STRING(ROOT "/b.txt", (const char *)spt_Last()->u8ar_data);
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_ABORT, NULL, 0U, 0U));
   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_OPEN_FILE_WRITE, ROOT "/FW/c.txt"));
   TEST_ASSERT_EQUAL_STRING(ROOT "/FW/c.txt", (const char *)spt_Last()->u8ar_data);
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_CLOSE_FILE, NULL, 0U, 0U));

   // Empty names, a lone "/", and a terminator inside are refused
   TEST_ASSERT_EQUAL_INT(-EINVAL, si_Exec(eFSC_OPEN_FILE_WRITE, NULL, 0U, 0U));
   TEST_ASSERT_EQUAL_INT(-EINVAL, si_ExecStr(eFSC_OPEN_FILE_WRITE, "/"));
   TEST_ASSERT_EQUAL_INT(-EINVAL, si_Exec(eFSC_OPEN_FILE_WRITE, "a\0b", 3U, 0U));
   TEST_ASSERT_EQUAL_INT(-EINVAL, si_Exec(eFSC_MAKE_DIR, NULL, 0U, 0U));

   // A name that does not fit with its base path (it used to be cut short)
   (void)memset(car_long, 'n', sizeof(car_long));
   TEST_ASSERT_EQUAL_INT(-ENAMETOOLONG, si_Exec(eFSC_OPEN_FILE_WRITE, car_long, sizeof(car_long), 0U));
   TEST_ASSERT_EQUAL_INT(eFSM_IDLE, se_State());
}

/******************************************************************************/
/*  Writing                                                                   */
/******************************************************************************/
static void test_WriteCloseRoundTrip(void)
{
   uint8_t u8ar_data[600];
   uint32_t i;

   for (i = 0U; i < sizeof(u8ar_data); i++) { u8ar_data[i] = (uint8_t)(i * 7U); }

   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_OPEN_FILE_WRITE, "w.bin"));
   TEST_ASSERT_TRUE(gb_FSMGR_IsFileOpen());
   TEST_ASSERT_EQUAL_INT(eFSM_WRITE_FILE, se_State());
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_WRITE_DATA, &u8ar_data[0], 242U, 0U));
   TEST_ASSERT_EQUAL_UINT32(242U, spt_Last()->u32_total);
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_WRITE_DATA, &u8ar_data[242], 242U, 0U));
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_WRITE_DATA, &u8ar_data[484], 116U, 0U));
   TEST_ASSERT_EQUAL_UINT32(600U, spt_Last()->u32_total);

#if FSMGR_WRITE_BUFFER_SIZE > 0
   // Buffered: only whole buffers are on the volume before the close
   {
      uint32_t u32_size = 0U;
      (void)gu8pt_SimFsData(ROOT "/w.bin", &u32_size);
      TEST_ASSERT_EQUAL_UINT32(FSMGR_WRITE_BUFFER_SIZE, u32_size);
      TEST_ASSERT_EQUAL_UINT32(1U, gu32_SimFsCalls(eSFS_WRITE));
   }
#else
   TEST_ASSERT_EQUAL_UINT32(3U, gu32_SimFsCalls(eSFS_WRITE));
#endif // FSMGR_WRITE_BUFFER_SIZE

   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_CLOSE_FILE, NULL, 0U, 0U));
   TEST_ASSERT_EQUAL_UINT32(600U, spt_Last()->u32_total);
   TEST_ASSERT_EQUAL_UINT32(1U, gu32_SimFsCalls(eSFS_SYNC));
   TEST_ASSERT_FALSE(gb_FSMGR_IsFileOpen());
   TEST_ASSERT_EQUAL_INT(eFSM_IDLE, se_State());
   sv_AssertFile(ROOT "/w.bin", u8ar_data, sizeof(u8ar_data));
}

static void test_OpenWriteDoesNotTruncate(void)
{
   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPut(ROOT "/t.txt", "0123456789", 10U));
   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_OPEN_FILE_WRITE, "t.txt"));
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_WRITE_DATA, "ab", 2U, 0U));
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_CLOSE_FILE, NULL, 0U, 0U));
   sv_AssertFile(ROOT "/t.txt", "ab23456789", 10U);
}

static void test_WriteSizeLimits(void)
{
   uint8_t u8ar_data[FS_MAX_CHUNK_SIZE] = { 0 };

   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_OPEN_FILE_WRITE, "s.bin"));
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_WRITE_DATA, u8ar_data, 1U, 0U));
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_WRITE_DATA, u8ar_data, FS_MAX_CHUNK_SIZE, 0U));
   // An empty write fails and closes the file
   TEST_ASSERT_EQUAL_INT(-EINVAL, si_Exec(eFSC_WRITE_DATA, NULL, 0U, 0U));
   TEST_ASSERT_EQUAL_INT(eFSM_IDLE, se_State());
   TEST_ASSERT_FALSE(gb_FSMGR_IsFileOpen());
   TEST_ASSERT_EQUAL_INT(-EPERM, si_Exec(eFSC_WRITE_DATA, u8ar_data, 1U, 0U));
}

static void test_WriteFailureClosesFile(void)
{
   uint8_t u8ar_data[FS_MAX_CHUNK_SIZE] = { 0 };
   uint32_t i;

   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_OPEN_FILE_WRITE, "e.bin"));
   gv_SimFsFailOnce(eSFS_WRITE, 0U, -EIO);
   // The failing fs_write comes at once, or when the buffer fills
   for (i = 0U; i < 10U; i++)
   {
      if (si_Exec(eFSC_WRITE_DATA, u8ar_data, sizeof(u8ar_data), 0U) != 0) { break; }
   }
   TEST_ASSERT_EQUAL_INT(-EIO, spt_Last()->i_status);
   TEST_ASSERT_EQUAL_INT(eFSM_IDLE, se_State());
   TEST_ASSERT_FALSE(gb_FSMGR_IsFileOpen());
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("FSM operation failed"));
}

static void test_VolumeFullIsNoSpace(void)
{
   uint8_t u8ar_data[FS_MAX_CHUNK_SIZE] = { 0 };
   uint32_t i;
   int i_ret = 0;

   gv_SimFsSetFree(1000U);
   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_OPEN_FILE_WRITE, "full.bin"));
   for (i = 0U; (i < 10U) && (i_ret == 0); i++)
   {
      i_ret = si_Exec(eFSC_WRITE_DATA, u8ar_data, sizeof(u8ar_data), 0U);
   }
   if (i_ret == 0)
   {
      i_ret = si_Exec(eFSC_CLOSE_FILE, NULL, 0U, 0U);
   }
   TEST_ASSERT_EQUAL_INT(-ENOSPC, i_ret);
   TEST_ASSERT_EQUAL_INT(eFSM_IDLE, se_State());
}

static void test_CloseSyncFailure(void)
{
   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_OPEN_FILE_WRITE, "y.bin"));
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_WRITE_DATA, "abc", 3U, 0U));
   gv_SimFsFailOnce(eSFS_SYNC, 0U, -EIO);
   TEST_ASSERT_EQUAL_INT(-EIO, si_Exec(eFSC_CLOSE_FILE, NULL, 0U, 0U));
   TEST_ASSERT_EQUAL_INT(eFSM_IDLE, se_State());
   TEST_ASSERT_FALSE(gb_FSMGR_IsFileOpen());
}

static void test_OpenFailure(void)
{
   gv_SimFsFailOnce(eSFS_OPEN, 0U, -EIO);
   TEST_ASSERT_EQUAL_INT(-EIO, si_ExecStr(eFSC_OPEN_FILE_WRITE, "o.bin"));
   TEST_ASSERT_EQUAL_INT(eFSM_IDLE, se_State());
   TEST_ASSERT_EQUAL_INT(-ENOENT, si_ExecStr(eFSC_OPEN_FILE_WRITE, "NODIR/o.bin"));
}

static void test_AbortWriteDropsBuffer(void)
{
   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_OPEN_FILE_WRITE, "ab.bin"));
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_WRITE_DATA, "abc", 3U, 0U));
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_ABORT, NULL, 0U, 0U));
   TEST_ASSERT_EQUAL_INT(eFSM_IDLE, se_State());
   TEST_ASSERT_FALSE(gb_FSMGR_IsFileOpen());
#if FSMGR_WRITE_BUFFER_SIZE > 0
   sv_AssertFile(ROOT "/ab.bin", "", 0U);
   TEST_ASSERT_EQUAL_UINT32(0U, sst_FSMGRContext.u32_writeCacheFill);
#else
   sv_AssertFile(ROOT "/ab.bin", "abc", 3U);
#endif // FSMGR_WRITE_BUFFER_SIZE
}

/******************************************************************************/
/*  Reading                                                                   */
/******************************************************************************/
static void test_ReadSizesAndEof(void)
{
   uint8_t u8ar_data[300];
   uint32_t i;

   for (i = 0U; i < sizeof(u8ar_data); i++) { u8ar_data[i] = (uint8_t)i; }
   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPut(ROOT "/r.bin", u8ar_data, sizeof(u8ar_data)));

   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_OPEN_FILE_READ, "r.bin"));
   TEST_ASSERT_EQUAL_INT(eFSM_READ_FILE, se_State());
   TEST_ASSERT_EQUAL_INT(0, si_Read(4U));
   TEST_ASSERT_EQUAL_UINT32(4U, spt_Last()->u32_len);
   TEST_ASSERT_EQUAL_HEX8_ARRAY(u8ar_data, spt_Last()->u8ar_data, 4U);

   // 0 and more than FS_MAX_CHUNK_SIZE read FS_MAX_CHUNK_SIZE
   TEST_ASSERT_EQUAL_INT(0, si_Read(0U));
   TEST_ASSERT_EQUAL_UINT32(FS_MAX_CHUNK_SIZE, spt_Last()->u32_len);
   TEST_ASSERT_EQUAL_HEX8_ARRAY(&u8ar_data[4], spt_Last()->u8ar_data, FS_MAX_CHUNK_SIZE);
   TEST_ASSERT_EQUAL_INT(0, si_Read(100000U));
   TEST_ASSERT_EQUAL_UINT32(300U - 4U - FS_MAX_CHUNK_SIZE, spt_Last()->u32_len);

   // End of file: 0 bytes, status 0
   TEST_ASSERT_EQUAL_INT(0, si_Read(10U));
   TEST_ASSERT_EQUAL_UINT32(0U, spt_Last()->u32_len);
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("End of file reached"));

   // Closing a file read needs no flush or sync
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_CLOSE_FILE, NULL, 0U, 0U));
   TEST_ASSERT_EQUAL_UINT32(0U, gu32_SimFsCalls(eSFS_SYNC));
   TEST_ASSERT_EQUAL_INT(eFSM_IDLE, se_State());
}

static void test_ReadErrors(void)
{
   TEST_ASSERT_EQUAL_INT(-ENOENT, si_ExecStr(eFSC_OPEN_FILE_READ, "none.bin"));
   TEST_ASSERT_EQUAL_INT(eFSM_IDLE, se_State());

   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPut(ROOT "/r.bin", "abc", 3U));
   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_OPEN_FILE_READ, "r.bin"));
   gv_SimFsFailOnce(eSFS_READ, 0U, -EIO);
   TEST_ASSERT_EQUAL_INT(-EIO, si_Read(3U));
   TEST_ASSERT_EQUAL_INT(eFSM_IDLE, se_State());
   TEST_ASSERT_FALSE(gb_FSMGR_IsFileOpen());

   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_OPEN_FILE_READ, "r.bin"));
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_ABORT, NULL, 0U, 0U));
   TEST_ASSERT_EQUAL_INT(eFSM_IDLE, se_State());
}

/******************************************************************************/
/*  Commands refused by state                                                 */
/******************************************************************************/
static void test_IdleRefusesReadWrite(void)
{
   TEST_ASSERT_EQUAL_INT(-EPERM, si_Exec(eFSC_WRITE_DATA, "a", 1U, 0U));
   TEST_ASSERT_EQUAL_INT(-EPERM, si_Read(1U));
   // Close and abort without an open file succeed
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_CLOSE_FILE, NULL, 0U, 0U));
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_ABORT, NULL, 0U, 0U));
}

static void test_WriteStateRefusesOthers(void)
{
   static const FileSysCommand_E scear_refused[] =
   {
      eFSC_OPEN_DIR, eFSC_MAKE_DIR, eFSC_OPEN_FILE_READ, eFSC_OPEN_FILE_WRITE, eFSC_READ_FILE,
      eFSC_DEBUG_LIST_DRIVE, eFSC_DELETE_FILE, eFSC_DELETE_DIR, eFSC_RENAME,
   };
   uint32_t i;

   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_OPEN_FILE_WRITE, "x.bin"));
   for (i = 0U; i < ARRAY_SIZE(scear_refused); i++)
   {
      TEST_ASSERT_EQUAL_INT(-EPERM, si_ExecStr(scear_refused[i], "x"));
      TEST_ASSERT_EQUAL_INT(eFSM_WRITE_FILE, se_State());
   }
   // Still writable
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_WRITE_DATA, "z", 1U, 0U));
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_CLOSE_FILE, NULL, 0U, 0U));
   sv_AssertFile(ROOT "/x.bin", "z", 1U);
}

static void test_ReadStateRefusesOthers(void)
{
   static const FileSysCommand_E scear_refused[] =
   {
      eFSC_OPEN_DIR, eFSC_MAKE_DIR, eFSC_OPEN_FILE_READ, eFSC_OPEN_FILE_WRITE, eFSC_WRITE_DATA,
      eFSC_DEBUG_LIST_DRIVE, eFSC_DELETE_FILE, eFSC_DELETE_DIR, eFSC_RENAME,
   };
   uint32_t i;

   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPut(ROOT "/r.bin", "abc", 3U));
   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_OPEN_FILE_READ, "r.bin"));
   for (i = 0U; i < ARRAY_SIZE(scear_refused); i++)
   {
      TEST_ASSERT_EQUAL_INT(-EPERM, si_ExecStr(scear_refused[i], "x"));
      TEST_ASSERT_EQUAL_INT(eFSM_READ_FILE, se_State());
   }
   TEST_ASSERT_EQUAL_INT(0, si_Read(3U));
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_CLOSE_FILE, NULL, 0U, 0U));
}

static void test_AbortInIdleResetsCurrentDir(void)
{
   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_MAKE_DIR, "FW"));
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_ABORT, NULL, 0U, 0U));
   // The sample emptied the current directory, so relative paths broke
   TEST_ASSERT_EQUAL_STRING(ROOT, sst_FSMGRContext.as8_currentDir);
   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_OPEN_FILE_WRITE, "after.txt"));
   TEST_ASSERT_EQUAL_STRING(ROOT "/after.txt", (const char *)spt_Last()->u8ar_data);
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_CLOSE_FILE, NULL, 0U, 0U));
}

static void test_UnknownCommandInIdle(void)
{
   sst_FSMGRContext.st_currentMsg.e_command = eFSC_COUNT;
   sst_FSMGRContext.st_currentMsg.fpt_onResult = sv_Record;
   sst_FSMGRContext.st_currentMsg.vpt_user = &si_user;
   su32_recCount = 0U;
   su32_finals = 0U;
   gv_FileSysManagerFSMRun(&sst_FSMGRContext);
   TEST_ASSERT_EQUAL_UINT32(1U, su32_finals);
   TEST_ASSERT_EQUAL_INT(-EINVAL, spt_Last()->i_status);
}

static void test_MissingResultIsReportedAsIo(void)
{
   // A run action that ends without a result (a defect) still ends the message:
   // a transient state, never current in practice, has such a run action
   sst_FSMGRContext.st_currentMsg.e_command = eFSC_CLOSE_FILE;
   sst_FSMGRContext.st_currentMsg.fpt_onResult = sv_Record;
   sst_FSMGRContext.st_currentMsg.vpt_user = &si_user;
   su32_recCount = 0U;
   su32_finals = 0U;
   sst_FSMGRContext.smf.current = &scstar_fsmStates[eFSM_CREATE_DIR];
   gv_FileSysManagerFSMRun(&sst_FSMGRContext);
   TEST_ASSERT_EQUAL_UINT32(1U, su32_finals);
   TEST_ASSERT_EQUAL_INT(-EIO, spt_Last()->i_status);
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("ended without a result"));
   sst_FSMGRContext.smf.current = &scstar_fsmStates[eFSM_IDLE];
}

/******************************************************************************/
/*  Delete, rename, list                                                      */
/******************************************************************************/
static void test_DeleteFile(void)
{
   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPutDir(ROOT "/D"));
   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPut(ROOT "/f.txt", "x", 1U));

   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_DELETE_FILE, "f.txt"));
   TEST_ASSERT_FALSE(gb_SimFsExists(ROOT "/f.txt"));
   TEST_ASSERT_EQUAL_INT(-ENOENT, si_ExecStr(eFSC_DELETE_FILE, "f.txt"));
   TEST_ASSERT_EQUAL_INT(-EISDIR, si_ExecStr(eFSC_DELETE_FILE, "D"));
   TEST_ASSERT_TRUE(gb_SimFsExists(ROOT "/D"));

   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPut(ROOT "/g.txt", "x", 1U));
   gv_SimFsFailOnce(eSFS_UNLINK, 0U, -EIO);
   TEST_ASSERT_EQUAL_INT(-EIO, si_ExecStr(eFSC_DELETE_FILE, "g.txt"));
   TEST_ASSERT_EQUAL_INT(eFSM_IDLE, se_State());
}

static void test_DeleteDir(void)
{
   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPutDir(ROOT "/D"));
   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPut(ROOT "/D/a", "1", 1U));
   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPut(ROOT "/D/b", "2", 1U));
   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPut(ROOT "/f.txt", "x", 1U));

   TEST_ASSERT_EQUAL_INT(-ENOTDIR, si_ExecStr(eFSC_DELETE_DIR, "f.txt"));
   // Its files go first, then the directory
   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_DELETE_DIR, "D"));
   TEST_ASSERT_FALSE(gb_SimFsExists(ROOT "/D/a"));
   TEST_ASSERT_FALSE(gb_SimFsExists(ROOT "/D"));
   TEST_ASSERT_EQUAL_INT(-ENOENT, si_ExecStr(eFSC_DELETE_DIR, "D"));

   // A subdirectory stops it: no recursive delete
   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPutDir(ROOT "/E"));
   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPutDir(ROOT "/E/S"));
   TEST_ASSERT_EQUAL_INT(-ENOTSUP, si_ExecStr(eFSC_DELETE_DIR, "E"));
   TEST_ASSERT_TRUE(gb_SimFsExists(ROOT "/E/S"));
   TEST_ASSERT_EQUAL_INT(eFSM_IDLE, se_State());
}

static void test_Rename(void)
{
   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPutDir(ROOT "/FW"));
   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPut(ROOT "/FW/UPLOAD.TMP", "new", 3U));
   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPut(ROOT "/FW/APP", "old", 3U));

   // An existing target is replaced
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_RENAME, "/FW/UPLOAD.TMP\0/FW/APP", 22U, 0U));
   TEST_ASSERT_FALSE(gb_SimFsExists(ROOT "/FW/UPLOAD.TMP"));
   sv_AssertFile(ROOT "/FW/APP", "new", 3U);

   // Relative names from the current directory
   TEST_ASSERT_EQUAL_INT(0, si_ExecStr(eFSC_OPEN_DIR, "FW"));
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_RENAME, "APP\0APP2", 8U, 0U));
   TEST_ASSERT_TRUE(gb_SimFsExists(ROOT "/FW/APP2"));

   TEST_ASSERT_EQUAL_INT(-EINVAL, si_ExecStr(eFSC_RENAME, "APP2"));
   TEST_ASSERT_EQUAL_INT(-EINVAL, si_Exec(eFSC_RENAME, "APP2\0", 5U, 0U));
   TEST_ASSERT_EQUAL_INT(-EINVAL, si_Exec(eFSC_RENAME, "\0APP3", 5U, 0U));
   TEST_ASSERT_EQUAL_INT(-ENOENT, si_Exec(eFSC_RENAME, "NO\0APP3", 7U, 0U));
   TEST_ASSERT_EQUAL_INT(-EINVAL, si_Exec(eFSC_RENAME, NULL, 0U, 0U));
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("Rename failed"));
}

static void test_ListDrive(void)
{
   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPutDir(ROOT "/FW"));
   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPut(ROOT "/FW/APP", "12345", 5U));
   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPutDir(ROOT "/FW/OLD"));
   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPut(ROOT "/FW/OLD/deep", "x", 1U));
   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPut(ROOT "/top.txt", "ab", 2U));

   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_DEBUG_LIST_DRIVE, NULL, 0U, 0U));
   // FW, FW/APP, FW/OLD (its content is not listed), top.txt, then the final result
   TEST_ASSERT_EQUAL_UINT32(5U, su32_recCount);
   TEST_ASSERT_FALSE(sstar_rec[0].b_final);
   TEST_ASSERT_EQUAL_STRING(ROOT "/FW", (const char *)sstar_rec[0].u8ar_data);
   TEST_ASSERT_EQUAL_UINT8(FSMGR_ENTRY_DIR, sstar_rec[0].u8_entryType);
   TEST_ASSERT_EQUAL_STRING(ROOT "/FW/APP", (const char *)sstar_rec[1].u8ar_data);
   TEST_ASSERT_EQUAL_UINT8(FSMGR_ENTRY_FILE, sstar_rec[1].u8_entryType);
   TEST_ASSERT_EQUAL_UINT32(5U, sstar_rec[1].u32_entrySize);
   TEST_ASSERT_EQUAL_STRING(ROOT "/FW/OLD", (const char *)sstar_rec[2].u8ar_data);
   TEST_ASSERT_EQUAL_UINT8(FSMGR_ENTRY_DIR, sstar_rec[2].u8_entryType);
   TEST_ASSERT_EQUAL_STRING(ROOT "/top.txt", (const char *)sstar_rec[3].u8ar_data);
   TEST_ASSERT_EQUAL_UINT32(2U, sstar_rec[3].u32_entrySize);
   TEST_ASSERT_EQUAL_UINT32(4U, sstar_rec[4].u32_total);
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("depth>1 not supported"));
}

static void test_ListEmptyAndErrors(void)
{
   TEST_ASSERT_EQUAL_INT(0, si_Exec(eFSC_DEBUG_LIST_DRIVE, NULL, 0U, 0U));
   TEST_ASSERT_EQUAL_UINT32(1U, su32_recCount);
   TEST_ASSERT_EQUAL_UINT32(0U, spt_Last()->u32_total);

   gv_SimFsFailOnce(eSFS_OPENDIR, 0U, -EIO);
   TEST_ASSERT_EQUAL_INT(-EIO, si_Exec(eFSC_DEBUG_LIST_DRIVE, NULL, 0U, 0U));

   TEST_ASSERT_EQUAL_INT(0, gi_SimFsPut(ROOT "/a", "1", 1U));
   gv_SimFsFailOnce(eSFS_READDIR, 0U, -EIO);
   TEST_ASSERT_EQUAL_INT(-EIO, si_Exec(eFSC_DEBUG_LIST_DRIVE, NULL, 0U, 0U));
   TEST_ASSERT_EQUAL_INT(eFSM_IDLE, se_State());
}

/******************************************************************************/
/*  Status codes                                                              */
/******************************************************************************/
static void test_StatusCodes(void)
{
   TEST_ASSERT_EQUAL_UINT8(eFSS_OK, gu8_FSMGR_StatusCode(0));
   TEST_ASSERT_EQUAL_UINT8(eFSS_NOT_FOUND, gu8_FSMGR_StatusCode(-ENOENT));
   TEST_ASSERT_EQUAL_UINT8(eFSS_EXISTS, gu8_FSMGR_StatusCode(-EEXIST));
   TEST_ASSERT_EQUAL_UINT8(eFSS_NOT_EMPTY, gu8_FSMGR_StatusCode(-ENOTEMPTY));
   TEST_ASSERT_EQUAL_UINT8(eFSS_NO_SPACE, gu8_FSMGR_StatusCode(-ENOSPC));
   TEST_ASSERT_EQUAL_UINT8(eFSS_BAD_ARG, gu8_FSMGR_StatusCode(-EINVAL));
   TEST_ASSERT_EQUAL_UINT8(eFSS_BAD_ARG, gu8_FSMGR_StatusCode(-ENAMETOOLONG));
   TEST_ASSERT_EQUAL_UINT8(eFSS_BAD_STATE, gu8_FSMGR_StatusCode(-EPERM));
   TEST_ASSERT_EQUAL_UINT8(eFSS_BAD_STATE, gu8_FSMGR_StatusCode(-EBADF));
   TEST_ASSERT_EQUAL_UINT8(eFSS_BUSY, gu8_FSMGR_StatusCode(-EBUSY));
   TEST_ASSERT_EQUAL_UINT8(eFSS_BUSY, gu8_FSMGR_StatusCode(-ENOMSG));
   TEST_ASSERT_EQUAL_UINT8(eFSS_BUSY, gu8_FSMGR_StatusCode(-EAGAIN));
   TEST_ASSERT_EQUAL_UINT8(eFSS_NOT_MOUNTED, gu8_FSMGR_StatusCode(-ENODEV));
   TEST_ASSERT_EQUAL_UINT8(eFSS_NOT_SUPPORTED, gu8_FSMGR_StatusCode(-ENOTSUP));
   TEST_ASSERT_EQUAL_UINT8(eFSS_WRONG_TYPE, gu8_FSMGR_StatusCode(-ENOTDIR));
   TEST_ASSERT_EQUAL_UINT8(eFSS_WRONG_TYPE, gu8_FSMGR_StatusCode(-EISDIR));
   TEST_ASSERT_EQUAL_UINT8(eFSS_DENIED, gu8_FSMGR_StatusCode(-EACCES));
   TEST_ASSERT_EQUAL_UINT8(eFSS_IO, gu8_FSMGR_StatusCode(-EIO));
   TEST_ASSERT_EQUAL_UINT8(eFSS_IO, gu8_FSMGR_StatusCode(-12345));
}

int main(void)
{
   (void)setvbuf(stdout, NULL, _IONBF, 0);
   UNITY_BEGIN();
   RUN_TEST(test_MountFormatsUnformattedVolume);
   RUN_TEST(test_MountFormattedVolumeDoesNotFormat);
   RUN_TEST(test_MkfsFailureLeavesUnmounted);
   RUN_TEST(test_MountFailureAfterFormat);
   RUN_TEST(test_UnmountedCommandsFailNoDev);
   RUN_TEST(test_StartOnce);
   RUN_TEST(test_SubmitValidation);
   RUN_TEST(test_CallWaitsForFinalResult);
   RUN_TEST(test_MakeDirBecomesCurrent);
   RUN_TEST(test_MakeDirKeepDirFlag);
   RUN_TEST(test_MakeDirMissingParentFails);
   RUN_TEST(test_OpenDir);
   RUN_TEST(test_PathRules);
   RUN_TEST(test_WriteCloseRoundTrip);
   RUN_TEST(test_OpenWriteDoesNotTruncate);
   RUN_TEST(test_WriteSizeLimits);
   RUN_TEST(test_WriteFailureClosesFile);
   RUN_TEST(test_VolumeFullIsNoSpace);
   RUN_TEST(test_CloseSyncFailure);
   RUN_TEST(test_OpenFailure);
   RUN_TEST(test_AbortWriteDropsBuffer);
   RUN_TEST(test_ReadSizesAndEof);
   RUN_TEST(test_ReadErrors);
   RUN_TEST(test_IdleRefusesReadWrite);
   RUN_TEST(test_WriteStateRefusesOthers);
   RUN_TEST(test_ReadStateRefusesOthers);
   RUN_TEST(test_AbortInIdleResetsCurrentDir);
   RUN_TEST(test_UnknownCommandInIdle);
   RUN_TEST(test_MissingResultIsReportedAsIo);
   RUN_TEST(test_DeleteFile);
   RUN_TEST(test_DeleteDir);
   RUN_TEST(test_Rename);
   RUN_TEST(test_ListDrive);
   RUN_TEST(test_ListEmptyAndErrors);
   RUN_TEST(test_StatusCodes);
   return UNITY_END();
}
