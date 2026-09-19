#import "ResourceImport.h"
#include "tools/mmerge_import/RuntimeResources.h"
#include <cstring>
#include <filesystem>
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <array>
#include <atomic>

@interface OYImportCancellation ()
- (BOOL)beginPublication;
@end

@implementation OYImportCancellation
{
    std::atomic<int> m_state; // Running, cancelled, publishing.
}
- (instancetype)init
{
    self = [super init];
    if (self != nil) m_state.store(0);
    return self;
}
- (BOOL)cancel
{
    int expected = 0;
    return m_state.compare_exchange_strong(expected, 1) || expected == 1;
}
- (BOOL)isCancelled
{
    return m_state.load() == 1;
}
- (BOOL)beginPublication
{
    int expected = 0;
    return m_state.compare_exchange_strong(expected, 2);
}
@end

namespace
{
constexpr unsigned long long MaximumImportBytes = 8ULL * 1024 * 1024 * 1024;
constexpr NSUInteger MaximumImportFiles = 100000;

NSError *importError(NSString *message)
{
    return [NSError errorWithDomain:@"org.openyamm.import" code:1
        userInfo:@{NSLocalizedDescriptionKey: message}];
}

NSError *cancellationError()
{
    return [NSError errorWithDomain:NSCocoaErrorDomain code:NSUserCancelledError
        userInfo:@{NSLocalizedDescriptionKey: @"已取消，原有资源保持不变。"}];
}

bool checkCancelled(OYImportCancellation *cancellation, NSError **error)
{
    if (!cancellation.isCancelled) return false;
    *error = cancellationError();
    return true;
}

class ImportFile
{
public:
    explicit ImportFile(int descriptor) : m_descriptor(descriptor) {}
    ~ImportFile() { if (m_descriptor >= 0) close(m_descriptor); }
    ImportFile(const ImportFile &) = delete;
    ImportFile &operator=(const ImportFile &) = delete;
    int descriptor() const { return m_descriptor; }
private:
    int m_descriptor;
};

bool copyResource(NSURL *source, NSURL *target, unsigned long long expectedSize,
    unsigned long long &bytes, NSUInteger completedFiles,
    void (^progress)(unsigned long long, NSUInteger), OYImportCancellation *cancellation, NSError **error)
{
    if (checkCancelled(cancellation, error)) return false;
    ImportFile input(open(source.path.fileSystemRepresentation, O_RDONLY | O_NOFOLLOW | O_CLOEXEC));
    if (input.descriptor() < 0)
    {
        *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:nil];
        return false;
    }
    struct stat status = {};
    if (fstat(input.descriptor(), &status) != 0 || !S_ISREG(status.st_mode)
        || status.st_size < 0 || static_cast<unsigned long long>(status.st_size) != expectedSize)
    {
        *error = importError(@"源文件在导入时发生变化，请检查后重新导入。");
        return false;
    }
    // Resources are data only; no copied Windows file is executable in the app.
    ImportFile output(open(target.path.fileSystemRepresentation,
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0644));
    if (output.descriptor() < 0)
    {
        *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:nil];
        return false;
    }
    std::array<char, 65536> buffer;
    unsigned long long copied = 0;
    for (;;)
    {
        if (checkCancelled(cancellation, error)) return false;
        const ssize_t count = read(input.descriptor(), buffer.data(), buffer.size());
        if (count < 0)
        {
            if (errno == EINTR) continue;
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:nil];
            return false;
        }
        if (count == 0) break;
        if (static_cast<unsigned long long>(count) > expectedSize - copied)
        {
            *error = importError(@"源文件在导入时发生变化，请检查后重新导入。");
            return false;
        }
        for (ssize_t written = 0; written < count;)
        {
            if (checkCancelled(cancellation, error)) return false;
            const ssize_t result = write(output.descriptor(), buffer.data() + written, count - written);
            if (result <= 0)
            {
                if (result < 0 && errno == EINTR) continue;
                *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:result < 0 ? errno : EIO userInfo:nil];
                return false;
            }
            written += result;
        }
        copied += count;
        bytes += count;
        if (progress != nil) progress(bytes, completedFiles);
    }
    if (copied != expectedSize)
    {
        *error = importError(@"源文件未能完整读取，请检查后重新导入。");
        return false;
    }
    if (progress != nil) progress(bytes, completedFiles + 1);
    return !checkCancelled(cancellation, error);
}

