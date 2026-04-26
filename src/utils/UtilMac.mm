//
// macOS utility functions
//

#include "Util.h"

#include <QImage>
#include <QPixmap>

#import <AppKit/AppKit.h>

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
} // namespace utils
