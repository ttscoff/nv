#import <Foundation/Foundation.h>
#import <stdio.h>
#import <string.h>
#import "BufferUtils.h"

static int gFailures = 0;

static void expectTrue(BOOL cond, const char *what) {
	if (!cond) {
		fprintf(stderr, "FAIL %s\n", what);
		gFailures++;
	}
}

int main(int argc, const char *argv[]) {
	// A fresh buffer from a different string.
	char *s = replaceString(NULL, "mark");
	expectTrue(s && strcmp(s, "mark") == 0, "copy into a new buffer");

	// Growing and shrinking.
	s = replaceString(s, "markdownify");
	expectTrue(strcmp(s, "markdownify") == 0, "grow");
	s = replaceString(s, "m");
	expectTrue(strcmp(s, "m") == 0, "shrink");
	s = replaceString(s, "");
	expectTrue(strcmp(s, "") == 0, "empty");

	// refilterNotes hands the search string in force back as the new text. The buffer must keep
	// its contents, which fails when realloc moves the block and the copy then reads freed memory.
	s = replaceString(s, "mark");
	char *same = s;
	s = replaceString(s, s);
	expectTrue(s == same, "replacing a buffer with itself keeps the buffer");
	expectTrue(strcmp(s, "mark") == 0, "replacing a buffer with itself keeps its contents");
	s = replaceString(s, s);
	expectTrue(strcmp(s, "mark") == 0, "and again");

	// A different buffer with the same text is still copied.
	char *other = strdup("mark");
	s = replaceString(s, other);
	expectTrue(strcmp(s, "mark") == 0 && s != other, "same text from another buffer");
	free(other);
	free(s);

	if (gFailures) {
		fprintf(stderr, "%d replace string test(s) failed\n", gFailures);
		return 1;
	}
	printf("replace string tests passed\n");
	return 0;
}