bool hasFile(NSURL *root, NSString *name)
{
    BOOL directory = NO;
    return [[NSFileManager defaultManager] fileExistsAtPath:
        [root URLByAppendingPathComponent:name].path isDirectory:&directory] && !directory;
}

NSArray<NSString *> *baseArchivePaths()
{
    return @[@"Data/bitmaps.lod", @"Data/sprites.lod", @"Data/icons.lod"];
}

NSArray<NSString *> *chestBackgroundPaths()
{
    NSMutableArray<NSString *> *paths = [NSMutableArray array];
    for (NSUInteger index = 1; index <= 8; ++index)
    {
        [paths addObject:[NSString stringWithFormat:@"icons/chest0%lu.bmp", (unsigned long)index]];
    }
    return paths;
}

bool validMissingPaths(id value, NSArray<NSString *> *allowed)
{
    if (![value isKindOfClass:NSArray.class] || [value count] > allowed.count)
    {
        return false;
    }
    NSMutableSet *seen = [NSMutableSet set];
    for (id path in value)
    {
        if (![path isKindOfClass:NSString.class] || ![allowed containsObject:path] || [seen containsObject:path])
        {
            return false;
        }
        [seen addObject:path];
    }
    return true;
}

NSDictionary *normalizedResourceAudit(id value)
{
    if (![value isKindOfClass:NSDictionary.class] || ![value[@"schema"] isEqual:@1])
    {
        return nil;
    }
    for (NSString *key in @[@"base_archives_checked", @"chest_backgrounds_checked"])
    {
        if (![value[key] isKindOfClass:NSNumber.class]
            || (![(NSNumber *)value[key] isEqual:@YES] && ![(NSNumber *)value[key] isEqual:@NO]))
        {
            return nil;
        }
    }
    if (!validMissingPaths(value[@"missing_mm8_base_archives"], baseArchivePaths())
        || !validMissingPaths(value[@"missing_chest_backgrounds"], chestBackgroundPaths()))
    {
        return nil;
    }
    if ((![value[@"base_archives_checked"] boolValue] && [value[@"missing_mm8_base_archives"] count] > 0)
        || (![value[@"chest_backgrounds_checked"] boolValue] && [value[@"missing_chest_backgrounds"] count] > 0))
    {
        return nil;
    }
    // Store only the measured profile, never arbitrary presentation text from a portable manifest.
    return @{@"schema": @1, @"base_archives_checked": value[@"base_archives_checked"],
        @"chest_backgrounds_checked": value[@"chest_backgrounds_checked"],
        @"missing_mm8_base_archives": value[@"missing_mm8_base_archives"],
        @"missing_chest_backgrounds": value[@"missing_chest_backgrounds"]};
}

NSDictionary *sourceResourceAudit(NSURL *source)
{
    NSMutableArray<NSString *> *missing = [NSMutableArray array];
    for (NSString *path in baseArchivePaths())
    {
        if (!hasFile(source, path))
        {
            [missing addObject:path];
        }
    }
    return @{@"schema": @1, @"base_archives_checked": @YES, @"chest_backgrounds_checked": @NO,
        @"missing_mm8_base_archives": missing, @"missing_chest_backgrounds": @[]};
}

