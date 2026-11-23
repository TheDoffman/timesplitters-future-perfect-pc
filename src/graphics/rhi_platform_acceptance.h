#ifndef TSFP_RHI_PLATFORM_ACCEPTANCE_H
#define TSFP_RHI_PLATFORM_ACCEPTANCE_H
/* Platform acceptance, not a player preference. See opengl-recovery.md for
 * hardware/build provenance and remaining native-host checks. A test fixture
 * may explicitly exercise either gate without changing production defaults. */
#ifndef TSFP_OPENGL_RECOVERY_ACCEPTED
#if defined(_WIN32)
#define TSFP_OPENGL_RECOVERY_ACCEPTED 1
#else
/* Linux/WSL fullscreen/performance and native Mac acceptance remain pending. */
#define TSFP_OPENGL_RECOVERY_ACCEPTED 0
#endif
#endif
#endif
