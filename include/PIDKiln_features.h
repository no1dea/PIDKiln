#pragma once

/*
** Compile-time feature flags — every PK_FEATURE_* must be set by the PlatformIO env (0 or 1).
** Missing macros fail closed. Illegal combos fail at compile time.
*/

#ifndef PK_FEATURE_LOCAL_UI
#error "PK_FEATURE_LOCAL_UI must be set by the build env (0 or 1)"
#endif
#ifndef PK_FEATURE_WIFI
#error "PK_FEATURE_WIFI must be set by the build env (0 or 1)"
#endif
#ifndef PK_FEATURE_HTTP
#error "PK_FEATURE_HTTP must be set by the build env (0 or 1)"
#endif
#ifndef PK_FEATURE_LOGS
#error "PK_FEATURE_LOGS must be set by the build env (0 or 1)"
#endif
#ifndef PK_FEATURE_EMR
#error "PK_FEATURE_EMR must be set by the build env (0 or 1)"
#endif
#ifndef PK_FEATURE_ALARM
#error "PK_FEATURE_ALARM must be set by the build env (0 or 1)"
#endif
#ifndef PK_FEATURE_HOUSING_TC
#error "PK_FEATURE_HOUSING_TC must be set by the build env (0 or 1)"
#endif
#ifndef PK_FEATURE_SSR2
#error "PK_FEATURE_SSR2 must be set by the build env (0 or 1)"
#endif
#ifndef PK_FEATURE_ENERGY
#error "PK_FEATURE_ENERGY must be set by the build env (0 or 1)"
#endif
#ifndef PK_FEATURE_SYSLOG
#error "PK_FEATURE_SYSLOG must be set by the build env (0 or 1)"
#endif

#if PK_FEATURE_HTTP && !PK_FEATURE_WIFI
#error "PK_FEATURE_HTTP requires PK_FEATURE_WIFI"
#endif
#if PK_FEATURE_SYSLOG && !PK_FEATURE_WIFI
#error "PK_FEATURE_SYSLOG requires PK_FEATURE_WIFI"
#endif

/* Pin map — only defined when the matching feature is enabled */
#define SSR1_RELAY_PIN 25
#define MAXCS1 27

#if PK_FEATURE_EMR
#define EMR_RELAY_PIN 21
#endif

#if PK_FEATURE_SSR2
#define SSR2_RELAY_PIN 22
#endif

#if PK_FEATURE_HOUSING_TC
#define MAXCS2 15
#endif

#if PK_FEATURE_ENERGY
#define ENERGY_MON_PIN 33
#endif

#if PK_FEATURE_ALARM
#define ALARM_PIN 26
#endif

#define SCREEN_W 128
#define SCREEN_H 64
#define MAX_CHARS_PL (SCREEN_W / 3)

#if PK_FEATURE_LOCAL_UI
#define ENCODER0_PINA 34
#define ENCODER0_PINB 35
#define ENCODER0_BUTTON 32
#define ENCODER_BUTTON_DELAY 150
#define ENCODER_ROTATE_DELAY 120
#endif
