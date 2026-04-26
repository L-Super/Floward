#import <AppKit/AppKit.h>

int getClipboardChangeCount() {
    return static_cast<int>([NSPasteboard generalPasteboard].changeCount);
}
