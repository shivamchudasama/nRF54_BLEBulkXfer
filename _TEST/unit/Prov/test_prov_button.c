/**
 * @file          test_prov_button.c
 * @brief         Host unit tests for the provisioning wipe button
 *                (_ASW/_PROV/ProvButton.c): holding DK Button 0 (DK_BTN1_MSK)
 *                for CONFIG_PROV_WIPE_HOLD_MS calls gv_Prov_RequestWipe() once;
 *                releasing it earlier cancels. ProvButton.c is included to
 *                reach its delayable work item. The DK library
 *                (shim/dk_buttons_and_leds.h) and gv_Prov_RequestWipe() are
 *                stubbed; time is simulated (gi64_simNowMs).
 *                Contract: _DOC/Provisioning/README.md (wipe).
 *
 * @date          02/10/2026
 * @author        Shivam Chudasama [SC]
 * @copyright     Bajaj Auto Technology Limited (BATL)
 */

#define CONFIG_PROV_WIPE_HOLD_MS             5000

#include "ProvButton.c"
#include "unity.h"

/******************************************************************************/
/*  Stubs                                                                     */
/******************************************************************************/
static button_handler_t sfpt_handler;
static int si_dkInitRet;
static uint32_t su32_dkInits;
static uint32_t su32_wipes;

int dk_buttons_init(button_handler_t button_handler)
{
   su32_dkInits++;
   sfpt_handler = button_handler;
   return si_dkInitRet;
}

void gv_Prov_RequestWipe(void)
{
   su32_wipes++;
}

/******************************************************************************/
/*  Helpers                                                                   */
/******************************************************************************/
#define WIPE                                 (DK_BTN1_MSK)
#define OTHER                                (DK_BTN2_MSK)

/** The DK library reports a change: button state and which bits changed. */
static void sv_Buttons(uint32_t u32_state, uint32_t u32_changed)
{
   TEST_ASSERT_NOT_NULL_MESSAGE(sfpt_handler, "gi_ProvButton_Init() registers the handler");
   sfpt_handler(u32_state, u32_changed);
}

/** Let simulated time pass, running the work item when it falls due. */
static void sv_Wait(int64_t i64_ms)
{
   gi64_simNowMs += i64_ms;
   (void)gb_SimRunDelayedWork(&sst_wipeWork);
}

void setUp(void)
{
   sfpt_handler = NULL;
   si_dkInitRet = 0;
   su32_dkInits = 0U;
   su32_wipes = 0U;
   gi64_simNowMs = 0;
   (void)k_work_cancel_delayable(&sst_wipeWork);
   gv_SimLogClear();
   TEST_ASSERT_EQUAL_INT(0, gi_ProvButton_Init());
}

void tearDown(void) {}

/******************************************************************************/
/*  Init                                                                      */
/******************************************************************************/
static void test_InitRegistersWithTheDkLibrary(void)
{
   TEST_ASSERT_EQUAL_UINT32(1U, su32_dkInits);
   TEST_ASSERT_NOT_NULL(sfpt_handler);
}

static void test_InitErrorIsReturnedAndLogged(void)
{
   si_dkInitRet = -ENODEV;
   TEST_ASSERT_EQUAL_INT(-ENODEV, gi_ProvButton_Init());
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("dk_buttons_init failed (-"));
}

/******************************************************************************/
/*  Hold and release                                                          */
/******************************************************************************/
static void test_HoldForTheWholeTimeWipesOnce(void)
{
   sv_Buttons(WIPE, WIPE);
   sv_Wait(CONFIG_PROV_WIPE_HOLD_MS - 1);
   TEST_ASSERT_EQUAL_UINT32_MESSAGE(0U, su32_wipes, "not before the hold time");
   sv_Wait(1);
   TEST_ASSERT_EQUAL_UINT32(1U, su32_wipes);
   TEST_ASSERT_NOT_NULL(gcpt_SimLogFind("button held 5000 ms: wiping provisioning"));

   // Still held, then released: no second wipe
   sv_Wait(CONFIG_PROV_WIPE_HOLD_MS * 3);
   sv_Buttons(0U, WIPE);
   sv_Wait(CONFIG_PROV_WIPE_HOLD_MS);
   TEST_ASSERT_EQUAL_UINT32(1U, su32_wipes);
}

static void test_ReleaseBeforeTheHoldTimeCancels(void)
{
   sv_Buttons(WIPE, WIPE);
   sv_Wait(CONFIG_PROV_WIPE_HOLD_MS - 1);
   sv_Buttons(0U, WIPE);
   TEST_ASSERT_FALSE(k_work_delayable_is_pending(&sst_wipeWork));
   sv_Wait(CONFIG_PROV_WIPE_HOLD_MS);
   TEST_ASSERT_EQUAL_UINT32(0U, su32_wipes);
}

static void test_EachPressStartsAFreshHold(void)
{
   // Two short presses do not add up
   sv_Buttons(WIPE, WIPE);
   sv_Wait(3000);
   sv_Buttons(0U, WIPE);
   sv_Buttons(WIPE, WIPE);
   sv_Wait(3000);
   TEST_ASSERT_EQUAL_UINT32(0U, su32_wipes);
   sv_Wait(CONFIG_PROV_WIPE_HOLD_MS - 3000);
   TEST_ASSERT_EQUAL_UINT32(1U, su32_wipes);
}

static void test_OtherButtonsAreIgnored(void)
{
   // Another button pressed and released: nothing scheduled
   sv_Buttons(OTHER, OTHER);
   TEST_ASSERT_FALSE(k_work_delayable_is_pending(&sst_wipeWork));
   sv_Buttons(0U, OTHER);

   // Another button changing while the wipe button is held does not cancel it
   sv_Buttons(WIPE, WIPE);
   sv_Wait(1000);
   sv_Buttons(WIPE | OTHER, OTHER);
   sv_Buttons(WIPE, OTHER);
   sv_Wait(CONFIG_PROV_WIPE_HOLD_MS - 1000);
   TEST_ASSERT_EQUAL_UINT32(1U, su32_wipes);
}

static void test_UnchangedWipeBitIsNotAPress(void)
{
   // The state shows the wipe button down, but it is not among the changes
   sv_Buttons(WIPE | OTHER, OTHER);
   TEST_ASSERT_FALSE(k_work_delayable_is_pending(&sst_wipeWork));
   sv_Wait(CONFIG_PROV_WIPE_HOLD_MS * 2);
   TEST_ASSERT_EQUAL_UINT32(0U, su32_wipes);
}

int main(void)
{
   (void)setvbuf(stdout, NULL, _IONBF, 0);
   UNITY_BEGIN();
   RUN_TEST(test_InitRegistersWithTheDkLibrary);
   RUN_TEST(test_InitErrorIsReturnedAndLogged);
   RUN_TEST(test_HoldForTheWholeTimeWipesOnce);
   RUN_TEST(test_ReleaseBeforeTheHoldTimeCancels);
   RUN_TEST(test_EachPressStartsAFreshHold);
   RUN_TEST(test_OtherButtonsAreIgnored);
   RUN_TEST(test_UnchangedWipeBitIsNotAPress);
   return UNITY_END();
}
