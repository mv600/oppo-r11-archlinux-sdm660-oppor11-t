/*
 * R11T OSM power-profiles-daemon platform driver.
 *
 * The R11T kernel exposes two cpufreq policies backed by the qcom_r11t_osm
 * driver. PPD already owns the D-Bus profile state, so this driver applies
 * the same policy caps previously handled by r11t-ppd-osm-watch.sh.
 */

#define G_LOG_DOMAIN "R11tOsmDriver"

#include "ppd-driver-r11t-osm.h"

#include <errno.h>

#include "ppd-utils.h"

#define CPUFREQ_POLICY_DIR "/sys/devices/system/cpu/cpufreq/"
#define R11T_OSM_MODULE_PATH "/sys/module/qcom_r11t_osm_cpufreq"
#define R11T_PWRCL_POWER_SAVER_KHZ 1401600ULL
#define R11T_PERFCL_POWER_SAVER_KHZ 1747200ULL

struct _PpdDriverR11tOsm
{
  PpdDriverPlatform parent_instance;

  gchar *pwrcl_policy;
  gchar *perfcl_policy;
};

G_DEFINE_TYPE (PpdDriverR11tOsm, ppd_driver_r11t_osm, PPD_TYPE_DRIVER_PLATFORM)

static GObject *
ppd_driver_r11t_osm_constructor (GType                  type,
                                 guint                  n_construct_params,
                                 GObjectConstructParam *construct_params)
{
  GObject *object;

  object = G_OBJECT_CLASS (ppd_driver_r11t_osm_parent_class)->constructor (type,
                                                                          n_construct_params,
                                                                          construct_params);
  g_object_set (object,
                "driver-name", R11T_OSM_DRIVER_NAME,
                "profiles", PPD_PROFILE_POWER_SAVER | PPD_PROFILE_BALANCED,
                NULL);

  return object;
}

static gboolean
related_cpus_contains (const gchar *related_cpus,
                       guint        cpu)
{
  g_auto(GStrv) cpus = NULL;

  if (related_cpus == NULL)
    return FALSE;

  cpus = g_strsplit_set (related_cpus, " \t\n", -1);
  for (guint i = 0; cpus != NULL && cpus[i] != NULL; i++) {
    gchar *end = NULL;
    guint64 value;

    if (*cpus[i] == '\0')
      continue;

    errno = 0;
    value = g_ascii_strtoull (cpus[i], &end, 10);
    if (end != cpus[i] && *end == '\0' && errno == 0 && value == cpu)
      return TRUE;
  }

  return FALSE;
}

static gboolean
find_policies (PpdDriverR11tOsm  *self,
               GError           **error)
{
  g_autofree gchar *policy_dir = NULL;
  g_autoptr(GDir) dir = NULL;
  const gchar *name;

  policy_dir = ppd_utils_get_sysfs_path (CPUFREQ_POLICY_DIR);
  dir = g_dir_open (policy_dir, 0, error);
  if (dir == NULL)
    return FALSE;

  while ((name = g_dir_read_name (dir)) != NULL) {
    g_autofree gchar *related_path = NULL;
    g_autofree gchar *related_cpus = NULL;
    g_autofree gchar *policy_path = NULL;

    if (!g_str_has_prefix (name, "policy"))
      continue;

    related_path = g_build_filename (policy_dir, name, "related_cpus", NULL);
    if (!g_file_get_contents (related_path, &related_cpus, NULL, NULL))
      continue;
    g_strstrip (related_cpus);

    policy_path = g_build_filename (policy_dir, name, NULL);
    if (related_cpus_contains (related_cpus, 0)) {
      g_free (self->pwrcl_policy);
      self->pwrcl_policy = g_steal_pointer (&policy_path);
    } else if (self->perfcl_policy == NULL) {
      self->perfcl_policy = g_steal_pointer (&policy_path);
    }
  }

  if (self->pwrcl_policy == NULL || self->perfcl_policy == NULL) {
    g_set_error (error,
                 G_IO_ERROR,
                 G_IO_ERROR_NOT_FOUND,
                 "Could not find both R11T cpufreq policies in %s",
                 policy_dir);
    return FALSE;
  }

  return TRUE;
}

static gboolean
read_policy_value (const gchar  *policy,
                   const gchar  *attribute,
                   guint64      *value,
                   GError      **error)
{
  g_autofree gchar *path = NULL;
  g_autofree gchar *contents = NULL;
  gchar *end = NULL;
  guint64 parsed;

  path = g_build_filename (policy, attribute, NULL);
  if (!g_file_get_contents (path, &contents, NULL, error))
    return FALSE;

  g_strchomp (contents);
  errno = 0;
  parsed = g_ascii_strtoull (contents, &end, 10);
  if (end == contents || *end != '\0' || errno != 0) {
    g_set_error (error,
                 G_IO_ERROR,
                 G_IO_ERROR_INVALID_DATA,
                 "Invalid value in %s",
                 path);
    return FALSE;
  }

  *value = parsed;
  return TRUE;
}

