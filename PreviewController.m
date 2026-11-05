//
//  PreviewController.m
//  Notation
//
//  Created by Christian Tietze on 15.10.10.
//  Copyright 2010

#import "PreviewController.h"
#import "AppController.h" // TODO for the defines only, can you get around that?
#import "AppController_Preview.h"
#import "NSString_MultiMarkdown.h"
#import "NSString_Markdown.h"
#import "NSString_Textile.h"
#import "NoteObject.h"
#import "NotationPrefs.h"
#import "BTTransparentScroller.h"
#import "NSFileManager_NV.h"
#import "NSFileManager+DirectoryLocations.h"
#import "NVThemeManager.h"
#import "PreviewRender.h"
#import <Security/Security.h>

#define kDefaultMarkupPreviewVisible @"markupPreviewVisible"

static NSString * const NVMarkedShareKeychainService = @"net.elasticthreads.nv.marked-share";
static NSString * const NVMarkedShareTokenAccount = @"api_token";
static NSString * const NVMarkedShareDeviceAccount = @"device_key";
static NSString * const NVMarkedShareAPIURL = @"https://share.markedapp.com/api/v1/documents";
static NSString * const NVMarkedShareConnectURL = @"https://share.markedapp.com/auth/marked?return_to=nvalt%3A%2F%2Fshare-auth%2Fcallback&label=nvALT";

// Scroll bookkeeping for the preview. The themed document scrolls as a whole
// (document.scrollingElement; body.scrollTop is always 0 in standards mode),
// while the legacy template scrolls a fixed-position #contentdiv, so every
// element with an id is checked rather than assuming one. NV_JS_CAPTURE_SCROLL
// evaluates to an object mapping element ids ("" for the document itself) to
// their scrollTop; NV_JS_RESTORE_SCROLL is a function that applies such an object.
// Restoring also turns off scroll anchoring (overflow-anchor) for the elements it
// scrolls: WebKit would otherwise move the offset again whenever MathJax, Mermaid or
// an image changes the height of content above the viewport, which made the view
// creep by that height on every keystroke.
// The restore takes an optional record of what earlier calls achieved. A later
// call only touches an element whose offset still equals that record, so a reader
// who scrolled while a renderer or a load was still working is left alone.
#define NV_JS_SCROLLER "(document.scrollingElement||document.documentElement||document.body)"
#define NV_JS_CAPTURE_SCROLL \
    "(function(){var s={};var d=" NV_JS_SCROLLER ";if(d&&d.scrollTop>0)s['']=d.scrollTop;" \
    "var els=document.querySelectorAll('[id]');for(var i=0;i<els.length;i++){var e=els[i];if(e.scrollTop>0)s[e.id]=e.scrollTop;}" \
    "return s;})()"
#define NV_JS_RESTORE_SCROLL \
    "(function(s,a){for(var k in s){var e;" \
    "if(k===''){e=" NV_JS_SCROLLER ";document.documentElement.style.overflowAnchor='none';if(document.body)document.body.style.overflowAnchor='none';}" \
    "else{e=document.getElementById(k);if(!e)continue;e.style.overflowAnchor='none';}" \
    "if(a&&(k in a)&&e.scrollTop!==a[k])continue;" \
    "e.scrollTop=s[k];if(a)a[k]=e.scrollTop;}})"

// The two restores around a full load: inline while the page parses, recording
// what was achieved on the page, and once more when the load has finished, which
// yields if the reader scrolled in between.
#define NV_JS_RESTORE_INLINE_FORMAT @"(" NV_JS_RESTORE_SCROLL ")(%@,window.nvScrollApplied={});"
#define NV_JS_RESTORE_AFTER_LOAD_FORMAT @"(" NV_JS_RESTORE_SCROLL ")(%@,window.nvScrollApplied||{});"

// Used when a full document load cannot be avoided: the saved state is replayed
// into the new document (see -preview: and -webView:didFinishLoadForFrame:).
static NSString * const NVPreviewScrollStateScript = @"JSON.stringify(" NV_JS_CAPTURE_SCROLL ")";

// Replaces the rendered note between the nv-content marker comments without
// reloading the document, so nothing flashes and the scroll position is kept.
// Takes a JSON array [html, title]. Returns "1", or "0" when the markers are not
// there (nothing loaded yet, or a stray closing tag in the note pushed the end
// marker out of the container) and the caller must load a full document. MathJax
// and Mermaid, when the page has them, are asked to render the new content.
static NSString * const NVPreviewUpdateInPlaceScript =
    @"(function(a){"
    "var it=document.createNodeIterator(document.documentElement,NodeFilter.SHOW_COMMENT),n,s=null,e=null;"
    "while((n=it.nextNode())){if(n.nodeValue==='nv-content-start')s=n;else if(n.nodeValue==='nv-content-end')e=n;}"
    "if(!s||!e)return '0';"
    // A note whose raw HTML leaves an element open makes the parser nest the end
    // marker inside that element. Climb to the child of the start marker's parent
    // that holds the end marker: the note is everything up to and including it.
    // A fresh end marker is placed as a sibling so the next edit finds it directly.
    "var p=s.parentNode,last=e;while(last&&last.parentNode!==p)last=last.parentNode;"
    "if(!last)return '0';"
    "var saved=" NV_JS_CAPTURE_SCROLL ",applied={};var restore=function(){(" NV_JS_RESTORE_SCROLL ")(saved,applied);};"
    "while(s.nextSibling&&s.nextSibling!==last)p.removeChild(s.nextSibling);p.removeChild(last);"
    "var t=document.createElement('template');t.innerHTML=a[0];t.content.appendChild(document.createComment('nv-content-end'));"
    "p.insertBefore(t.content,s.nextSibling);"
    "document.title=a[1];var h=document.getElementsByClassName('doctitle');for(var i=0;i<h.length;i++)h[i].textContent=a[1];"
    // The wiki linker ran once when the page loaded; link the swapped-in note too,
    // before MathJax and Mermaid rewrite their blocks, as on a full load.
    "if(window.nvLinkWikiText){try{nvLinkWikiText(p);}catch(x){}}"
    // MathJax's promise API (typesetPromise) chains on startup.promise, which never
    // settles inside the legacy WebView, so drive the document directly;
    // handleRetriesFor covers glyphs whose font data MathJax still has to load.
    "if(window.MathJax&&MathJax.startup&&MathJax.startup.document){try{var d=MathJax.startup.document;d.clear();"
    "var mj=MathJax._&&MathJax._.mathjax&&MathJax._.mathjax.mathjax;"
    "if(mj&&mj.handleRetriesFor){mj.handleRetriesFor(function(){d.render();}).then(restore,function(){});}else{d.render();}}catch(x){}}"
    "if(window.mermaid&&mermaid.run){try{mermaid.run().then(restore,function(){});}catch(x){}}"
    // Nothing above repaints, so one restore here sees the swapped content and
    // MathJax's synchronous pass. The renderers call it again when their
    // asynchronous work changes the page height, which also undoes a restore
    // that was clamped at the bottom of a note; those later calls yield if the
    // reader has scrolled in the meantime.
    "restore();"
    "return '1';})(%@)";

