//
// macOS utility functions
//

#include "Util.h"

#include <QImage>
#include <QPixmap>
#include <QDebug>

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>

namespace utils {
QString GetFrontmostAppPath() {
    NSRunningApplication *app = [[NSWorkspace sharedWorkspace] frontmostApplication];
    if (app && app.bundleURL) {
        return QString::fromNSString(app.bundleURL.path);
    }
    return {};
}

QIcon GetAppIconFromBundle(const QString& appPath) {
    if (appPath.isEmpty()) {
        return {};
    }

    NSString *nsPath = appPath.toNSString();
    NSImage *nsIcon = [[NSWorkspace sharedWorkspace] iconForFile:nsPath];
    if (!nsIcon) {
        return {};
    }

    NSData *tiffData = [nsIcon TIFFRepresentation];
    if (!tiffData) {
        return {};
    }

    QImage img;
    img.loadFromData(QByteArray::fromRawNSData(tiffData), "TIFF");
    if (img.isNull()) {
        return {};
    }

    return QPixmap::fromImage(img);
}

std::optional<QRect> GetFocusCaretPosition() {
    // Get the frontmost application
    NSRunningApplication *frontApp = [[NSWorkspace sharedWorkspace] frontmostApplication];
    if (!frontApp) {
        return {};
    }

    pid_t pid = frontApp.processIdentifier;
    AXUIElementRef appElement = AXUIElementCreateApplication(pid);
    if (!appElement) {
        return {};
    }

    // Get focused element
    AXUIElementRef focusedElement = nullptr;
    AXError error = AXUIElementCopyAttributeValue(appElement, kAXFocusedUIElementAttribute,
                                                   (CFTypeRef *)&focusedElement);
    CFRelease(appElement);

    if (error != kAXErrorSuccess || !focusedElement) {
        return {};
    }

    // Try 1: Get caret position via AXSelectedTextRange + AXBoundsForRange
    CFTypeRef selectedRangeValue = nullptr;
    error = AXUIElementCopyAttributeValue(focusedElement, kAXSelectedTextRangeAttribute, &selectedRangeValue);

    if (error == kAXErrorSuccess && selectedRangeValue) {
        CFRange selectedRange;
        if (AXValueGetValue((AXValueRef)selectedRangeValue, (AXValueType)kAXValueCFRangeType, &selectedRange)) {
            // Use the cursor position (start of selection, length 0)
            CFRange caretRange = CFRangeMake(selectedRange.location, 0);
            AXValueRef caretRangeValue = AXValueCreate((AXValueType)kAXValueCFRangeType, &caretRange);

            if (caretRangeValue) {
                CFTypeRef boundsValue = nullptr;
                error = AXUIElementCopyParameterizedAttributeValue(focusedElement, kAXBoundsForRangeParameterizedAttribute,
                                                                   caretRangeValue, &boundsValue);
                CFRelease(caretRangeValue);

                if (error == kAXErrorSuccess && boundsValue) {
                    CGRect bounds;
                    if (AXValueGetValue((AXValueRef)boundsValue, (AXValueType)kAXValueCGRectType, &bounds)) {
                        CFRelease(boundsValue);
                        CFRelease(selectedRangeValue);
                        CFRelease(focusedElement);

                        QRect rect = QRect(static_cast<int>(bounds.origin.x), static_cast<int>(bounds.origin.y + bounds.size.height),
                                     static_cast<int>(bounds.size.width), static_cast<int>(bounds.size.height));
                        return rect;
                    }
                    CFRelease(boundsValue);
                }
            }
        }
        CFRelease(selectedRangeValue);
    }

    qDebug() << "macOS: all caret position methods failed";
    return {};
}
} // namespace utils
