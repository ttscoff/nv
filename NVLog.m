#import <Foundation/Foundation.h>
#import <os/log.h>
#import <stdarg.h>
#import <stdio.h>
#import <stdlib.h>
#import "NVLog.h"

static _Bool sWritesErrorsAndWarnings = 1;
static _Bool sWritesAllDiagnosticMessages = 0;
static NVLogSink sSink = NULL;

void NVLogSetEnabled(_Bool errorsAndWarnings, _Bool allDiagnosticMessages) {
	sWritesAllDiagnosticMessages = allDiagnosticMessages;
	sWritesErrorsAndWarnings = errorsAndWarnings || allDiagnosticMessages;
}

_Bool NVLogWritesErrorsAndWarnings(void) {
	return sWritesErrorsAndWarnings;
}

_Bool NVLogWritesAllDiagnosticMessages(void) {
	return sWritesAllDiagnosticMessages;
}

_Bool NVLogIsEnabled(NVLogLevel level) {
	return level == NVLogLevelInfo ? sWritesAllDiagnosticMessages : sWritesErrorsAndWarnings;
}

void NVLogSetSink(NVLogSink sink) {
	sSink = sink;
}

static os_log_t NVLogForLevel(NVLogLevel level) {
	static os_log_t logs[3];
	static dispatch_once_t once;
	dispatch_once(&once, ^{
		const char *subsystem = [[[NSBundle mainBundle] bundleIdentifier] UTF8String];
		if (!subsystem || !*subsystem)
			subsystem = "net.elasticthreads.nv";
		logs[NVLogLevelError] = os_log_create(subsystem, "errors");
		logs[NVLogLevelWarning] = os_log_create(subsystem, "warnings");
		logs[NVLogLevelInfo] = os_log_create(subsystem, "diagnostics");
	});
	if (level < NVLogLevelError || level > NVLogLevelInfo)
		level = NVLogLevelInfo;
	return logs[level];
}

/* The system log stamps each line itself, so a trailing newline from an old
   printf-style message is dropped. The text is public, as it was with NSLog. */
static void NVLogWrite(NVLogLevel level, NSString *message) {
	NSUInteger length = [message length];
	while (length > 0 && [message characterAtIndex:length - 1] == '\n')
		length--;
	if (length != [message length])
		message = [message substringToIndex:length];
	if (sSink) {
		sSink(level, message);
		return;
	}
	os_log_with_type(NVLogForLevel(level), level == NVLogLevelError ? OS_LOG_TYPE_ERROR : OS_LOG_TYPE_DEFAULT, "%{public}@", message);
}

void NVLogMessagev(NVLogLevel level, NSString *format, va_list arguments) {
	if (!format || !NVLogIsEnabled(level))
		return;
	@autoreleasepool {
		NSString *message = [[NSString alloc] initWithFormat:format arguments:arguments];
		NVLogWrite(level, message);
#if !__has_feature(objc_arc)
		[message release];
#endif
	}
}

void NVLogMessage(NVLogLevel level, NSString *format, ...) {
	va_list arguments;
	va_start(arguments, format);
	NVLogMessagev(level, format, arguments);
	va_end(arguments);
}

void NVLogC(NVLogLevel level, const char *format, ...) {
	if (!format || !NVLogIsEnabled(level))
		return;
	char *text = NULL;
	va_list arguments;
	va_start(arguments, format);
	int written = vasprintf(&text, format, arguments);
	va_end(arguments);
	if (written < 0 || !text)
		return;
	@autoreleasepool {
		/* Old printf-style messages carry file names from the C APIs, which
		   are UTF-8 on this platform; anything else is read as Latin-1 so no
		   message is lost. */
		NSString *message = [NSString stringWithUTF8String:text];
		if (!message)
			message = [NSString stringWithCString:text encoding:NSISOLatin1StringEncoding];
		NVLogWrite(level, message ?: @"");
	}
	free(text);
}