static const NSUInteger NVMarkedShareMaxBodyBytes = 500000;

@interface PreviewController (MarkedShare)
- (NSString *)keychainStringForAccount:(NSString *)account;
- (void)setKeychainString:(NSString *)string forAccount:(NSString *)account;
- (void)deleteMarkedShareCredentials;
- (BOOL)markedShareIsConnected;
- (void)connectMarkedShare;
- (NSString *)markedShareMessageForStatus:(NSInteger)status data:(NSData *)data;
@end

@implementation PreviewController

@synthesize preview;
@synthesize isPreviewOutdated;
@synthesize isPreviewSticky;

+(void)initialize
{
    NSDictionary *appDefaults = [NSDictionary dictionaryWithObject:[NSNumber numberWithBool:NO]
                                                            forKey:kDefaultMarkupPreviewVisible];

    [[NSUserDefaults standardUserDefaults] registerDefaults:appDefaults];
    /* Initialize webInspector. */
    [[NSUserDefaults standardUserDefaults] setBool:TRUE forKey:@"WebKitDeveloperExtras"];
    [[NSUserDefaults standardUserDefaults] synchronize];

}

-(id)init
{
    if ((self = [super initWithWindowNibName:@"MarkupPreview" owner:self])) {
        self.isPreviewOutdated = YES;
        self.isPreviewSticky = NO;
        //        [[self class] createCustomFiles];
        BOOL showPreviewWindow = [[NSUserDefaults standardUserDefaults] boolForKey:kDefaultMarkupPreviewVisible];
        if (showPreviewWindow) {
            [[self window] orderFront:self];
        }

        //        tabSwitcher = [[[ETTransparentButton alloc]initWithFrame:shCon] retain];
        //        shCon.origin.x = [[[self window] contentView]visibleRect].origin.x + [[[self window] contentView]visibleRect].size.width - 80;
        //        shCon.size.width = 56;
        //        saveButton = [[[ETTransparentButton alloc]initWithFrame:shCon] retain];
        //        shCon.origin.x -= 65;
        //        shareButton = [[[ETTransparentButton alloc]initWithFrame:shCon] retain];
        //        shCon.origin.x -= 65;
        //        stickyPreviewButton = [[[ETTransparentButton alloc]initWithFrame:shCon] retain];
        //        shCon.origin.x -= 65;
        //        printPreviewButton = [[[ETTransparentButton alloc]initWithFrame:shCon] retain];
        //        [tabSwitcher setTitle:@"View Source"];
        //        [tabSwitcher setTarget:self];
        //        [tabSwitcher setAction:@selector(switchTabs:)];
        //        [tabSwitcher setAutoresizingMask:NSViewMaxXMargin];
        //        [shareButton setTitle:@"Share"];
        //        [shareButton setToolTip:@"Share this note on Marked Share"];
        //        [shareButton setTarget:self];
        //        [shareButton setAction:@selector(shareAsk:)];
        //        [shareButton setAutoresizingMask:NSViewMinXMargin];
        //        [saveButton setTitle:@"Save"];
        //        [saveButton setToolTip:@"Save the current preview as an HTML file"];
        //        [saveButton setTarget:self];
        //        [saveButton setAction:@selector(saveHTML:)];
        //        [saveButton setAutoresizingMask:NSViewMinXMargin];
        //        [stickyPreviewButton setTitle:@"Stick"];
        //        [stickyPreviewButton setToolTip:@"Maintain current note in Preview, even if you switch to other notes."];
        //        [stickyPreviewButton setTarget:self];
        //        [stickyPreviewButton setAction:@selector(makePreviewSticky:)];
        //        [stickyPreviewButton setAutoresizingMask:NSViewMinXMargin];
        //        [printPreviewButton setTitle:@"Print"];
        //        [printPreviewButton setToolTip:@"Print to Printer or PDF."];
        //        [printPreviewButton setTarget:self];
        //        [printPreviewButton setAction:@selector(printPreview:)];
        //        [printPreviewButton setAutoresizingMask:NSViewMinXMargin];
        //        [[[self window] contentView] addSubview:tabSwitcher];
        //        [[[self window] contentView] addSubview:shareButton];
        //        [[[self window] contentView] addSubview:saveButton];
        //        [[[self window] contentView] addSubview:stickyPreviewButton];
        //        [[[self window] contentView] addSubview:printPreviewButton];
        // [[[self window] contentView] setNeedsDisplay:YES];

        //		[preview setPolicyDelegate:self];
        //		[preview setUIDelegate:self];
    }
    return self;
}

