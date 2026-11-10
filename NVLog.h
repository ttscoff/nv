/* NVLog: the app's logging module. Messages go to the unified system log under
   the app's bundle identifier, in the categories "errors", "warnings" and
   "diagnostics". Two settings decide what is written: errors and warnings, or
   every diagnostic message. Nothing is written with both off.

   This header is included from the prefix header, so it is also seen by plain
   C sources; the NSString entry points are Objective-C only. */

#ifndef NVLOG_H
#define NVLOG_H

/* No system includes here: this header rides along in the precompiled prefix,
   and pulling the C library's builtin modules into it makes clang see them
   twice. _Bool needs no header, and va_list is only used by the Objective-C
   entry point, where Foundation has already supplied it. */

typedef enum NVLogLevel {
	NVLogLevelError = 0,
	NVLogLevelWarning = 1,
	NVLogLevelInfo = 2
} NVLogLevel;

#ifdef __cplusplus
extern "C" {
#endif

/* Which tiers are written. Every diagnostic message includes the errors and
   warnings, so allDiagnosticMessages implies errorsAndWarnings. Until the
   preferences load, errors and warnings are written and the rest is not. */
void NVLogSetEnabled(_Bool errorsAndWarnings, _Bool allDiagnosticMessages);
_Bool NVLogWritesErrorsAndWarnings(void);
_Bool NVLogWritesAllDiagnosticMessages(void);
_Bool NVLogIsEnabled(NVLogLevel level);

/* For C sources: a printf-style message. */
void NVLogC(NVLogLevel level, const char *format, ...) __attribute__((format(printf, 2, 3)));

#ifdef __OBJC__
void NVLogMessage(NVLogLevel level, NSString *format, ...) NS_FORMAT_FUNCTION(2, 3);
void NVLogMessagev(NVLogLevel level, NSString *format, va_list arguments) NS_FORMAT_FUNCTION(2, 0);

/* Tests replace the system log with a sink; NULL puts the system log back. */
typedef void (*NVLogSink)(NVLogLevel level, NSString *message);
void NVLogSetSink(NVLogSink sink);
#endif

#ifdef __cplusplus
}
#endif

/* The macros test the setting first, so a message that is not written costs
   one comparison and its arguments are never evaluated. */
#define NVLogError(...)   do { if (NVLogIsEnabled(NVLogLevelError))   NVLogMessage(NVLogLevelError,   __VA_ARGS__); } while (0)
#define NVLogWarning(...) do { if (NVLogIsEnabled(NVLogLevelWarning)) NVLogMessage(NVLogLevelWarning, __VA_ARGS__); } while (0)
#define NVLogInfo(...)    do { if (NVLogIsEnabled(NVLogLevelInfo))    NVLogMessage(NVLogLevelInfo,    __VA_ARGS__); } while (0)

#define NVLogErrorC(...)   do { if (NVLogIsEnabled(NVLogLevelError))   NVLogC(NVLogLevelError,   __VA_ARGS__); } while (0)
#define NVLogWarningC(...) do { if (NVLogIsEnabled(NVLogLevelWarning)) NVLogC(NVLogLevelWarning, __VA_ARGS__); } while (0)
#define NVLogInfoC(...)    do { if (NVLogIsEnabled(NVLogLevelInfo))    NVLogC(NVLogLevelInfo,    __VA_ARGS__); } while (0)

#endif /* NVLOG_H */
