#import <Foundation/Foundation.h>
#import <stdio.h>
#import "TaskList.h"

static int gFailures = 0;

static void expectTrue(BOOL cond, const char *what) {
	if (!cond) {
		fprintf(stderr, "FAIL %s\n", what);
		gFailures++;
	}
}

static NSString *applyEdit(NSString *line, NVTaskEdit edit) {
	if (edit.range.location == NSNotFound)
		return line;
	NSMutableString *edited = [NSMutableString stringWithString:line];
	[edited replaceCharactersInRange:edit.range withString:edit.replacement ?: @""];
	return edited;
}

static void expectShortcut(NSString *line, NSString *want, const char *what) {
	NSString *got = applyEdit(line, NVTaskShortcutEdit(line));
	expectTrue([got isEqualToString:want], what);
	if (![got isEqualToString:want])
		fprintf(stderr, "  got \"%s\" want \"%s\"\n", [got UTF8String], [want UTF8String]);
}

static void expectNoToggle(NSString *line, NSUInteger index, const char *what) {
	expectTrue(NVTaskToggleEdit(line, index).range.location == NSNotFound, what);
}

int main(void) {
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	expectShortcut(@"milk", @"- [ ] milk", "a plain line becomes a task");
	expectShortcut(@"  milk", @"  - [ ] milk", "indent stays in front of a new task");
	expectShortcut(@"", @"- [ ] ", "a blank line becomes an empty task");
	expectShortcut(@"- milk", @"- [ ] milk", "a bullet gains a checkbox after its marker");
	expectShortcut(@"*  milk", @"* [ ] milk", "extra spaces after a bullet collapse");
	expectShortcut(@"+ milk", @"+ [ ] milk", "a plus bullet gains a checkbox");
	expectShortcut(@"1. milk", @"1. [ ] milk", "a numbered item gains a checkbox");
	expectShortcut(@"1) milk", @"1) [ ] milk", "a parenthesis number gains a checkbox");
	expectShortcut(@"- [ ] milk", @"- milk", "an open task drops the checkbox");
	expectShortcut(@"- [x] milk", @"- milk", "a completed task drops the checkbox");
	expectShortcut(@"- [X] milk", @"- milk", "an uppercase completed task drops the checkbox");
	expectShortcut(@"- [ ]", @"- ", "an empty task keeps its list marker");
	expectShortcut(@"1) [X] Done", @"1) Done", "a completed numbered task drops the checkbox");

	NVTaskEdit open = NVTaskToggleEdit(@"- [ ] milk", 3);
	expectTrue(open.range.location == 3 && open.range.length == 1 && [open.replacement isEqualToString:@"x"], "clicking the open box marks it done");
	NVTaskEdit done = NVTaskToggleEdit(@"- [x] milk", 3);
	expectTrue(done.range.location == 3 && done.range.length == 1 && [done.replacement isEqualToString:@" "], "clicking a completed box clears it");
	NVTaskEdit upper = NVTaskToggleEdit(@"- [X] milk", 3);
	expectTrue(upper.range.location == 3 && [upper.replacement isEqualToString:@" "], "clicking an uppercase box clears it");
	expectNoToggle(@"- [ ] milk", 2, "clicking the opening bracket is not the checkbox");
	expectNoToggle(@"see [x] here", 5, "a bracket pair that is not a task is left alone");

	NSRange struck = NVTaskDoneStrikethroughRange(@"- [x] milk");
	expectTrue(struck.location == 6 && struck.length == 4, "completed task text is struck");
	expectTrue(NVTaskDoneStrikethroughRange(@"- [ ] milk").location == NSNotFound, "an open task is not struck");
	NSRange withDone = NVTaskDoneStrikethroughRange(@"- [x] milk @done");
	expectTrue(withDone.location == 6 && withDone.length == 4, "the @done tag stays outside the strike");
	NSRange prefix = NVTaskCheckedPrefixRange(@"- [x] milk");
	expectTrue(prefix.location == 0 && prefix.length == 6, "the marker and checkbox stay outside the strike");
	expectTrue(NVTaskCheckedPrefixRange(@"- [ ] milk").location == NSNotFound, "an open task has no checked prefix");

	NSString *fenced = @"before\n```\n- [ ] inside\n```\n- milk";
	NSRange inside = [fenced rangeOfString:@"- [ ] inside"];
	NSRange after = [fenced rangeOfString:@"- milk"];
	NSRange fence = [fenced rangeOfString:@"```"];
	expectTrue(NVTaskIndexIsInsideFence(fenced, inside.location), "a line between fences is inside a code block");
	expectTrue(NVTaskIndexIsInsideFence(fenced, fence.location), "a fence line is inside a code block");
	expectTrue(!NVTaskIndexIsInsideFence(fenced, after.location), "a line after the closing fence is editable");
	expectTrue(!NVTaskIndexIsInsideFence(fenced, 0), "a line before a fence is editable");

	expectTrue([NVTaskContinuationSuffix(@"- [ ] milk") isEqualToString:@"[ ] "], "return after an open task starts another open task");
	expectTrue([NVTaskContinuationSuffix(@"- [x] milk") isEqualToString:@"[ ] "], "return after a completed task starts an open task");
	expectTrue([NVTaskContinuationSuffix(@"1. [X] Done") isEqualToString:@"[ ] "], "return after a numbered task starts an open task");
	expectTrue(NVTaskContinuationSuffix(@"- [ ] ") == nil, "an empty task does not continue");
	expectTrue(NVTaskContinuationSuffix(@"- milk") == nil, "a plain bullet does not gain a task suffix");
	expectTrue(NVTaskLineIsEmptyItem(@"- [ ]"), "a checkbox with no text is an empty task");
	expectTrue(NVTaskLineIsEmptyItem(@"- [x] "), "a completed checkbox with no text is an empty task");
	expectTrue(NVTaskLineIsEmptyItem(@"  1. [ ] "), "an indented empty numbered task is empty");
	expectTrue(!NVTaskLineIsEmptyItem(@"- [ ] milk"), "a task with text is not empty");
	expectTrue(!NVTaskLineIsEmptyItem(@"- "), "a plain bullet is not an empty task");

	expectTrue(NVTaskCursorBelongsToCheckbox(@"- [ ] ", 6), "the cursor after an empty checkbox belongs to it");
	expectTrue(NVTaskCursorBelongsToCheckbox(@"- [ ]", 4), "the cursor on the checkbox closer belongs to it");
	expectTrue(NVTaskCursorBelongsToCheckbox(@"- [x] ", 4), "the cursor inside a completed checkbox belongs to it");
	expectTrue(!NVTaskCursorBelongsToCheckbox(@"- [ ] milk", 8), "the cursor in the task text is not the checkbox");
	expectTrue(!NVTaskCursorBelongsToCheckbox(@"[link]", 6), "a link bracket is not a task checkbox");

	if (!gFailures)
		printf("test-ok\n");
	[pool drain];
	return gFailures ? 1 : 0;
}