- (NSButton *)popupButtonWithTitle:(NSString *)title action:(SEL)action frame:(NSRect)frame
{
    NSButton *button = [[NSButton alloc] initWithFrame:frame];
    [button setTitle:title];
    [button setBezelStyle:NSBezelStyleRounded];
    [button setButtonType:NSButtonTypeMomentaryPushIn];
    [button setControlSize:NSControlSizeSmall];
    [button setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
    [[button cell] setControlSize:NSControlSizeSmall];
    [button setTarget:self];
    [button setAction:action];
    return button;
}

- (NSColor *)color:(NSColor *)color forAppearance:(NSAppearance *)appearance
{
    __block NSColor *resolved = color;
    [appearance performAsCurrentDrawingAppearance:^{
        NSColor *components = [color colorUsingType:NSColorTypeComponentBased];
        if (components)
            resolved = components;
    }];
    return resolved;
}

- (void)styleShareTextField:(NSTextField *)field
{
    [field setTextColor:[NSColor labelColor]];
    [field setDrawsBackground:NO];
    [field setBezeled:NO];
    [field setBordered:NO];
}

- (void)applySystemAppearanceToPopup:(MAAttachedWindow *)popup
{
    NSAppearance *appearance = [NSApp effectiveAppearance];
    [popup setAppearance:appearance];
    [popup setBackgroundColor:[self color:[NSColor windowBackgroundColor] forAppearance:appearance]];
    [popup setBorderColor:[self color:[NSColor separatorColor] forAppearance:appearance]];
    [popup setViewMargin:8.0f];
    [popup setBorderWidth:1.0f];
    [popup setCornerRadius:12.0f];
    [popup setHasArrow:YES];
    [popup setDrawsRoundCornerBesideArrow:YES];
    [popup setArrowBaseWidth:16.0f];
    [popup setArrowHeight:8.0f];
}

-(void)awakeFromNib
{
    NSRect confirmBounds = [shareConfirmation bounds];
    shareCancel = [self popupButtonWithTitle:@"No, thanks" action:@selector(cancelShare:) frame:NSMakeRect(12, 8, 108, 24)];
    shareConfirm = [self popupButtonWithTitle:@"Yes" action:@selector(shareNote:) frame:NSMakeRect(NSMaxX(confirmBounds) - 12 - 72, 8, 72, 24)];
    [shareConfirm setKeyEquivalent:@"\r"];
    [shareConfirmation addSubview:shareCancel];
    [shareConfirmation addSubview:shareConfirm];
    for (NSView *subview in [shareConfirmation subviews]) {
        if ([subview isKindOfClass:[NSTextField class]])
            [self styleShareTextField:(NSTextField *)subview];
    }

    NSRect noticeBounds = [shareNotification bounds];
    viewOnWebButton = [self popupButtonWithTitle:@"View in Browser" action:@selector(openShareURL:) frame:NSMakeRect(NSMidX(noticeBounds) - 64, 8, 128, 24)];
    [shareNotification addSubview:viewOnWebButton];
    [self styleShareTextField:urlTextField];
    [tabView selectTabViewItem:[tabView tabViewItemAtIndex:0]];
    for (NSView *subview in [shareNotification subviews]) {
        if (![subview isKindOfClass:[NSButton class]] || [(NSButton *)subview action] != @selector(hideShareURL:))
            continue;
        NSButton *closeButton = (NSButton *)subview;
        NSImage *image = [NSImage imageWithSystemSymbolName:@"xmark" accessibilityDescription:@"Close"];
        [image setTemplate:YES];
        [closeButton setImage:image];
        [closeButton setAlternateImage:nil];
        [closeButton setBordered:NO];
        [closeButton setImagePosition:NSImageOnly];
        [closeButton setContentTintColor:[NSColor secondaryLabelColor]];
        [closeButton setFrame:NSMakeRect(6, NSMaxY(noticeBounds) - 22, 16, 16)];
    }

    cssString = [[[self class] css] retain];
    htmlString = [[[self class] html] retain];
    lastNote = [[[NSApp delegate] selectedNoteObject] retain];
    [sourceView setTextContainerInset:NSMakeSize(10.0,12.0)];
    NSScrollView *scrlView=[sourceView enclosingScrollView];
    if (!IsLionOrLater) {
        NSRect vsRect=[[scrlView verticalScroller]frame];
        BTTransparentScroller *theScroller=[[BTTransparentScroller alloc]initWithFrame:vsRect];
        [scrlView setVerticalScroller:theScroller];
        [theScroller release];
    }
    [scrlView setScrollsDynamically:YES];
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
    if (IsLionOrLater) {
        [scrlView setHorizontalScrollElasticity:NSScrollElasticityNone];
        [scrlView setVerticalScrollElasticity:NSScrollElasticityAutomatic];
        [scrlView setScrollerStyle:NSScrollerStyleOverlay];
    }
#endif
}

//this returns a nice name for the method in the JavaScript environment
+(NSString*)webScriptNameForSelector:(SEL)sel
{
    if(sel == @selector(logJavaScriptString:))
        return @"log";
    return nil;
}

//this allows JavaScript to call the -logJavaScriptString: method
+ (BOOL)isSelectorExcludedFromWebScript:(SEL)sel
{
    if(sel == @selector(logJavaScriptString:))
        return NO;
    return YES;
}

//this is a simple log command
- (void)logJavaScriptString:(NSString*) logText
{
    NSLog(@"JavaScript: %@",logText);
}

//this is called as soon as the script environment is ready in the webview
- (void)webView:(WebView *)sender didClearWindowObject:(WebScriptObject *)windowScriptObject forFrame:(WebFrame *)frame
{
    //add the controller to the script environment
    //the "Cocoa" object will now be available to JavaScript
    [windowScriptObject setValue:self forKey:@"Cocoa"];
}

// Re-apply the saved scroll position once the new document has fully loaded, in
// case layout moved after the inline restore ran (images, fonts, scripts). The
// script leaves the page alone if the reader scrolled after the inline restore.
- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    if (frame != [sender mainFrame])
        return;
    if (pendingScrollState) {
        [sender stringByEvaluatingJavaScriptFromString:[self javaScriptReapplyingPreviewScrollState:pendingScrollState]];
        [pendingScrollState release];
        pendingScrollState = nil;
    }
}

// Above webView methods from <http://stackoverflow.com/questions/2288582/embedded-webkit-script-callbacks-how/2293305#2293305>

- (void)webView:(WebView *)sender decidePolicyForNavigationAction:(NSDictionary *)actionInformation request:(NSURLRequest *)request frame:(WebFrame *)frame decisionListener:(id<WebPolicyDecisionListener>)listener {
    // A link to an anchor in the note itself (href="#section") scrolls the
    // preview. Everything else a click asks for opens outside, as before. The
    // page loaded from a string has no request URL, so ask the document itself.
    if (NVPreviewURLIsFragmentOfDocument([request URL], [NSURL URLWithString:[[frame DOMDocument] URL]])) {
        [listener use];
        return;
    }
    if (![[actionInformation objectForKey:@"WebActionNavigationTypeKey"] isEqualToNumber:[NSNumber numberWithInt:5]]) {
        [[NSWorkspace sharedWorkspace] openURL:[request URL]];
        [listener ignore];
    } else {
        [listener use];
    }
}

- (void)webView:(WebView *)sender decidePolicyForNewWindowAction:(NSDictionary *)actionInformation request:(NSURLRequest *)request newFrameName:(NSString *)frameName decisionListener:(id<WebPolicyDecisionListener>)listener {
    NSLog(@"NEW WIN ACTION SENDER: %@",sender);
    [[NSWorkspace sharedWorkspace] openURL:[actionInformation objectForKey:WebActionOriginalURLKey]];
    [listener ignore];
}

-(void)requestPreviewUpdate:(NSNotification *)notification
{
    AppController *app = [notification object];
    NSString *rawString = [app noteContent];
    NSPasteboard* pb = [NSPasteboard pasteboardWithName:@"mkStreamingPreview"];
    [pb clearContents];
    [pb setString:rawString forType:(NSString*)kUTTypeUTF8PlainText];

    if (![[self window] isVisible]) {
        self.isPreviewOutdated = YES;
        return;
    }

    if (self.isPreviewSticky) {
        return;
    }


    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(preview:) object:app];

    [self performSelector:@selector(preview:) withObject:app afterDelay:0.05];
}

- (BOOL)previewIsVisible{
    return [[self window] isVisible];
}

