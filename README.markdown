# nvALT 2

A collaboration between Brett Terpstra (ttscoff) and David Halter (ElasticThreads) based on [DivineDominion's](github.com/divineDominion/nv) fork. nvALT adds a few features we'd been looking for (and let me get some coding practice).

![Screenshot](http://img.skitch.com/20110520-k5y4i6i3p8ciftq2dbs7rx64e7.jpg)

## Contents

- [About nvALT](#about-nvalt)
- [What it is](#what-it-is)
- [Additional Features](#additional-features)
- [Customization](#customization)
- [Theming](#theming)
- [Download](#download)
- [Credits](#credits)

## About nvALT

nvALT is a fork of the original [Notational Velocity][notational] with some additional features and some interface modifications. It is a work in progress. I'm not listing it as a beta, as that would imply that it was on its way to being its own product. It's an experiment, and I hope you enjoy it!

## What it is

Notational Velocity is a way to take notes quickly and effortlessly using just your keyboard. You press a shortcut to bring up the window and just start typing. It will begin searching existing notes, filtering them as you type. You can use &#x2318;-J and &#x2318;-K to move through the list. Enter selects and begins editing. If you're creating a new note, you just type a unique title and press enter to move the cursor into a blank edit area. Check out the descriptions at [notational.net][notational] for a more eloquent synopsis.

## Additional Features

nvALT adds:

* Widescreen (horizontal) layout option
* Shortcut (&#x2318;-&#x2325;-N) to collapse the notes panel
* Markdown, Textile and MultiMarkdown support with Preview window
* HTML source code tab in the Preview window for fast copy/paste to blogs, etc.
* Unique interface design changes
* Fixes for a couple of bugs/annoyances
* Customizable HTML and CSS files for the Preview window
    * You can use Javascript in the templates to do a few neat tricks
* Editor and preview themes, including MultiMarkdown Composer themes and Marked CSS

## Customization

Select "Open Custom CSS Folder" within the Preview menu, and the application's supprt folder will open. You will find two files:` template.html` and `custom.css`. If you're handy with HTML and CSS, feel free to customize these in whatever way you like. You can add Javascript as well, but you'll need to load external scripts from a url or using a full file:// path. If worse comes to worst, you can just delete or rename your customizations and the default files will be put back in place automatically when you select the menu item again.

Those files are the **Default** preview. A preview theme, below, replaces them for that preview.

## Theming

Themes are chosen in **Settings → Fonts & Colors**. There is a menu for the editor and a menu for the preview, plus **Import…** and **Open Themes Folder**.

**Default** leaves the note editor and the notes list on the current color scheme, and the preview on `template.html` / `custom.css`. Pick a named theme to override that. An editor theme restyles the note editor (background, text, caret, selection, and Markdown syntax colors) and the notes list. List colors come from the theme's `tableView`, `tableViewHighlighted`, and `tableViewInactive` keys, then from the theme's window background and foreground. The note body keeps the `textView` colors when those differ.

Themes are stored in:

```
~/Library/Application Support/nvALT/Themes
```

**Open Themes Folder** reveals that folder. Drop files in it and they show up in the menus. **Import…** copies selected files into the same folder. The first time themes are used, a sample **Paper** editor theme and **Paper** preview theme are copied in. They are not selected for you.

### Editor themes

Editor themes are MultiMarkdown Composer themes. nvALT reads `.theme` files (JSON), plus `.plist` and `.json` files that use the same keys: `textView` colors, `caretColor`, `selectionBackgroundColor`, list colors (`tableView`, `tableViewHighlighted`, `tableViewInactive`), and syntax styles such as `heading`, `strong`, `emphasis`, `codeBlock`, `codeSpan`, `blockquote`, `markup`, and `linkParens`. A Composer theme can be imported as-is.

With no editor theme, the title bar, the divider, and the scrollbars use the system window color, so they follow light and dark mode. An editor theme's window `backgroundColor` (the color at the top of the theme, outside `textView`) colors that same chrome. The scrollbar knob uses the window `foregroundColor` when that color stays visible against it.

### Preview themes

Preview themes are CSS files (`.css`). The note is wrapped in `<div id="wrapper">`, so a Marked theme can be dropped in and used directly. Style `#wrapper` and the elements inside it.

### Adding a theme from a link

`nvalt://` and `nv://` can install a theme from a file or a web address. Percent-encode the `url` value:

```
nvalt://add-theme?url=file%3A%2F%2F%2FUsers%2Fme%2FDesktop%2FSvelte.theme
nv://add-theme?url=https%3A%2F%2Fexample.com%2FPaper.css
```

A local path works without encoding the whole URL:

```
nvalt://add-theme?path=/Users/me/Desktop/Svelte.theme
```

The file is copied into the themes folder and selected. Accepted files are `.theme`, `.plist`, `.json`, and `.css`.

## Download

More info and a download for the compiled binary can be found at [brettterpstra.com/projects/nvalt](http://brettterpstra.com/projects/nvalt/)

## Credits

* [Notational Velocity][notational]
* Code: The original Notational Velocity [source code][original source] by Zachary Schneirov
* Code: DivineDominion's [MultiMarkdown fork][DivineDominion]
* Inspiration: [Elastic Threads' version](http://elasticthreads.tumblr.com/nv) of Notational Velocity

[notational]: http://notational.net/
[original source]: https://github.com/scrod/nv
[DivineDominion]: https://github.com/DivineDominion/nv

