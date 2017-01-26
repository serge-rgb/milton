#include <AppKit/AppKit.h>

// FileKind is temporarily redefined because when SDL is included in an Objective-C file
// it brings with it Carbon's old Rect type, which conflicts with Milton's Rect type.
enum FileKind
{
    FileKind_IMAGE,
    FileKind_MILTON_CANVAS,
    
    FileKind_COUNT,
};

namespace {
    template <typename T>
    char* runPanel(T *panel, FileKind kind) {
        switch (kind) {
            case FileKind_IMAGE:
                panel.allowedFileTypes = @[(NSString*)kUTTypeJPEG];
                break;
            case FileKind_MILTON_CANVAS:
                panel.allowedFileTypes = @[@"mlt"];
                break;
            default:
                break;
        }
        if ([panel runModal] != NSFileHandlingPanelOKButton) {
            return nullptr;
        }
        return strdup(panel.URL.path.fileSystemRepresentation);
    }
    
    NSAlert* alertWithInfoTitle(const char* info, const char* title) {
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = [NSString stringWithUTF8String:title ? title : ""];
        alert.informativeText = [NSString stringWithUTF8String:info ? info : ""];
        return alert;
    }
}

void
platform_dialog_mac(char* info, char* title)
{
    @autoreleasepool {
        [alertWithInfoTitle(info, title) runModal];
    }
}

int32_t
platform_dialog_yesno_mac(char* info, char* title)
{
    @autoreleasepool {
        NSAlert *alert = alertWithInfoTitle(info, title);
        [alert addButtonWithTitle:NSLocalizedString(@"Yes", nil)];
        [alert addButtonWithTitle:NSLocalizedString(@"No", nil)];
        return [alert runModal] == NSAlertFirstButtonReturn;
    }
}

char*
platform_open_dialog_mac(FileKind kind)
{
    @autoreleasepool {
        return runPanel([NSOpenPanel openPanel], kind);
    }
}

char*
platform_save_dialog_mac(FileKind kind)
{
    @autoreleasepool {
        return runPanel([NSSavePanel savePanel], kind);
    }
}

void
platform_open_link_mac(char* link)
{
    @autoreleasepool {
        NSURL *url = [NSURL URLWithString:[NSString stringWithUTF8String:link]];
        [[NSWorkspace sharedWorkspace] openURL:url];
    }
}