-(void)togglePreview:(id)sender
{

    NSWindow *wnd = [self window];
    if ([wnd isVisible]) {
        if (attachedWindow) {
            [[shareButton window] removeChildWindow:attachedWindow];
            [attachedWindow orderOut:self];
            [attachedWindow release];
            attachedWindow = nil;
            [shareURL release];
        }
        //      // TODO: should the "stuck" note remain stuck when preview is closed?
        //      if (self.isPreviewSticky)
        //        [self makePreviewNotSticky:self];
        [wnd orderOut:self];
    } else {
        if (self.isPreviewOutdated) {
            // TODO high coupling; too many assumptions on architecture:
            [self performSelector:@selector(preview:) withObject:[[NSApplication sharedApplication] delegate] afterDelay:0.0];
        }
        [tabView selectTabViewItem:[tabView tabViewItemAtIndex:0]];
        [tabSwitcher setTitle:@"View Source"];

        [wnd orderFront:self];
    }

    // save visibility to defaults
    [[NSUserDefaults standardUserDefaults] setObject:[NSNumber numberWithBool:[wnd isVisible]]
                                              forKey:kDefaultMarkupPreviewVisible];
}

-(void)windowWillClose:(NSNotification *)notification
{
    [[NSUserDefaults standardUserDefaults] setObject:[NSNumber numberWithBool:NO]
                                              forKey:kDefaultMarkupPreviewVisible];
    NSMenu *previewMenu = [[[NSApp mainMenu] itemWithTitle:@"Preview"] submenu];
    [[previewMenu itemWithTitle:@"Toggle Preview Window"]setState:0];
}

+(NSString*)css {
    NSFileManager *mgr = [NSFileManager defaultManager];
    NSString *folder = [[NSFileManager defaultManager] applicationSupportDirectory];
    NSString *cssFileName = @"custom.css";
    NSString *customCSSPath = [folder stringByAppendingPathComponent: cssFileName];
    if ([mgr fileExistsAtPath:customCSSPath]) {
        return [NSString stringWithContentsOfFile:customCSSPath
                                         encoding:NSUTF8StringEncoding
                                            error:NULL];
    } else {
        NSString *cssPath = [[NSBundle mainBundle] pathForResource:@"custom" ofType:@"css" inDirectory:nil];
        return [NSString stringWithContentsOfFile:cssPath encoding:NSUTF8StringEncoding error:nil];
    }

    //	if (![mgr fileExistsAtPath:customCSSPath]) {
    //		[[self class] createCustomFiles];
    //	}


}

+ (NSString *)htmlEscapedString:(NSString *)string {
    if (![string length])
        return @"";
    NSMutableString *escaped = [NSMutableString stringWithString:string];
    [escaped replaceOccurrencesOfString:@"&" withString:@"&amp;" options:0 range:NSMakeRange(0, [escaped length])];
    [escaped replaceOccurrencesOfString:@"<" withString:@"&lt;" options:0 range:NSMakeRange(0, [escaped length])];
    [escaped replaceOccurrencesOfString:@">" withString:@"&gt;" options:0 range:NSMakeRange(0, [escaped length])];
    [escaped replaceOccurrencesOfString:@"\"" withString:@"&quot;" options:0 range:NSMakeRange(0, [escaped length])];
    return escaped;
}

+ (NSString *)themedHTMLDocumentForTitle:(NSString *)title contentHTML:(NSString *)contentHTML {
    NSString *css = [[NVThemeManager sharedManager] currentPreviewCSS];
    if (![css length])
        return nil;
    NSString *safeCSS = [css stringByReplacingOccurrencesOfString:@"</style" withString:@"<\\/style" options:NSCaseInsensitiveSearch range:NSMakeRange(0, [css length])];
    return [NSString stringWithFormat:@"<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>%@</title><style type=\"text/css\">%@</style></head><body><div id=\"wrapper\">%@</div></body></html>",
            [self htmlEscapedString:title], safeCSS, contentHTML ?: @""];
}

- (void)themesDidChange:(NSNotification *)notification {
    // The CSS changed, so the next preview must rebuild the page rather than
    // swap the note into the one that is showing.
    previewNeedsFullReload = YES;
    if (self.isPreviewSticky || ![self isWindowLoaded] || ![[self window] isVisible])
        return;
    [self preview:[NSApp delegate]];
}

+(NSString*)html {
    NSFileManager *mgr = [NSFileManager defaultManager];

    NSString *folder = [[NSFileManager defaultManager] applicationSupportDirectory];
    NSString *htmlFileName = @"template.html";
    NSString *customHTMLPath = [folder stringByAppendingPathComponent: htmlFileName];
    if ([mgr fileExistsAtPath:customHTMLPath]) {
        return [NSString stringWithContentsOfFile:customHTMLPath
                                         encoding:NSUTF8StringEncoding
                                            error:NULL];
    } else {
        NSString *htmlPath = [[NSBundle mainBundle] pathForResource:@"template" ofType:@"html" inDirectory:nil];
        return [NSString stringWithContentsOfFile:htmlPath encoding:NSUTF8StringEncoding error:nil];
    }
    //	if (![mgr fileExistsAtPath:customHTMLPath]) {
    //		[[self class] createCustomFiles];
    //	}
}

#pragma mark - Preview scroll position

// Returns a JSON object mapping element ids ("" for the document itself) to their
// scrollTop, or nil when nothing is scrolled.
- (NSString *)currentPreviewScrollState
{
    NSString *state = [preview stringByEvaluatingJavaScriptFromString:NVPreviewScrollStateScript];
    if ([state length] < 3 || ![state hasPrefix:@"{"] || ![state hasSuffix:@"}"])
        return nil;
    // Keep the literal safe to embed inside an inline <script> element.
    return [state stringByReplacingOccurrencesOfString:@"<" withString:@"\\u003c"];
}

// Inline script for a full load: applies the state while the page parses and
// records what it achieved, so the post-load restore can tell whether the reader
// has scrolled since.
- (NSString *)javaScriptRestoringPreviewScrollState:(NSString *)state
{
    return [NSString stringWithFormat:NV_JS_RESTORE_INLINE_FORMAT, state];
}

// Run when the load has finished: applies the state again unless the reader
// moved the scroll bar after the inline restore.
- (NSString *)javaScriptReapplyingPreviewScrollState:(NSString *)state
{
    return [NSString stringWithFormat:NV_JS_RESTORE_AFTER_LOAD_FORMAT, state];
}

// Swaps the rendered note into the page that is already showing. Returns NO when
// the page has no markers (nothing loaded yet), in which case the caller loads a
// full document instead.
- (BOOL)updatePreviewInPlaceWithContent:(NSString *)content title:(NSString *)title
{
    NSData *args = [NSJSONSerialization dataWithJSONObject:@[content ?: @"", title ?: @""] options:0 error:NULL];
    if (!args)
        return NO;
    NSString *argsString = [[[NSString alloc] initWithData:args encoding:NSUTF8StringEncoding] autorelease];
    // JSON leaves the Unicode line and paragraph separators raw, but older
    // JavaScript engines reject them inside a string literal; escape them.
    argsString = [argsString stringByReplacingOccurrencesOfString:@"\u2028" withString:@"\\u2028"];
    argsString = [argsString stringByReplacingOccurrencesOfString:@"\u2029" withString:@"\\u2029"];
    NSString *result = [preview stringByEvaluatingJavaScriptFromString:[NSString stringWithFormat:NVPreviewUpdateInPlaceScript, argsString]];
    return [result isEqualToString:@"1"];
}