NSDictionary *runtimeResourceAudit(NSURL *root)
{
    NSURL *url = [root URLByAppendingPathComponent:@"conversion.json"];
    NSDictionary *attributes = [NSFileManager.defaultManager attributesOfItemAtPath:url.path error:nil];
    if (![attributes[NSFileType] isEqual:NSFileTypeRegular]
        || [attributes[NSFileSize] unsignedLongLongValue] > 1024 * 1024)
    {
        return nil;
    }
    NSData *data = [NSData dataWithContentsOfURL:url options:0 error:nil];
    id manifest = data == nil ? nil : [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
    return [manifest isKindOfClass:NSDictionary.class] ? normalizedResourceAudit(manifest[@"resource_audit"]) : nil;
}

NSString *resourceKind(NSURL *source, NSError **error)
{
    if (hasFile(source, @"engine.zip") && hasFile(source, @"worlds/mm6.zip")
        && hasFile(source, @"worlds/mm7.zip") && hasFile(source, @"worlds/mm8.zip"))
    {
        return @"runtime";
    }
    NSMutableArray<NSString *> *required = [NSMutableArray arrayWithObjects:
        @"Data/mmmerge.T.lod", @"Data/mmmerge.D.lod", nil];
    for (NSNumber *version in @[@6, @7, @8])
    {
        for (NSString *kind in @[@"games", @"icons", @"bitmaps"])
        {
            [required addObject:[NSString stringWithFormat:@"Data/mm%@.%@.lod", version, kind]];
        }
    }
    for (NSString *name in required)
    {
        if (!hasFile(source, name))
        {
            *error = importError([NSString stringWithFormat:
                @"目录中缺少 %@。请选择完整的 MMMerge 目录，或包含 engine.zip 和 worlds/mm6、mm7、mm8.zip 的转换目录。",
                name]);
            return nil;
        }
        NSFileHandle *file = [NSFileHandle fileHandleForReadingFromURL:
            [source URLByAppendingPathComponent:name] error:error];
        if (file == nil)
        {
            return nil;
        }
        NSData *magic = [file readDataUpToLength:4 error:error];
        [file closeFile];
        const char expected[] = {'L', 'O', 'D', 0};
        if (magic.length != 4 || memcmp(magic.bytes, expected, 4) != 0)
        {
            *error = importError([NSString stringWithFormat:@"%@ 不是有效的 LOD 资源包。", name]);
            return nil;
        }
    }
    return @"source";
}
bool validIdentifier(NSString *identifier)
{
    return [identifier isKindOfClass:NSString.class] && [[NSUUID alloc] initWithUUIDString:identifier] != nil;
}

bool realDirectory(NSURL *url)
{
    struct stat status = {};
    return lstat(url.path.fileSystemRepresentation, &status) == 0 && S_ISDIR(status.st_mode);
}

class ImportLock
{
public:
    ImportLock(NSURL *support, NSError **error)
    {
        m_imports = [support URLByAppendingPathComponent:@"GameImports" isDirectory:YES];
        if (![NSFileManager.defaultManager createDirectoryAtURL:m_imports
            withIntermediateDirectories:YES attributes:nil error:error]) return;
        if (!realDirectory(m_imports))
        {
            *error = importError(@"应用资源存储目录不是普通文件夹。");
            return;
        }
        NSURL *lock = [m_imports URLByAppendingPathComponent:@".operation.lock"];
        m_descriptor = open(lock.path.fileSystemRepresentation, O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600);
        if (m_descriptor < 0 || flock(m_descriptor, LOCK_EX | LOCK_NB) != 0)
        {
            *error = importError(@"资源正在导入或检查，请稍后再试。");
            if (m_descriptor >= 0) close(m_descriptor);
            m_descriptor = -1;
        }
    }
    ~ImportLock()
    {
        if (m_descriptor >= 0) close(m_descriptor);
    }
    ImportLock(const ImportLock &) = delete;
    ImportLock &operator=(const ImportLock &) = delete;
    bool acquired() const { return m_descriptor >= 0; }
    NSURL *imports() const { return m_imports; }
private:
    int m_descriptor = -1;
    NSURL *m_imports;
};

bool removeIfPresent(NSURL *url)
{
    NSError *failure = nil;
    return [NSFileManager.defaultManager removeItemAtURL:url error:&failure]
        || (failure.code == NSFileNoSuchFileError && [failure.domain isEqual:NSCocoaErrorDomain]);
}

NSDictionary *recoverImports(NSURL *imports, NSError **error)
{
    NSFileManager *manager = NSFileManager.defaultManager;
    NSArray<NSURL *> *entries = [manager contentsOfDirectoryAtURL:imports
        includingPropertiesForKeys:nil options:0 error:error];
    if (entries == nil) return nil;
    NSURL *activeURL = [imports URLByAppendingPathComponent:@"active.json"];
    NSData *activeData = [NSData dataWithContentsOfURL:activeURL];
    NSDictionary *active = activeData == nil ? nil
        : [NSJSONSerialization JSONObjectWithData:activeData options:0 error:nil];
    const bool selectionReadable = ![manager fileExistsAtPath:activeURL.path]
        || ([active isKindOfClass:NSDictionary.class] && validIdentifier(active[@"id"])
            && [@[@"source", @"runtime"] containsObject:active[@"kind"]]);
    NSMutableSet<NSString *> *protectedIds = [NSMutableSet set];
    if ([active isKindOfClass:NSDictionary.class])
    {
        for (NSString *key in @[@"id", @"source_id"])
            if (validIdentifier(active[key])) [protectedIds addObject:[active[key] uppercaseString]];
    }
    NSUInteger removed = 0;
    NSUInteger finished = 0;
    NSUInteger preserved = 0;
    for (NSURL *entry in entries)
    {
        NSString *name = entry.lastPathComponent;
        if (![name hasPrefix:@".pending-"] || ![name hasSuffix:@".json"]) continue;
        NSString *identifier = [name substringWithRange:NSMakeRange(9, name.length - 14)];
        struct stat status = {};
        if (!validIdentifier(identifier) || lstat(entry.path.fileSystemRepresentation, &status) != 0
            || !S_ISREG(status.st_mode))
        {
            ++preserved;
            continue;
        }
        NSData *data = [NSData dataWithContentsOfURL:entry];
        NSDictionary *pending = data == nil ? nil : [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
        if (![pending isKindOfClass:NSDictionary.class] || ![pending[@"schema"] isEqual:@1]
            || ![pending[@"id"] isEqual:identifier]
            || ![@[@"copy", @"convert"] containsObject:pending[@"operation"]])
        {
            ++preserved;
            continue;
        }
        NSURL *staging = [imports URLByAppendingPathComponent:[@".staging-" stringByAppendingString:identifier]];
        NSURL *destination = [imports URLByAppendingPathComponent:identifier];
        const bool protectDestination = [protectedIds containsObject:identifier.uppercaseString] || !selectionReadable;
        bool unresolved = false;
        for (NSURL *directory in protectDestination ? @[staging] : @[staging, destination])
        {
            if (lstat(directory.path.fileSystemRepresentation, &status) != 0)
            {
                if (errno == ENOENT) continue;
                unresolved = true;
                continue;
            }
            if (!S_ISDIR(status.st_mode))
            {
                unresolved = true;
                continue;
            }
            if (![manager removeItemAtURL:directory error:error]) return nil;
            ++removed;
        }
        if (unresolved || (!selectionReadable && [manager fileExistsAtPath:destination.path]))
        {
            ++preserved;
            continue;
        }
        if (![manager removeItemAtURL:entry error:error]) return nil;
        ++finished;
    }
    // Older copies already used this reserved staging namespace. Unmarked UUID directories
    // may be completed legacy imports or conversions; never infer that they are disposable.
    for (NSURL *entry in entries)
    {
        NSString *name = entry.lastPathComponent;
        if (![name hasPrefix:@".staging-"] || !validIdentifier([name substringFromIndex:9])) continue;
        NSURL *journal = [imports URLByAppendingPathComponent:
            [NSString stringWithFormat:@".pending-%@.json", [name substringFromIndex:9]]];
        if ([manager fileExistsAtPath:journal.path]) continue;
        struct stat status = {};
        if (lstat(entry.path.fileSystemRepresentation, &status) != 0) continue;
        if (!S_ISDIR(status.st_mode))
        {
            ++preserved;
            continue;
        }
        if (![manager removeItemAtURL:entry error:error]) return nil;
        ++removed;
    }
    return @{@"schema": @1, @"removed_incomplete_directories": @(removed),
        @"finished_operations": @(finished), @"preserved_unrecognized_operations": @(preserved)};
}

class ImportTransaction
{
public:
    ImportTransaction(NSURL *imports, NSString *operation, NSError **error) : m_imports(imports)
    {
        m_identifier = NSUUID.UUID.UUIDString;
        m_staging = [imports URLByAppendingPathComponent:[@".staging-" stringByAppendingString:m_identifier]];
        m_destination = [imports URLByAppendingPathComponent:m_identifier];
        m_journal = [imports URLByAppendingPathComponent:
            [NSString stringWithFormat:@".pending-%@.json", m_identifier]];
        NSDictionary *pending = @{@"schema": @1, @"id": m_identifier, @"operation": operation};
        NSData *json = [NSJSONSerialization dataWithJSONObject:pending options:0 error:error];
        m_started = json != nil && [json writeToURL:m_journal options:NSDataWritingAtomic error:error];
    }
    ~ImportTransaction()
    {
        if (!m_started) return;
        const bool cleaned = m_committed || (removeIfPresent(m_staging) && removeIfPresent(m_destination));
        // Keep the journal if cleanup failed, so a later launch can finish it.
        if (cleaned) removeIfPresent(m_journal);
    }
    ImportTransaction(const ImportTransaction &) = delete;
    ImportTransaction &operator=(const ImportTransaction &) = delete;
    bool started() const { return m_started; }
    NSString *identifier() const { return m_identifier; }
    NSURL *staging() const { return m_staging; }
    bool publish(NSDictionary *record, OYImportCancellation *cancellation, NSError **error)
    {
        NSData *json = [NSJSONSerialization dataWithJSONObject:record options:NSJSONWritingPrettyPrinted error:error];
        if (json == nil) return false;
        if (cancellation != nil && ![cancellation beginPublication])
        {
            *error = cancellationError();
            return false;
        }
        if (![NSFileManager.defaultManager moveItemAtURL:m_staging toURL:m_destination error:error])
            return false;
        m_committed = [json writeToURL:[m_imports URLByAppendingPathComponent:@"active.json"]
            options:NSDataWritingAtomic error:error];
        return m_committed;
    }
private:
    NSURL *m_imports;
    NSString *m_identifier;
    NSURL *m_staging;
    NSURL *m_destination;
    NSURL *m_journal;
    bool m_started = false;
    bool m_committed = false;
};
}

@implementation OYResourceImport

+ (NSDictionary *)recoverInterruptedImportsInSupport:(NSURL *)support error:(NSError **)error
{
    ImportLock lock(support, error);
    return lock.acquired() ? recoverImports(lock.imports(), error) : nil;
}

+ (NSArray<NSString *> *)missingMM8BaseArchives:(NSURL *)source
{
    return sourceResourceAudit(source)[@"missing_mm8_base_archives"];
}

+ (NSDictionary *)resourceStatusForImport:(NSDictionary *)record inSupport:(NSURL *)support
{
    NSURL *content = [self contentURLForImport:record support:support];
    const BOOL source = [record[@"kind"] isEqual:@"source"];
    NSDictionary *audit = source ? sourceResourceAudit(content) : normalizedResourceAudit(record[@"resource_audit"]);
    if (audit == nil && !source)
    {
        audit = runtimeResourceAudit(content);
    }
    if (audit == nil && validMissingPaths(record[@"missing_mm8_base_archives"], baseArchivePaths())
        && [record[@"missing_mm8_base_archives"] count] > 0)
    {
        // Old native conversions recorded these known gaps before portable audits existed.
        audit = @{@"schema": @1, @"base_archives_checked": @YES, @"chest_backgrounds_checked": @NO,
            @"missing_mm8_base_archives": record[@"missing_mm8_base_archives"], @"missing_chest_backgrounds": @[]};
    }
    audit = audit ?: @{@"schema": @1, @"base_archives_checked": @NO, @"chest_backgrounds_checked": @NO,
        @"missing_mm8_base_archives": @[], @"missing_chest_backgrounds": @[]};
    NSArray *missingBase = audit[@"missing_mm8_base_archives"];
    NSArray *missingChests = audit[@"missing_chest_backgrounds"];
    NSMutableArray<NSString *> *messages = [NSMutableArray array];
    if (missingBase.count > 0)
    {
        [messages addObject:@"缺少 MM8 原版基础资源。请将完整 MM8 安装目录与 Merge 合并后重新导入。"];
    }
    if (missingChests.count > 0)
    {
        [messages addObject:[NSString stringWithFormat:@"缺少 %lu 张宝箱背景，宝箱画面尚不可正常显示。",
            (unsigned long)missingChests.count]];
    }
    if (messages.count == 0)
    {
        if ([audit[@"base_archives_checked"] boolValue] && [audit[@"chest_backgrounds_checked"] boolValue])
        {
            [messages addObject:@"转换记录未报告基础档案或宝箱背景缺项；其它素材与完整玩法仍在验证。"];
        }
        else if (source)
        {
            [messages addObject:@"基础档案已找到，转换时将继续检查宝箱图像。"];
        }
        else
        {
            [messages addObject:@"此转换目录没有可用的资源缺项记录，素材完整性尚未核查。"];
        }
    }
    NSMutableDictionary *status = [audit mutableCopy];
    status[@"has_known_gaps"] = @(missingBase.count > 0 || missingChests.count > 0);
    status[@"message"] = [messages componentsJoinedByString:@"\n"];
    return status;
}

+ (NSURL *)contentURLForImport:(NSDictionary *)record support:(NSURL *)support
{
    return [[support URLByAppendingPathComponent:@"GameImports"]
        URLByAppendingPathComponent:record[@"id"] isDirectory:YES];
}

+ (NSDictionary *)activeImportInSupport:(NSURL *)support
{
    NSURL *active = [support URLByAppendingPathComponent:@"GameImports/active.json"];
    NSData *data = [NSData dataWithContentsOfURL:active];
    if (data == nil)
    {
        return nil;
    }
    NSDictionary *record = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
    if (![record isKindOfClass:NSDictionary.class] || ![record[@"id"] isKindOfClass:NSString.class]
        || [[NSUUID alloc] initWithUUIDString:record[@"id"]] == nil
        || ![@[@"source", @"runtime"] containsObject:record[@"kind"]])
    {
        return nil;
    }
    BOOL isDirectory = NO;
    if (![[NSFileManager defaultManager] fileExistsAtPath:
        [self contentURLForImport:record support:support].path isDirectory:&isDirectory] || !isDirectory)
    {
        return nil;
    }
    return record;
}

+ (NSDictionary *)importDirectory:(NSURL *)source intoSupport:(NSURL *)support
                         progress:(void (^)(unsigned long long, NSUInteger))progress
                     cancellation:(OYImportCancellation *)cancellation error:(NSError **)error
{
    if (checkCancelled(cancellation, error)) return nil;
    if (progress != nil) progress(0, 0);
    if (checkCancelled(cancellation, error)) return nil;
    NSFileManager *manager = [NSFileManager defaultManager];
    source = source.URLByStandardizingPath;
    support = support.URLByStandardizingPath;
    NSNumber *isDirectory = nil;
    NSNumber *isLink = nil;
    if (![source getResourceValue:&isDirectory forKey:NSURLIsDirectoryKey error:error]
        || ![source getResourceValue:&isLink forKey:NSURLIsSymbolicLinkKey error:error])
    {
        return nil;
    }
    std::error_code pathError;
    const std::filesystem::path sourceCanonical =
        std::filesystem::weakly_canonical(source.path.UTF8String, pathError);
    if (pathError)
    {
        *error = importError(@"无法解析资源目录路径。");
        return nil;
    }
    const std::filesystem::path supportCanonical =
        std::filesystem::weakly_canonical(support.path.UTF8String, pathError);
    if (pathError)
    {
        *error = importError(@"无法解析应用存储路径。");
        return nil;
    }
    NSString *sourcePath = [NSString stringWithUTF8String:sourceCanonical.c_str()];
    NSString *supportPath = [NSString stringWithUTF8String:supportCanonical.c_str()];
    if (!isDirectory.boolValue || isLink.boolValue
        || [sourcePath isEqual:supportPath]
        || [supportPath hasPrefix:[sourcePath stringByAppendingString:@"/"]]
        || [sourcePath hasPrefix:[supportPath stringByAppendingString:@"/"]])
    {
        *error = importError(@"请选择独立的游戏资源目录，不能使用应用的内部导入目录或符号链接。");
        return nil;
    }
    NSString *kind = resourceKind(source, error);
    if (kind == nil)
    {
        return nil;
    }
    ImportLock lock(support, error);
    if (!lock.acquired() || recoverImports(lock.imports(), error) == nil) return nil;
    ImportTransaction transaction(lock.imports(), @"copy", error);
    if (!transaction.started()) return nil;
    NSString *identifier = transaction.identifier();
    NSURL *staging = transaction.staging();
    NSArray *keys = @[NSURLIsRegularFileKey, NSURLIsDirectoryKey, NSURLIsSymbolicLinkKey, NSURLFileSizeKey];
    __block NSError *enumerationError = nil;
    NSDirectoryEnumerator<NSURL *> *enumerator = [manager enumeratorAtURL:source
        includingPropertiesForKeys:keys options:NSDirectoryEnumerationSkipsHiddenFiles
        errorHandler:^BOOL(NSURL *url, NSError *failure)
        {
            enumerationError = failure;
            return NO;
        }];
    if (enumerator == nil)
    {
        *error = importError(@"无法读取选择的目录。");
        return nil;
    }
    if (![manager createDirectoryAtURL:staging withIntermediateDirectories:NO attributes:nil error:error])
    {
        return nil;
    }
    NSUInteger files = 0;
    unsigned long long bytes = 0;
    {
        for (NSURL *file in enumerator)
        {
            if (checkCancelled(cancellation, error)) return nil;
            NSDictionary *values = [file resourceValuesForKeys:keys error:error];
            if (values == nil)
            {
                return nil;
            }
            if ([values[NSURLIsSymbolicLinkKey] boolValue])
            {
                *error = importError(@"目录包含符号链接，请使用完整的本地资源副本。");
                return nil;
            }
            if ([values[NSURLIsDirectoryKey] boolValue])
            {
                continue;
            }
            if (![values[NSURLIsRegularFileKey] boolValue])
            {
                *error = importError(@"目录包含无法导入的特殊文件。");
                return nil;
            }
            // Foundation may enumerate /private/var for a /var URL. Resolve both paths
            // before deriving the relative name, rather than dropping a fixed component count.
            const std::filesystem::path relativePath =
                std::filesystem::relative(file.path.UTF8String, sourceCanonical, pathError);
            if (pathError || relativePath.empty() || relativePath.is_absolute() || *relativePath.begin() == "..")
            {
                *error = importError(@"资源文件路径超出选择的目录。");
                return nil;
            }
            NSString *relative = [NSString stringWithUTF8String:relativePath.c_str()];
            const BOOL legacyDataExecutable = [kind isEqualToString:@"source"]
                && relative.pathComponents.count == 1 && [relative.lowercaseString isEqualToString:@"mm8.exe"];
            if ([kind isEqualToString:@"source"])
            {
                NSString *top = [relative.pathComponents.firstObject lowercaseString];
                // Preserve this one root file as input data for the fixed-profile
                // table reader. The port never loads or executes Windows code.
                const BOOL resourceFolder = [@[@"data", @"music", @"sounds", @"anims", @"scripts"] containsObject:top];
                if (!legacyDataExecutable && (!resourceFolder
                    || [@[@"exe", @"dll"] containsObject:file.pathExtension.lowercaseString]))
                {
                    continue;
                }
                if (legacyDataExecutable)
                {
                    relative = @"mm8.exe";
                }
            }
            else if (!([relative isEqual:@"engine.zip"] || [relative isEqual:@"assets.zip"]
                || [relative isEqual:@"conversion.json"]
                || (relative.pathComponents.count == 2 && [relative.pathComponents.firstObject isEqual:@"worlds"]
                    && [file.pathExtension isEqual:@"zip"])))
            {
                continue;
            }
            unsigned long long size = [values[NSURLFileSizeKey] unsignedLongLongValue];
            if (++files > MaximumImportFiles || size > MaximumImportBytes - bytes)
            {
                *error = importError(@"资源超过当前导入上限（8 GB 或 100,000 个文件）。");
                return nil;
            }
            NSURL *target = [staging URLByAppendingPathComponent:relative];
            if (![manager createDirectoryAtURL:target.URLByDeletingLastPathComponent
                withIntermediateDirectories:YES attributes:nil error:error]
                || !copyResource(file, target, size, bytes, files - 1, progress, cancellation, error))
            {
                return nil;
            }
            if (legacyDataExecutable && ![manager setAttributes:@{NSFilePosixPermissions: @0644}
                ofItemAtPath:target.path error:error])
            {
                return nil;
            }
        }
        if (enumerationError != nil)
        {
            *error = enumerationError;
            return nil;
        }
        if (resourceKind(staging, error) == nil)
        {
            return nil;
        }
        NSMutableDictionary *record = [@{@"schema": @1, @"id": identifier, @"kind": kind,
            @"name": source.lastPathComponent, @"files": @(files), @"bytes": @(bytes),
            @"missing_mm8_base_archives": [kind isEqual:@"source"] ? [self missingMM8BaseArchives:staging] : @[],
            @"runtime_validated": @NO} mutableCopy];
        NSDictionary *audit = [kind isEqual:@"source"] ? sourceResourceAudit(staging) : runtimeResourceAudit(staging);
        if (audit != nil)
        {
            record[@"resource_audit"] = audit;
        }
        return transaction.publish(record, cancellation, error) ? record : nil;
    }
}

+ (NSDictionary *)convertImport:(NSDictionary *)record
                      inSupport:(NSURL *)support
                gameplayLayouts:(NSURL *)layouts
                       progress:(void (^)(NSString *, NSUInteger, NSUInteger))progress
                   cancellation:(OYImportCancellation *)cancellation
                          error:(NSError **)error
{
    if (checkCancelled(cancellation, error)) return nil;
    ImportLock lock(support, error);
    if (!lock.acquired() || recoverImports(lock.imports(), error) == nil) return nil;
    NSDictionary *active = [self activeImportInSupport:support];
    if (![record[@"kind"] isEqual:@"source"] || ![active isEqual:record])
    {
        *error = importError(@"原始资源记录已改变，请重新选择要转换的目录。");
        return nil;
    }
    NSURL *source = [self contentURLForImport:record support:support];
    ImportTransaction transaction(lock.imports(), @"convert", error);
    if (!transaction.started()) return nil;
    NSString *identifier = transaction.identifier();
    NSURL *destination = transaction.staging();
    {
        NSUInteger files = 0;
        unsigned long long bytes = 0;
        try
        {
            OpenYAMM::Import::convertRuntimeResources(source.path.UTF8String, layouts.path.UTF8String,
                destination.path.UTF8String, [progress](const std::string &stage, size_t current, size_t total)
                {
                    if (progress != nil)
                    {
                        progress([NSString stringWithUTF8String:stage.c_str()], current, total);
                    }
                }, [cancellation] { return cancellation.isCancelled == YES; });
            for (const std::filesystem::directory_entry &entry :
                std::filesystem::recursive_directory_iterator(destination.path.UTF8String))
            {
                OpenYAMM::Import::checkImportCancellation([cancellation] { return cancellation.isCancelled == YES; });
                if (entry.is_regular_file())
                {
                    ++files;
                    bytes += entry.file_size();
                }
            }
        }
        catch (const OpenYAMM::Import::ImportCancelled &)
        {
            *error = cancellationError();
            return nil;
        }
        catch (const std::exception &failure)
        {
            NSString *detail = [NSString stringWithUTF8String:failure.what()];
            *error = importError([NSString stringWithFormat:@"资源转换未完成：%@。原始资源仍然保留。", detail]);
            return nil;
        }
        NSURL *manifestURL = [destination URLByAppendingPathComponent:@"conversion.json"];
        NSData *manifestData = [NSData dataWithContentsOfURL:manifestURL
            options:0 error:error];
        NSDictionary *manifest = manifestData == nil ? nil
            : [NSJSONSerialization JSONObjectWithData:manifestData options:0 error:error];
        if (![manifest isKindOfClass:NSDictionary.class]
            || ![manifest[@"converted_maps"] isKindOfClass:NSArray.class]
            || ![manifest[@"converted_maps_load_validated"] isEqual:@YES]
            || normalizedResourceAudit(manifest[@"resource_audit"]) == nil)
        {
            *error = importError(@"转换后的场景验证记录不完整，原始资源仍然保留。");
            return nil;
        }
        NSDictionary *converted = @{@"schema": @1, @"id": identifier, @"kind": @"runtime",
            @"name": record[@"name"], @"source_id": record[@"id"], @"files": @(files), @"bytes": @(bytes),
            @"runtime_validated": @NO, @"common_data_validated": @YES, @"starting_map_load_validated": @YES,
            @"converted_maps": manifest[@"converted_maps"], @"default_world": manifest[@"default_world"],
            @"converted_maps_load_validated": @YES,
            @"resource_audit": normalizedResourceAudit(manifest[@"resource_audit"]),
            @"missing_mm8_base_archives": record[@"missing_mm8_base_archives"] ?: @[]};
        if (![[self activeImportInSupport:support] isEqual:record])
        {
            *error = importError(@"转换期间已选择其他资源，当前选择没有被替换。");
            return nil;
        }
        return transaction.publish(converted, cancellation, error) ? converted : nil;
    }
}
@end
