#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// One token per operation. Safe to cancel from the UI while a worker reads it.
@interface OYImportCancellation : NSObject
@property(nonatomic, readonly, getter=isCancelled) BOOL cancelled;
// Returns NO once atomic publication has started; the operation must then finish.
- (BOOL)cancel;
@end

@interface OYResourceImport : NSObject
// Run before launcher actions. A live import/conversion owns a cross-process lock and is never cleaned.
+ (nullable NSDictionary *)recoverInterruptedImportsInSupport:(NSURL *)support error:(NSError **)error;
+ (nullable NSDictionary *)activeImportInSupport:(NSURL *)support;
+ (nullable NSDictionary *)importDirectory:(NSURL *)source
                              intoSupport:(NSURL *)support
                                 progress:(nullable void (^)(unsigned long long bytes, NSUInteger files))progress
                             cancellation:(nullable OYImportCancellation *)cancellation
                                    error:(NSError **)error;
+ (NSURL *)contentURLForImport:(NSDictionary *)record support:(NSURL *)support;
+ (NSArray<NSString *> *)missingMM8BaseArchives:(NSURL *)source;
// Read-only, bounded assessment of known base-file/chest-background gaps. Not full gameplay validation.
+ (NSDictionary *)resourceStatusForImport:(NSDictionary *)record inSupport:(NSURL *)support;
+ (nullable NSDictionary *)convertImport:(NSDictionary *)record
                              inSupport:(NSURL *)support
                        gameplayLayouts:(NSURL *)layouts
                               progress:(nullable void (^)(NSString *stage,
                                                           NSUInteger current, NSUInteger total))progress
                           cancellation:(nullable OYImportCancellation *)cancellation
                                  error:(NSError **)error;
@end

NS_ASSUME_NONNULL_END