-(void)preview:(id)object
{
    if (self.isPreviewSticky) {
        [self updateOpenInMarkedButton];
        return;
    }
    AppController *app = object;
    NSString *rawString = [app noteContent];

    SEL mode = [self markupProcessorSelector:[app currentPreviewMode]];
    NSString *processedString = [NSString performSelector:mode withObject:rawString];
    NSString *noteTitle =  ([app selectedNoteObject]) ? [NSString stringWithFormat:@"%@",titleOfNote([app selectedNoteObject])] : @"";
    NSString *previewAssets = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"preview-js"];

    BOOL needsMath = NO, needsMermaid = NO, needsWiki = NO;
    NSString *content = NVPreviewHTMLPreparingContent(processedString, &needsMath, &needsMermaid, &needsWiki) ?: @"";
    BOOL sameNote = (lastNote == [app selectedNoteObject]);

    // Editing the note that is already showing: swap the rendered note into the
    // existing page instead of reloading it, so nothing flashes and the scroll
    // position is untouched. A full load is still needed when the theme changed,
    // when a load is in flight, or when the note newly needs a renderer
    // (MathJax, Mermaid, the wiki linker) that this page has not loaded.
    if (sameNote && !previewNeedsFullReload && ![preview isLoading]
        && (!needsMath || previewHasMathJax) && (!needsMermaid || previewHasMermaid)
        && (!needsWiki || previewHasWiki)
        && [self updatePreviewInPlaceWithContent:content title:noteTitle]) {
        [[self window] setTitle:noteTitle];
        [sourceView replaceCharactersInRange:NSMakeRange(0, [[sourceView string] length]) withString:processedString];
        self.isPreviewOutdated = NO;
        [self updateOpenInMarkedButton];
        return;
    }

    NSString *scrollState = nil;
    if (sameNote) {
        // Keep the reader's place across the reload. If the previous reload is
        // still in flight the new document may not be scrolled yet, so reuse
        // what we saved. The retain/autorelease keeps that object alive for the
        // rest of this method even though pendingScrollState is replaced below.
        scrollState = ([preview isLoading] && pendingScrollState) ? [[pendingScrollState retain] autorelease] : [self currentPreviewScrollState];
    } else {
        [cssString release];
        [htmlString release];
        cssString = [[[self class] css] retain];
        htmlString = [[[self class] html] retain];
        // Owned, so a deleted note cannot be freed and its address reused by a
        // new note that would then pass the sameNote check above.
        [lastNote release];
        lastNote = [[app selectedNoteObject] retain];
    }
    NSString *newPendingState = [scrollState copy];
    [pendingScrollState release];
    pendingScrollState = newPendingState;

    // The marker comments let later edits replace just the note; see
    // -updatePreviewInPlaceWithContent:title:. The scroll restore goes before
    // the renderer scripts so it is not held up by loading them.
    NSMutableString *previewString = [NSMutableString stringWithFormat:@"<!--nv-content-start-->%@<!--nv-content-end-->", content];
    if (scrollState) {
        [previewString appendFormat:@"\n<script>%@</script>", [self javaScriptRestoringPreviewScrollState:scrollState]];
    }
    NSString *rendererScripts = NVPreviewRendererScripts(previewAssets, needsMath, needsMermaid, needsWiki);
    if (rendererScripts) {
        [previewString appendString:rendererScripts];
    }
    previewHasMathJax = needsMath;
    previewHasMermaid = needsMermaid;
    previewHasWiki = needsWiki;
    previewNeedsFullReload = NO;

    NSString *nvSupportPath = [[NSFileManager defaultManager] applicationSupportDirectory];

    NSString *themedDocument = [[self class] themedHTMLDocumentForTitle:noteTitle contentHTML:previewString];
    if (themedDocument) {
        [[preview mainFrame] loadHTMLString:themedDocument baseURL:nil];
        [[self window] setTitle:noteTitle];
        [sourceView replaceCharactersInRange:NSMakeRange(0, [[sourceView string] length]) withString:processedString];
        self.isPreviewOutdated = NO;
        [self updateOpenInMarkedButton];
        return;
    }
    NSMutableString *outputString = [NSMutableString stringWithString:(NSString *)htmlString];
    NSString *bodyString = previewString;
    if ([htmlString rangeOfString:@"id=\"wrapper\"" options:NSCaseInsensitiveSearch].location == NSNotFound &&
        [htmlString rangeOfString:@"id='wrapper'" options:NSCaseInsensitiveSearch].location == NSNotFound) {
        bodyString = [NSString stringWithFormat:@"<div id=\"wrapper\">%@</div>", previewString];
    }

    [outputString replaceOccurrencesOfString:@"{%support%}" withString:nvSupportPath options:0 range:NSMakeRange(0, [outputString length])];
    [outputString replaceOccurrencesOfString:@"{%title%}" withString:noteTitle options:0 range:NSMakeRange(0, [outputString length])];
    [outputString replaceOccurrencesOfString:@"{%content%}" withString:bodyString options:0 range:NSMakeRange(0, [outputString length])];
    [outputString replaceOccurrencesOfString:@"{%style%}" withString:cssString options:0 range:NSMakeRange(0, [outputString length])];

    [[preview mainFrame] loadHTMLString:outputString baseURL:nil];
    [[self window] setTitle:noteTitle];

    [sourceView replaceCharactersInRange:NSMakeRange(0, [[sourceView string] length]) withString:processedString];
    self.isPreviewOutdated = NO;
    [self updateOpenInMarkedButton];
}

-(SEL)markupProcessorSelector:(NSInteger)previewMode
{
    if (previewMode == MarkdownPreview) {
        previewMode = MultiMarkdownPreview;
        return @selector(stringWithProcessedMultiMarkdown:);
    } else if (previewMode == MultiMarkdownPreview) {
        return @selector(stringWithProcessedMultiMarkdown:);
    } else if (previewMode == TextilePreview) {
        return @selector(stringWithProcessedTextile:);
    }

    return nil;
}

