#import <Foundation/Foundation.h>
#import <stdio.h>
#import "NVLog.h"

static int gFailures = 0;

static void expectTrue(BOOL cond, const char *what) {
	if (!cond) {
		fprintf(stderr, "FAIL %s\n", what);
		gFailures++;
	}
}

static NSMutableArray *gReceived;

static void collect(NVLogLevel level, NSString *message) {
	[gReceived addObject:[NSString stringWithFormat:@"%d:%@", (int)level, message]];
}

static int gSideEffects = 0;
static int sideEffect(void) {
	gSideEffects++;
	return 42;
}

static void logEverything(void) {
	NVLogError(@"error %d", 1);
	NVLogWarning(@"warning %d", 2);
	NVLogInfo(@"info %d", 3);
	NVLogErrorC("c error %d", 4);
	NVLogWarningC("c warning %d", 5);
	NVLogInfoC("c info %d", 6);
}

static NSString *joined(void) {
	return [gReceived componentsJoinedByString:@"|"];
}

int main(int argc, const char *argv[]) {
	@autoreleasepool {
		gReceived = [NSMutableArray array];
		NVLogSetSink(collect);

		// Before any preference is applied, errors and warnings are written and the rest is not.
		expectTrue(NVLogWritesErrorsAndWarnings() && !NVLogWritesAllDiagnosticMessages(), "default: errors and warnings only");
		expectTrue(NVLogIsEnabled(NVLogLevelError) && NVLogIsEnabled(NVLogLevelWarning) && !NVLogIsEnabled(NVLogLevelInfo), "default: level checks");
		logEverything();
		expectTrue([joined() isEqualToString:@"0:error 1|1:warning 2|0:c error 4|1:c warning 5"], "default: only errors and warnings reach the log");

		// Both off: nothing at all.
		[gReceived removeAllObjects];
		NVLogSetEnabled(false, false);
		expectTrue(!NVLogIsEnabled(NVLogLevelError) && !NVLogIsEnabled(NVLogLevelWarning) && !NVLogIsEnabled(NVLogLevelInfo), "both off: nothing enabled");
		logEverything();
		expectTrue([gReceived count] == 0, "both off: nothing written");

		// All diagnostic messages include the errors and warnings, even if the other flag was passed as off.
		[gReceived removeAllObjects];
		NVLogSetEnabled(false, true);
		expectTrue(NVLogWritesErrorsAndWarnings() && NVLogWritesAllDiagnosticMessages(), "all implies errors and warnings");
		logEverything();
		expectTrue([joined() isEqualToString:@"0:error 1|1:warning 2|2:info 3|0:c error 4|1:c warning 5|2:c info 6"], "all: everything written in order");

		// Errors and warnings alone.
		[gReceived removeAllObjects];
		NVLogSetEnabled(true, false);
		logEverything();
		expectTrue([joined() isEqualToString:@"0:error 1|1:warning 2|0:c error 4|1:c warning 5"], "errors and warnings: info left out");

		// A message that is not written never evaluates its arguments.
		gSideEffects = 0;
		NVLogInfo(@"%d", sideEffect());
		expectTrue(gSideEffects == 0, "disabled tier does not evaluate arguments");
		NVLogError(@"%d", sideEffect());
		expectTrue(gSideEffects == 1, "enabled tier evaluates arguments once");

		// Formatting goes through NSString, so %@ works, and trailing newlines from old printf calls are dropped.
		[gReceived removeAllObjects];
		NVLogError(@"note %@ has %lu bytes\n", @"Title", (unsigned long)12);
		NVLogErrorC("file %s: %d\n\n", "a.txt", 7);
		expectTrue([joined() isEqualToString:@"0:note Title has 12 bytes|0:file a.txt: 7"], "formatting and newline trimming");

		// Odd input must not crash.
		[gReceived removeAllObjects];
		NVLogMessage(NVLogLevelError, nil);
		NVLogC(NVLogLevelError, NULL);
		NVLogError(@"");
		expectTrue([joined() isEqualToString:@"0:"], "nil formats are ignored, an empty message still goes through");

		// Levels outside the known range are written as diagnostics, not dropped on the floor.
		[gReceived removeAllObjects];
		NVLogSetEnabled(true, true);
		NVLogMessage((NVLogLevel)7, @"odd level");
		expectTrue([joined() isEqualToString:@"7:odd level"], "unknown level is passed through to the sink");

		// With no sink the system log is used; this must simply not crash.
		NVLogSetSink(NULL);
		NVLogError(@"system log smoke test %d", 1);
		NVLogInfoC("system log smoke test %d", 2);
		NVLogSetEnabled(true, false);

		if (gFailures) {
			fprintf(stderr, "%d log test(s) failed\n", gFailures);
			return 1;
		}
		printf("log tests passed\n");
		return 0;
	}
}
