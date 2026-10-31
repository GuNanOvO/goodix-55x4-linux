// Test the actual activation callbacks without USB or a sensor.
#define goodix_send_preset_psk_read test_read_psk
#define fpi_ssm_mark_failed test_ssm_failed
#define fpi_ssm_next_state test_ssm_next
#include "../libfprint/drivers/goodixtls/goodix55x4.c"

static GError *failure;
static guint reads, advances;
static GoodixPresetPskReadCallback read_callback;
static gpointer read_data;

void test_ssm_failed(FpiSsm *ssm, GError *error)
{
  g_assert_null(failure);
  failure = error;
}

void test_ssm_next(FpiSsm *ssm) { advances++; }

void test_read_psk(FpDevice *dev, guint32 flags, guint16 length,
                   GoodixPresetPskReadCallback callback, gpointer user_data)
{
  g_assert_cmphex(flags, ==, GOODIX_55X4_PSK_FLAGS);
  g_assert_cmpuint(length, ==, 32);
  reads++;
  read_callback = callback;
  read_data = user_data;
}

static void reset(void)
{
  g_clear_error(&failure);
  reads = advances = 0;
  read_callback = NULL;
  read_data = NULL;
}

static void start_timeout(void)
{
  reset();
  check_psk_write(NULL, FALSE, GINT_TO_POINTER(1),
      g_error_new_literal(G_IO_ERROR, G_IO_ERROR_TIMED_OUT, "write timeout"));
  g_assert_cmpuint(reads, ==, 1);
  g_assert_cmpuint(advances, ==, 0);
  g_assert_null(failure);
  g_assert_nonnull(read_callback);
  g_assert_true(read_data == GINT_TO_POINTER(1));
}

static void assert_stopped(const gchar *message)
{
  g_assert_error(failure, G_IO_ERROR, G_IO_ERROR_TIMED_OUT);
  g_assert_nonnull(strstr(failure->message, message));
  g_assert_cmpuint(reads, ==, 1);
  g_assert_cmpuint(advances, ==, 0);
  g_clear_error(&failure);
}

static void readback_match(void)
{
  start_timeout();
  read_callback(NULL, TRUE, GOODIX_55X4_PSK_FLAGS,
      (guint8 *)goodix_55x4_psk_0, sizeof(goodix_55x4_psk_0), read_data, NULL);
  assert_stopped("hash matches expected value");
}

static void readback_mismatch(void)
{
  guint8 different[32] = {0};
  start_timeout();
  read_callback(NULL, TRUE, GOODIX_55X4_PSK_FLAGS,
                different, sizeof(different), read_data, NULL);
  assert_stopped("hash does not match expected value");
}

static void readback_error(void)
{
  start_timeout();
  read_callback(NULL, FALSE, 0, NULL, 0, read_data,
      g_error_new_literal(G_IO_ERROR, G_IO_ERROR_TIMED_OUT, "read timeout"));
  assert_stopped("readback failed: read timeout");
}

static void invalid_readback(gconstpointer variant)
{
  start_timeout();
  read_callback(NULL, GPOINTER_TO_INT(variant) != 0,
      GPOINTER_TO_INT(variant) == 1 ? 0 : GOODIX_55X4_PSK_FLAGS,
      GPOINTER_TO_INT(variant) == 3 ? NULL : (guint8 *)goodix_55x4_psk_0,
      GPOINTER_TO_INT(variant) == 2 ? 0 : sizeof(goodix_55x4_psk_0),
      read_data, NULL);
  assert_stopped("readback invalid");
}

static void normal_result(gconstpointer variant)
{
  gint result = GPOINTER_TO_INT(variant);
  reset();
  check_psk_write(NULL, result == 0, NULL,
      result >= 2 ? g_error_new_literal(G_IO_ERROR,
          result == 2 ? G_IO_ERROR_CANCELLED : G_IO_ERROR_FAILED,
          "original failure") : NULL);
  g_assert_cmpuint(reads, ==, 0);
  g_assert_cmpuint(advances, ==, result == 0 ? 1 : 0);
  if (result == 0)
    g_assert_no_error(failure);
  else
    g_assert_error(failure, G_IO_ERROR,
                  result == 2 ? G_IO_ERROR_CANCELLED : G_IO_ERROR_FAILED);
  g_clear_error(&failure);
}

int main(int argc, char **argv)
{
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/goodix-psk/timeout/match", readback_match);
  g_test_add_func("/goodix-psk/timeout/mismatch", readback_mismatch);
  g_test_add_func("/goodix-psk/timeout/read-error", readback_error);
  g_test_add_data_func("/goodix-psk/timeout/rejected", GINT_TO_POINTER(0), invalid_readback);
  g_test_add_data_func("/goodix-psk/timeout/bad-flags", GINT_TO_POINTER(1), invalid_readback);
  g_test_add_data_func("/goodix-psk/timeout/bad-length", GINT_TO_POINTER(2), invalid_readback);
  g_test_add_data_func("/goodix-psk/timeout/null-hash", GINT_TO_POINTER(3), invalid_readback);
  g_test_add_data_func("/goodix-psk/success", GINT_TO_POINTER(0), normal_result);
  g_test_add_data_func("/goodix-psk/rejected", GINT_TO_POINTER(1), normal_result);
  g_test_add_data_func("/goodix-psk/cancelled", GINT_TO_POINTER(2), normal_result);
  g_test_add_data_func("/goodix-psk/other-error", GINT_TO_POINTER(3), normal_result);
  return g_test_run();
}