+ (void) createCustomFiles
{
    NSFileManager *fileManager = [NSFileManager defaultManager];

    NSString *folder = [[NSFileManager defaultManager] applicationSupportDirectory];
    if ([fileManager fileExistsAtPath: folder] == NO)
    {
        [fileManager createFolderAtPath:folder];
        //				[fileManager createDirectoryAtPath: folder attributes: nil];

    }

    NSString *cssFileName = @"custom.css";
    NSString *cssFile = [folder stringByAppendingPathComponent: cssFileName];

    if ([fileManager fileExistsAtPath:cssFile] == NO)
    {
        NSString *cssPath = [[NSBundle mainBundle] pathForResource:@"customclean" ofType:@"css" inDirectory:nil];
        NSString *cssString = [NSString stringWithContentsOfFile:cssPath encoding:NSUTF8StringEncoding error:nil];
        NSData *cssData = [NSData dataWithBytes:[cssString UTF8String] length:[cssString length]];
        [fileManager createFileAtPath:cssFile contents:cssData attributes:nil];
    }

    NSString *htmlFileName = @"template.html";
    NSString *htmlFile = [folder stringByAppendingPathComponent: htmlFileName];

    if ([fileManager fileExistsAtPath:htmlFile] == NO)
    {
        NSString *htmlPath = [[NSBundle mainBundle] pathForResource:@"templateclean" ofType:@"html" inDirectory:nil];
        NSString *htmlString = [NSString stringWithContentsOfFile:htmlPath encoding:NSUTF8StringEncoding error:nil];
        NSData *htmlData = [NSData dataWithBytes:[htmlString UTF8String] length:[htmlString length]];
        [fileManager createFileAtPath:htmlFile contents:htmlData attributes:nil];
    }

}

- (NSString *)urlEncodeValue:(NSString *)str
{
    NSString *result = (NSString *) CFURLCreateStringByAddingPercentEscapes(kCFAllocatorDefault, (CFStringRef)str, NULL, CFSTR("?=&+"), kCFStringEncodingUTF8);
    return [result autorelease];
}

-(IBAction)makePreviewSticky:(id)sender
{
    self.isPreviewSticky = YES;
    //  [[preview window] setTitle:@"Locked"];
    [stickyPreviewButton setState:YES];
    [stickyPreviewButton setToolTip:@"Return the preview to normal functionality."];
    [stickyPreviewButton setAction:@selector(makePreviewNotSticky:)];
    [shareButton setEnabled:NO];
    [self updateOpenInMarkedButton];
    [[self window] setHidesOnDeactivate:NO];
}

-(IBAction)makePreviewNotSticky:(id)sender
{
    self.isPreviewSticky = NO;
    [[preview window] setTitle:@"Preview"];
    [stickyPreviewButton setState:NO];
    [stickyPreviewButton setToolTip:@"Maintain current note in Preview, even if you switch to other notes."];
    [stickyPreviewButton setAction:@selector(makePreviewSticky:)];
    [shareButton setEnabled:YES];
    self.isPreviewOutdated = YES;
    [self performSelector:@selector(preview:) withObject:[[NSApplication sharedApplication] delegate] afterDelay:0.0];
    [[self window] setHidesOnDeactivate:YES];
}

-(IBAction)printPreview:(id)sender
{
    NSTabViewItem *selectedTab=[tabView selectedTabViewItem];
    //1 is webview   2 is source view
    if ([selectedTab.identifier integerValue]==1) {
        [tabView selectNextTabViewItem:self];
    }
    NSPrintInfo* printInfo = [NSPrintInfo sharedPrintInfo];

    [printInfo setHorizontallyCentered:YES];
    [printInfo setVerticallyCentered:NO];
    NSPrintOperation *printOp=[[[preview mainFrame] frameView] printOperationWithPrintInfo:printInfo];
    [printOp runOperationModalForWindow:tabView.window delegate:self didRunSelector:@selector(printOperationDidRun:success:contextInfo:) contextInfo:selectedTab];
}

- (void)printOperationDidRun:(NSPrintOperation *)printOperation  success:(BOOL)success  contextInfo:(void *)contextInfo{
    NSTabViewItem *selTab=(NSTabViewItem *)contextInfo;
    if (selTab&&(tabView.selectedTabViewItem!=selTab)) {
        [tabView selectTabViewItem:selTab];
    }
}

- (NSString *)keychainStringForAccount:(NSString *)account
{
    NSDictionary *query = [NSDictionary dictionaryWithObjectsAndKeys:
                           (id)kSecClassGenericPassword, (id)kSecClass,
                           NVMarkedShareKeychainService, (id)kSecAttrService,
                           account, (id)kSecAttrAccount,
                           (id)kCFBooleanTrue, (id)kSecReturnData,
                           (id)kSecMatchLimitOne, (id)kSecMatchLimit,
                           nil];
    CFTypeRef result = NULL;
    if (SecItemCopyMatching((CFDictionaryRef)query, &result) != errSecSuccess || !result)
        return nil;
    NSData *data = [(NSData *)result autorelease];
    return [[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] autorelease];
}

- (void)setKeychainString:(NSString *)string forAccount:(NSString *)account
{
    NSDictionary *query = [NSDictionary dictionaryWithObjectsAndKeys:
                           (id)kSecClassGenericPassword, (id)kSecClass,
                           NVMarkedShareKeychainService, (id)kSecAttrService,
                           account, (id)kSecAttrAccount,
                           nil];
    SecItemDelete((CFDictionaryRef)query);
    NSMutableDictionary *item = [NSMutableDictionary dictionaryWithDictionary:query];
    [item setObject:[string dataUsingEncoding:NSUTF8StringEncoding] forKey:(id)kSecValueData];
    [item setObject:(id)kSecAttrAccessibleAfterFirstUnlock forKey:(id)kSecAttrAccessible];
    SecItemAdd((CFDictionaryRef)item, NULL);
}

- (void)deleteMarkedShareCredentials
{
    NSDictionary *query = [NSDictionary dictionaryWithObjectsAndKeys:
                           (id)kSecClassGenericPassword, (id)kSecClass,
                           NVMarkedShareKeychainService, (id)kSecAttrService,
                           nil];
    SecItemDelete((CFDictionaryRef)query);
}

- (BOOL)markedShareIsConnected
{
    return [[self keychainStringForAccount:NVMarkedShareTokenAccount] length] > 0
        && [[self keychainStringForAccount:NVMarkedShareDeviceAccount] length] > 0;
}

- (void)connectMarkedShare
{
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:NVMarkedShareConnectURL]];
}

- (void)handleShareAuthURL:(NSURL *)url
{
    [self window];
    NSString *token = nil;
    NSString *deviceKey = nil;
    NSURLComponents *components = [NSURLComponents componentsWithURL:url resolvingAgainstBaseURL:NO];
    for (NSURLQueryItem *item in [components queryItems]) {
        if ([[item name] isEqualToString:@"api_token"])
            token = [item value];
        else if ([[item name] isEqualToString:@"device_key"])
            deviceKey = [item value];
    }
    if (![token length] || ![deviceKey length]) {
        [self showShareURL:@"Marked Share did not return credentials." isError:YES];
        return;
    }
    [self setKeychainString:token forAccount:NVMarkedShareTokenAccount];
    [self setKeychainString:deviceKey forAccount:NVMarkedShareDeviceAccount];
    [self showShareURL:@"Marked Share is connected. Share the note again." isError:YES];
}

- (NSString *)markedShareMessageForStatus:(NSInteger)status data:(NSData *)data
{
    if (status == 401)
        return @"Connect nvALT to Marked Share, then share again.";
    if (status == 429)
        return @"Too many shares. Wait a minute and try again.";
    NSString *message = nil;
    if ([data length]) {
        id json = [NSJSONSerialization JSONObjectWithData:data options:0 error:NULL];
        if ([json isKindOfClass:[NSDictionary class]])
            message = [json objectForKey:@"error"];
    }
    if ([message isKindOfClass:[NSString class]] && [message length])
        return message;
    return @"Could not share this note.";
}

