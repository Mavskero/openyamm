#import "ios/ResourceImport.h"
#import <dispatch/dispatch.h>
#include <charconv>
#include <iostream>
#include <optional>
#include <string>

namespace
{
class ScopedConversionDiagnostics
{
public:
    ScopedConversionDiagnostics() : m_pOriginal(std::cout.rdbuf(std::cerr.rdbuf())) {}
    ~ScopedConversionDiagnostics() { std::cout.rdbuf(m_pOriginal); }
private:
    std::streambuf *m_pOriginal;
};

unsigned long long number(const char *pText)
{
    const std::string text(pText);
    unsigned long long value = 0;
    const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
        throw std::runtime_error("Expected a nonnegative integer");
    return value;
}
}

int main(int argc, char **argv)
{
    @autoreleasepool
    {
        int first = 1;
        std::optional<unsigned long long> cancelBytes;
        NSString *cancelStage = nil;
        unsigned long long delayMs = 0;
        bool cancelBeforeStart = false;
        bool cancelAfterPublication = false;
        try
        {
            while (first < argc)
            {
                const std::string option(argv[first]);
                if (option == "--cancel-before-start") cancelBeforeStart = true;
                else if (option == "--cancel-after-publication") cancelAfterPublication = true;
                else if (first + 1 < argc && option == "--cancel-copy-after")
                    cancelBytes = number(argv[++first]);
                else if (first + 1 < argc && option == "--cancel-convert-at")
                    cancelStage = [NSString stringWithUTF8String:argv[++first]];
                else if (first + 1 < argc && option == "--cancel-delay-ms")
                {
                    delayMs = number(argv[++first]);
                    if (delayMs > 60000) throw std::runtime_error("Cancellation delay exceeds 60 seconds");
                }
                else break;
                ++first;
            }
        }
        catch (const std::exception &error)
        {
            std::cerr << error.what() << '\n';
            return 2;
        }
        const bool recover = argc - first == 2 && std::string(argv[first]) == "--recover";
        const bool status = argc - first == 2 && std::string(argv[first]) == "--status";
        const bool convert = argc - first == 3 && std::string(argv[first]) == "--convert";
        if (argc - first != 2 && !convert)
        {
            std::cerr << "Usage: apple_import [OPTIONS] SOURCE_DIRECTORY SUPPORT_DIRECTORY\n"
                "       apple_import [OPTIONS] --convert SUPPORT_DIRECTORY GAMEPLAY_LAYOUT_DIRECTORY\n"
                "       apple_import --recover SUPPORT_DIRECTORY\n"
                "       apple_import --status SUPPORT_DIRECTORY\n"
                "Options: --cancel-before-start | --cancel-copy-after BYTES | --cancel-convert-at STAGE\n"
                "         --cancel-delay-ms MILLISECONDS | --cancel-after-publication\n";
            return 2;
        }
        NSError *error = nil;
        NSURL *support = [NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[first + 1]] isDirectory:YES];
        OYImportCancellation *cancellation = [[OYImportCancellation alloc] init];
        if (cancelBeforeStart) [cancellation cancel];
        NSDictionary *record = nil;
        if (status)
        {
            NSDictionary *active = [OYResourceImport activeImportInSupport:support];
            if (active == nil)
            {
                std::cerr << "No active import\n";
                return 1;
            }
            record = [OYResourceImport resourceStatusForImport:active inSupport:support];
        }
        else if (recover)
        {
            record = [OYResourceImport recoverInterruptedImportsInSupport:support error:&error];
        }
        else if (convert)
        {
            NSDictionary *active = [OYResourceImport activeImportInSupport:support];
            if (active == nil)
            {
                std::cerr << "No active source import\n";
                return 1;
            }
            ScopedConversionDiagnostics diagnostics;
            record = [OYResourceImport convertImport:active inSupport:support
                gameplayLayouts:[NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[first + 2]] isDirectory:YES]
                progress:^(NSString *stage, NSUInteger current, NSUInteger total)
                {
                    std::cerr << current << '/' << total << ' ' << stage.UTF8String << '\n';
                    if ([stage isEqual:cancelStage])
                    {
                        void (^requestCancellation)(void) = ^
                        {
                            const BOOL accepted = [cancellation cancel];
                            std::cerr << "cancel_accepted=" << accepted << " stage=" << stage.UTF8String << '\n';
                        };
                        if (delayMs == 0) requestCancellation();
                        else dispatch_after(dispatch_time(DISPATCH_TIME_NOW, delayMs * NSEC_PER_MSEC),
                            dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), requestCancellation);
                    }
                } cancellation:cancellation error:&error];
        }
        else
        {
            __block BOOL requested = NO;
            record = [OYResourceImport
                importDirectory:[NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[first]] isDirectory:YES]
                intoSupport:support progress:^(unsigned long long bytes, NSUInteger files)
                {
                    if (cancelBytes && bytes >= *cancelBytes && !requested)
                    {
                        requested = YES;
                        std::cerr << "cancel_accepted=" << [cancellation cancel]
                            << " bytes=" << bytes << " files=" << files << '\n';
                    }
                } cancellation:cancellation error:&error];
        }
        if (record == nil)
        {
            std::cerr << error.localizedDescription.UTF8String << '\n';
            return [error.domain isEqual:NSCocoaErrorDomain] && error.code == NSUserCancelledError ? 3 : 1;
        }
        if (cancelAfterPublication)
        {
            const BOOL accepted = [cancellation cancel];
            std::cerr << "post_publication_cancel_accepted=" << accepted << '\n';
            if (accepted) return 4;
        }
        NSData *json = [NSJSONSerialization dataWithJSONObject:record options:0 error:&error];
        std::cout.write(static_cast<const char *>(json.bytes), json.length);
        std::cout << '\n';
        return 0;
    }
}
