# Emoji Autocorrect Test

This file tests the emoji autocorrect functionality with various formatting and spelling errors.

## Uppercase Tests

These should be normalized to lowercase:
- :Smile: should become :smile:
- :ROCKET: should become :rocket:
- :Heart: should become :heart:
- :STAR: should become :star:
- :Fire: should become :fire:

## Hyphen Tests

These should have hyphens converted to underscores:
- :smile-face: should become :smile: (closest match, :smile_face: doesn't exist)
- :thumbs-up: should become :thumbsup:
- :check-flag: should become :checkered_flag:
- :heart-eyes: should become :heart_eyes:
- :fire-engine: should become :fire_engine:

## Spelling Error Tests

These should be corrected via fuzzy matching:
- :smil: should become :smile:
- :rockt: should become :rocket:
- :hart: should become :heart:
- :fir: should become :fire:
- :thum: should become :thumbsup: (shortest match)
- :starr: should become :star:
- :smily: should become :smiley:
- :rocket-ship: should become :rocket: (hyphen removed, closest match)

## Combined Errors

These have multiple issues that should all be corrected:
- :Smile-Face: should become :smile: (uppercase + hyphen fixed, closest match)
- :ROCKET-SHIP: should become :rocket: (uppercase + hyphen fixed)
- :Heart-Eyes: should become :heart_eyes: (uppercase + hyphen fixed)
- :Thumbs-Up: should become :thumbsup: (uppercase + hyphen fixed)
- :Fire-Engine: should become :fire_engine: (uppercase + hyphen fixed)

## Edge Cases

- :smile: (already correct, should remain unchanged)
- :smile::rocket: (multiple emojis)
- :smile: and :rocket: in a sentence.
- :smile: :rocket: :heart: (multiple with spaces)

## Common Typos

- :smileing: should become :smile: (typo correction)
- :rockets: should become :rocket: (plural to singular)
- :hearts: should become :hearts: (already correct, but different from :heart:)
- :fired: should become :fire: (past tense to base)
- :smiling: should become :smile: (closest match)

## Image-Based Emojis

These are image-based emojis that should also work with autocorrect:
- :bowtie: (image emoji)
- :octocat: (image emoji)
- :feelsgood: (image emoji)
- :Bowtie: should become :bowtie:
- :OctoCat: should become :octocat:
- :feels-good: should become :feelsgood:

## No Match Cases

These should remain unchanged (no close match found):
- :xyzabc123: (too far from any emoji)
- :nonexistent: (not close enough)
- :zzzzzz: (too different)

## In Headers

# Header with :smile: emoji

## Another header with :ROCKET: emoji

### Header with :heart-eyes: emoji

## In Lists

- Item with :smile: emoji
- Item with :rocket: emoji
- Item with :heart: emoji

## In Paragraphs

This is a paragraph with :smile: emoji and :rocket: emoji. It also has :heart: emoji.

Another paragraph with :Smile-Face: that should be corrected to :smile:.