-(IBAction)shareNote:(id)sender
{
    if (![self markedShareIsConnected]) {
        [self connectMarkedShare];
        [self showShareURL:@"Connect nvALT in the browser, then share again." isError:YES];
        return;
    }

    AppController *app = [NSApp delegate];
    NoteObject *note = [app selectedNoteObject];
    NSString *rawString = [app noteContent];
    if (!note || ![rawString length]) {
        [self showShareURL:@"There is no note to share." isError:YES];
        return;
    }
    if ([rawString lengthOfBytesUsingEncoding:NSUTF8StringEncoding] > NVMarkedShareMaxBodyBytes) {
        [self showShareURL:@"This note is too long to share." isError:YES];
        return;
    }

    NSString *noteTitle = titleOfNote(note);
    if (!noteTitle)
        noteTitle = @"";
    NSDictionary *payload = [NSDictionary dictionaryWithObject:
                             [NSDictionary dictionaryWithObjectsAndKeys:
                              noteTitle, @"title",
                              rawString, @"body_markdown",
                              @"unlisted", @"visibility",
                              [NSNumber numberWithBool:NO], @"allow_remix",
                              nil]
                             forKey:@"document"];
    NSError *jsonError = nil;
    NSData *body = [NSJSONSerialization dataWithJSONObject:payload options:0 error:&jsonError];
    if (!body) {
        [self showShareURL:@"Could not prepare this note for sharing." isError:YES];
        return;
    }

    NSString *token = [self keychainStringForAccount:NVMarkedShareTokenAccount];
    NSString *deviceKey = [self keychainStringForAccount:NVMarkedShareDeviceAccount];
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:NVMarkedShareAPIURL]];
    [request setHTTPMethod:@"POST"];
    [request setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
    [request setValue:[NSString stringWithFormat:@"Bearer %@", token] forHTTPHeaderField:@"Authorization"];
    [request setValue:deviceKey forHTTPHeaderField:@"X-Device-Key"];
    [request setHTTPBody:body];

    [self cancelShare:self];
    [shareButton setEnabled:NO];

    NSURLSessionDataTask *task = [[NSURLSession sharedSession] dataTaskWithRequest:request
        completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
            NSInteger status = [response isKindOfClass:[NSHTTPURLResponse class]] ? [(NSHTTPURLResponse *)response statusCode] : 0;
            NSString *publishedURL = nil;
            if (status == 201 && [data length]) {
                id json = [NSJSONSerialization JSONObjectWithData:data options:0 error:NULL];
                if ([json isKindOfClass:[NSDictionary class]]) {
                    id urlValue = [json objectForKey:@"url"];
                    if (![urlValue isKindOfClass:[NSString class]]) {
                        id document = [json objectForKey:@"document"];
                        if ([document isKindOfClass:[NSDictionary class]])
                            urlValue = [document objectForKey:@"url"];
                    }
                    if ([urlValue isKindOfClass:[NSString class]] && [urlValue length])
                        publishedURL = urlValue;
                }
            }
            NSString *failure = nil;
            if (!publishedURL)
                failure = error ? [error localizedDescription] : [self markedShareMessageForStatus:status data:data];
            dispatch_async(dispatch_get_main_queue(), ^{
                [shareButton setEnabled:YES];
                if (status == 401)
                    [self deleteMarkedShareCredentials];
                if (publishedURL) {
                    [shareURL release];
                    shareURL = [publishedURL retain];
                    [self showShareURL:shareURL isError:NO];
                } else {
                    [self showShareURL:failure isError:YES];
                }
            });
        }];
    [task resume];
}

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response
{
    [receivedData setLength:0];
}


- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data
{
    [receivedData appendData:data];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    NSLog(@"Succeeded! Received %lu bytes of data",(unsigned long)[receivedData length]);

    NSString * responseString = [[[NSString alloc] initWithData:receivedData encoding:NSASCIIStringEncoding] autorelease];
    NSLog(@"RESPONSE STRING: %@", responseString);
    [receivedData release];
}