static gboolean
write_policy_value (const gchar *policy,
                    const gchar *attribute,
                    guint64      value,
                    GError     **error)
{
  g_autofree gchar *path = NULL;
  g_autofree gchar *text = NULL;

  path = g_build_filename (policy, attribute, NULL);
  text = g_strdup_printf ("%" G_GUINT64_FORMAT, value);

  return ppd_utils_write (path, text, error);
}

static gboolean
restore_policy_value (const gchar *policy,
                      guint64      value)
{
  g_autoptr(GError) error = NULL;

  if (!write_policy_value (policy, "scaling_max_freq", value, &error)) {
    g_warning ("Failed to restore %s/scaling_max_freq to %" G_GUINT64_FORMAT ": %s",
               policy,
               value,
               error->message);
    return FALSE;
  }

  return TRUE;
}

static gboolean
ppd_driver_r11t_osm_activate_profile (PpdDriver                   *driver,
                                      PpdProfile                   profile,
                                      PpdProfileActivationReason   reason,
                                      GError                     **error)
{
  PpdDriverR11tOsm *self = PPD_DRIVER_R11T_OSM (driver);
  guint64 pwrcl_target;
  guint64 perfcl_target;
  guint64 pwrcl_previous;
  guint64 perfcl_previous;
  guint64 pwrcl_restore;

  if (self->pwrcl_policy == NULL || self->perfcl_policy == NULL) {
    if (!find_policies (self, error))
      return FALSE;
  }

  if (!read_policy_value (self->pwrcl_policy,
                          "scaling_max_freq",
                          &pwrcl_restore,
                          error))
    return FALSE;
  if (!read_policy_value (self->pwrcl_policy,
                          "cpuinfo_max_freq",
                          &pwrcl_previous,
                          error))
    return FALSE;
  if (!read_policy_value (self->perfcl_policy,
                          "cpuinfo_max_freq",
                          &perfcl_previous,
                          error))
    return FALSE;

  switch (profile) {
  case PPD_PROFILE_POWER_SAVER:
    pwrcl_target = R11T_PWRCL_POWER_SAVER_KHZ;
    perfcl_target = R11T_PERFCL_POWER_SAVER_KHZ;
    break;
  case PPD_PROFILE_BALANCED:
    pwrcl_target = pwrcl_previous;
    perfcl_target = perfcl_previous;
    break;
  default:
    g_set_error (error,
                 G_IO_ERROR,
                 G_IO_ERROR_NOT_SUPPORTED,
                 "R11T OSM driver does not support profile '%s'",
                 ppd_profile_to_str (profile));
    return FALSE;
  }

  if (!write_policy_value (self->pwrcl_policy,
                           "scaling_max_freq",
                           pwrcl_target,
                           error))
    return FALSE;

  if (!write_policy_value (self->perfcl_policy,
                           "scaling_max_freq",
                           perfcl_target,
                           error)) {
    restore_policy_value (self->pwrcl_policy, pwrcl_restore);
    return FALSE;
  }

  g_info ("Applied profile '%s' (reason: %s): pwrcl=%" G_GUINT64_FORMAT
          " kHz, perfcl=%" G_GUINT64_FORMAT " kHz",
          ppd_profile_to_str (profile),
          ppd_profile_activation_reason_to_str (reason),
          pwrcl_target,
          perfcl_target);

  return TRUE;
}

static PpdProbeResult
ppd_driver_r11t_osm_probe (PpdDriver *driver)
{
  PpdDriverR11tOsm *self = PPD_DRIVER_R11T_OSM (driver);
  g_autofree gchar *module_path = NULL;
  g_autoptr(GError) error = NULL;

  module_path = ppd_utils_get_sysfs_path (R11T_OSM_MODULE_PATH);
  if (!g_file_test (module_path, G_FILE_TEST_EXISTS)) {
    g_debug ("R11T OSM module path %s is absent", module_path);
    return PPD_PROBE_RESULT_FAIL;
  }

  if (!find_policies (self, &error)) {
    g_debug ("R11T OSM cpufreq policies are unavailable: %s", error->message);
    return PPD_PROBE_RESULT_FAIL;
  }

  return PPD_PROBE_RESULT_SUCCESS;
}

static void
ppd_driver_r11t_osm_finalize (GObject *object)
{
  PpdDriverR11tOsm *self = PPD_DRIVER_R11T_OSM (object);

  g_clear_pointer (&self->pwrcl_policy, g_free);
  g_clear_pointer (&self->perfcl_policy, g_free);

  G_OBJECT_CLASS (ppd_driver_r11t_osm_parent_class)->finalize (object);
}

static void
ppd_driver_r11t_osm_class_init (PpdDriverR11tOsmClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  PpdDriverClass *driver_class = PPD_DRIVER_CLASS (klass);

  object_class->constructor = ppd_driver_r11t_osm_constructor;
  object_class->finalize = ppd_driver_r11t_osm_finalize;

  driver_class->probe = ppd_driver_r11t_osm_probe;
  driver_class->activate_profile = ppd_driver_r11t_osm_activate_profile;
}

static void
ppd_driver_r11t_osm_init (PpdDriverR11tOsm *self)
{
  self->pwrcl_policy = NULL;
  self->perfcl_policy = NULL;
}