- (void)savePanelDidEnd:(NSSavePanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo {
    if (returnCode == NSFileHandlingPanelOKButton) {

        AppController *app = [[NSApplication sharedApplication] delegate];
        NSString *rawString = [app noteContent];
        NSString *processedString = [[[NSString alloc] init] autorelease];

        if ([app currentPreviewMode] == MarkdownPreview) {
            processedString = [NSString stringWithProcessedMarkdown:rawString];
        } else if ([app currentPreviewMode] == MultiMarkdownPreview) {
            processedString = ( [includeTemplate state] == NSOnState ) ? [NSString documentWithProcessedMultiMarkdown:rawString] : [NSString xhtmlWithProcessedMultiMarkdown:rawString];
        } else if ([app currentPreviewMode] == TextilePreview) {
            processedString = ( [includeTemplate state] == NSOnState ) ? [NSString documentWithProcessedTextile:rawString] : [NSString xhtmlWithProcessedTextile:rawString];
        }
        NSURL *file = [sheet URL];
        NSError *error;
        [processedString writeToURL:file atomically:YES encoding:NSUTF8StringEncoding error:&error];
    }
}

- (NoteObject *)noteShownInPreview
{
    if (self.isPreviewSticky && lastNote)
        return lastNote;
    return [(AppController *)[NSApp delegate] selectedNoteObject];
}

- (void)updateOpenInMarkedButton
{
    NoteObject *note = [self noteShownInPreview];
    BOOL show = NO;
    if (note && storageFormatOfNote(note) == PlainTextFormat)
        show = [[note noteFilePath] length] > 0;
    [saveButton setHidden:!show];
}

-(IBAction)openInMarked:(id)sender
{
    NoteObject *note = [self noteShownInPreview];
    if (!note || storageFormatOfNote(note) != PlainTextFormat) {
        NSBeep();
        return;
    }
    id notesController = [note delegate];
    if ([notesController respondsToSelector:@selector(synchronizeNoteChanges:)])
        [notesController synchronizeNoteChanges:nil];

    NSString *path = [note noteFilePath];
    if (![path length]) {
        NSBeep();
        return;
    }
    NSString *encodedPath = [path stringByAddingPercentEncodingWithAllowedCharacters:[NSCharacterSet URLQueryAllowedCharacterSet]];
    NSURL *url = encodedPath ? [NSURL URLWithString:[NSString stringWithFormat:@"x-marked-3://open?file=%@", encodedPath]] : nil;
    if (!url || ![[NSWorkspace sharedWorkspace] openURL:url]) {
        NSBeep();
        NSLog(@"Could not open note in Marked");
    }
}

-(IBAction)saveHTML:(id)sender
{
    if (!accessoryView) {
        if (![NSBundle loadNibNamed:@"SaveHTMLPreview" owner:self]) {
            NSLog(@"Failed to load SaveHTMLPreview.nib");
            NSBeep();
            return;
        }

    }
    // TODO high coupling; too many assumptions on architecture:
    AppController *app = [NSApp delegate];

    NSSavePanel *savePanel = [NSSavePanel savePanel];
    [savePanel setAccessoryView:accessoryView];
    [savePanel setCanCreateDirectories:YES];
    [savePanel setCanSelectHiddenExtension:YES];

    NSArray *fileTypes = [[NSArray alloc] initWithObjects:@"html",@"xhtml",@"htm",nil];
    [savePanel setAllowedFileTypes:fileTypes];


    NSString *rawString = [app noteContent];
    NSString *xhtmlOutput = [NSString xhtmlWithProcessedMultiMarkdown:rawString];
    if ([xhtmlOutput hasPrefix:@"<?xml version="]) {
        [includeTemplate setState:0];
        [includeTemplate setEnabled:NO];
        [templateNote setStringValue:@"Template embed unavailable because your note will render as a full XHTML document"];
    } else {
        [includeTemplate setEnabled:YES];
        [templateNote setStringValue:@"Select this to embed the ouput within your current preview HTML and CSS"];
    }

    NSString *noteTitle =  ([app selectedNoteObject]) ? [NSString stringWithFormat:@"%@",titleOfNote([app selectedNoteObject])] : @"";
    //	[savePanel beginSheetForDirectory:nil file:noteTitle modalForWindow:[self window] modalDelegate:self didEndSelector:@selector(savePanelDidEnd:returnCode:contextInfo:) contextInfo:nil];
    savePanel.nameFieldStringValue=noteTitle;
    [savePanel beginSheetModalForWindow:[self window] completionHandler:^(NSInteger returnCode) {
        if (returnCode == NSFileHandlingPanelOKButton) {
            NSString *processedString = [[[NSString alloc] init] autorelease];

            if ([app currentPreviewMode] == MarkdownPreview) {
                processedString = [NSString stringWithProcessedMarkdown:rawString];
            } else if ([app currentPreviewMode] == MultiMarkdownPreview) {
                processedString = ( [includeTemplate state] == NSOnState ) ? [NSString documentWithProcessedMultiMarkdown:rawString] : [NSString xhtmlWithProcessedMultiMarkdown:rawString];
            } else if ([app currentPreviewMode] == TextilePreview) {
                processedString = ( [includeTemplate state] == NSOnState ) ? [NSString documentWithProcessedTextile:rawString] : [NSString xhtmlWithProcessedTextile:rawString];
            }
            NSURL *file = [savePanel URL];
            NSError *error;
            [processedString writeToURL:file atomically:YES encoding:NSUTF8StringEncoding error:&error];
        }
    }];
    [fileTypes release];

}

-(IBAction)switchTabs:(id)sender
{

    if ([tabView indexOfTabViewItem:[tabView selectedTabViewItem]] == 0) {
        [tabSwitcher setTitle:@"View Preview"];
        [tabView selectTabViewItem:[tabView tabViewItemAtIndex:1]];
    } else {
        [tabSwitcher setTitle:@"View Source"];
        [tabView selectTabViewItem:[tabView tabViewItemAtIndex:0]];
    }
}

- (IBAction)shareAsk:(id)sender
{
    if (![self markedShareIsConnected]) {
        [self connectMarkedShare];
        [self showShareURL:@"Connect nvALT in the browser, then share again." isError:YES];
        return;
    }
    if (!confirmWindow && !attachedWindow) {
        int side = 3;
        NSPoint buttonPoint = NSMakePoint(NSMidX([shareButton frame]),
                                          NSMidY([shareButton frame]));
        confirmWindow = [[MAAttachedWindow alloc] initWithView:shareConfirmation
                                               attachedToPoint:buttonPoint
                                                      inWindow:[shareButton window]
                                                        onSide:side
                                                    atDistance:15.0f];
        [self applySystemAppearanceToPopup:confirmWindow];

        [[shareButton window] addChildWindow:confirmWindow ordered:NSWindowAbove];

    } else {
        if (confirmWindow)
            [self cancelShare:self];
        else if (attachedWindow)
            [self hideShareURL:self];
    }
}

- (void)showShareURL:(NSString *)url isError:(BOOL)isError
{
    if (confirmWindow) {
        [[shareButton window] removeChildWindow:confirmWindow];
        [confirmWindow orderOut:self];
        [confirmWindow release];
        confirmWindow = nil;
    }
    // Attach/detach window
    if (!attachedWindow) {
        int side = 3;
        NSPoint buttonPoint = NSMakePoint(NSMidX([shareButton frame]),
                                          NSMidY([shareButton frame]));
        attachedWindow = [[MAAttachedWindow alloc] initWithView:shareNotification
                                                attachedToPoint:buttonPoint
                                                       inWindow:[shareButton window]
                                                         onSide:side
                                                     atDistance:15.0f];
        [self applySystemAppearanceToPopup:attachedWindow];

        [[shareButton window] addChildWindow:attachedWindow ordered:NSWindowAbove];
        
    }
    
    if (isError) {
        [urlTextField setStringValue:url];
        [viewOnWebButton setHidden:YES];
    } else {
        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        NSArray *types = [NSArray arrayWithObjects:NSStringPboardType, nil];
        [pb declareTypes:types owner:self];
        [pb setString:shareURL forType:NSStringPboardType];
        [urlTextField setHidden:NO];
        [urlTextField setStringValue:[@"Copied " stringByAppendingString:[shareURL stringByAppendingString:@" to clipboard"]]];
        //[viewOnWebButton setTitle:url];
    }
    
    
}

-(void)closeShareURLView
{
    [[shareButton window] removeChildWindow:attachedWindow];
    [attachedWindow orderOut:self];
    [attachedWindow release];
    attachedWindow = nil;
    [shareURL release];
}

- (IBAction)hideShareURL:(id)sender
{
    [self closeShareURLView];
}

- (IBAction)cancelShare:(id)sender
{
    [[shareButton window] removeChildWindow:confirmWindow];
    [confirmWindow orderOut:self];
    [confirmWindow release];
    confirmWindow = nil;
}

- (IBAction)openShareURL:(id)sender
{
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:shareURL]];
    [[shareButton window] removeChildWindow:attachedWindow];
    [attachedWindow orderOut:self];
    [attachedWindow release];
    attachedWindow = nil;
    [shareURL release];
}

- (void)dealloc {
    [htmlString release];
    [cssString release];
    [lastNote release];
    [pendingScrollState release];
    [shareButton release];
    [saveButton release];
    [tabSwitcher release];
    [viewOnWebButton release];
    [shareCancel release];
    [shareConfirm release];
    [preview release];
    [super dealloc];
}

@end
