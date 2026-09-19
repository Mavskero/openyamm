#import "Launcher.h"
#import "ResourceImport.h"
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <SDL3/SDL.h>

@interface OYLauncherController : UIViewController <UIDocumentPickerDelegate>
@property(nonatomic, strong) NSURL *support;
@property(nonatomic, strong) NSDictionary *record;
@property(nonatomic, strong) UILabel *statusLabel;
@property(nonatomic, strong) UIButton *importButton;
@property(nonatomic, strong) UIButton *playButton;
@property(nonatomic, strong) UIButton *renderButton;
@property(nonatomic, strong) UIButton *previewButton;
@property(nonatomic, strong) UIButton *outdoorButton;
@property(nonatomic, strong) UIButton *cancelButton;
@property(nonatomic, strong) OYImportCancellation *cancellation;
@property(nonatomic, strong) NSFileCoordinator *fileCoordinator;
@property(nonatomic) BOOL cancelRequested;
@property(nonatomic, strong) UIActivityIndicatorView *spinner;
@property(nonatomic, copy) NSString *message;
@property(nonatomic) NSInteger requestedAction;
@end

@implementation OYLauncherController

- (UILabel *)label:(NSString *)text size:(CGFloat)size weight:(UIFontWeight)weight
{
    UILabel *label = [[UILabel alloc] init];
    label.text = text;
    label.font = [UIFont systemFontOfSize:size weight:weight];
    label.numberOfLines = 0;
    return label;
}

- (UIButton *)button:(NSString *)title action:(SEL)action primary:(BOOL)primary
{
    UIButtonConfiguration *configuration = primary
        ? UIButtonConfiguration.filledButtonConfiguration : UIButtonConfiguration.tintedButtonConfiguration;
    configuration.title = title;
    configuration.cornerStyle = UIButtonConfigurationCornerStyleLarge;
    configuration.contentInsets = NSDirectionalEdgeInsetsMake(16, 20, 16, 20);
    UIButton *button = [UIButton buttonWithConfiguration:configuration primaryAction:nil];
    [button addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
    return button;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
    self.view.backgroundColor = UIColor.systemBackgroundColor;
    self.view.tintColor = [UIColor colorWithRed:0.45 green:0.72 blue:1.0 alpha:1.0];
    UIScrollView *scroll = [[UIScrollView alloc] init];
    scroll.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:scroll];
    UIStackView *stack = [[UIStackView alloc] init];
    stack.axis = UILayoutConstraintAxisVertical;
    stack.spacing = 20;
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    [scroll addSubview:stack];
    NSLayoutConstraint *preferredWidth =
        [stack.widthAnchor constraintEqualToAnchor:scroll.frameLayoutGuide.widthAnchor multiplier:0.76];
    preferredWidth.priority = UILayoutPriorityDefaultHigh;
    [NSLayoutConstraint activateConstraints:@[
        [scroll.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
        [scroll.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor],
        [scroll.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
        [scroll.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor],
        [stack.centerXAnchor constraintEqualToAnchor:scroll.frameLayoutGuide.centerXAnchor],
        preferredWidth,
        [stack.widthAnchor constraintLessThanOrEqualToConstant:680],
        [stack.topAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.topAnchor constant:40],
        [stack.bottomAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.bottomAnchor constant:-40],
        [scroll.contentLayoutGuide.widthAnchor constraintEqualToAnchor:scroll.frameLayoutGuide.widthAnchor]
    ]];
    [stack addArrangedSubview:[self label:@"OpenYAMM" size:38 weight:UIFontWeightBold]];
    UILabel *subtitle = [self label:@"魔法门 VI · VII · VIII\n开发预览 · iPadOS" size:18 weight:UIFontWeightMedium];
    subtitle.textColor = UIColor.secondaryLabelColor;
    [stack addArrangedSubview:subtitle];
    [stack addArrangedSubview:[self label:@"游戏资源" size:24 weight:UIFontWeightSemibold]];
    self.statusLabel = [self label:@"" size:17 weight:UIFontWeightRegular];
    self.statusLabel.accessibilityIdentifier = @"resource-status";
    [stack addArrangedSubview:self.statusLabel];
    self.spinner = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];
    self.spinner.hidesWhenStopped = YES;
    [stack addArrangedSubview:self.spinner];
    self.cancelButton = [self button:@"取消" action:@selector(cancelResourceOperation) primary:NO];
    self.cancelButton.accessibilityIdentifier = @"cancel-resource-operation";
    self.cancelButton.hidden = YES;
    [stack addArrangedSubview:self.cancelButton];
    self.importButton = [self button:@"选择游戏资源目录" action:@selector(selectFolder) primary:NO];
    self.importButton.accessibilityIdentifier = @"import-resources";
    self.playButton = [self button:@"开始游戏" action:@selector(startGame) primary:YES];
    self.playButton.accessibilityIdentifier = @"start-game";
    self.renderButton = [self button:@"运行渲染检查" action:@selector(checkRendering) primary:NO];
    self.renderButton.accessibilityIdentifier = @"check-rendering";
    self.previewButton = [self button:@"预览原始主菜单（8 秒）" action:@selector(previewMenu) primary:NO];
    self.previewButton.accessibilityIdentifier = @"preview-menu";
    self.outdoorButton = [self button:@"浏览新索匹格镇（60 秒）" action:@selector(previewOutdoor) primary:NO];
    self.outdoorButton.accessibilityIdentifier = @"preview-outdoor";
    [stack addArrangedSubview:self.importButton];
    [stack addArrangedSubview:self.previewButton];
    [stack addArrangedSubview:self.outdoorButton];
    [stack addArrangedSubview:self.playButton];
    [stack addArrangedSubview:self.renderButton];
    UILabel *help = [self label:
        @"请选择自己拥有的 MMMerge 完整目录，或已经转换的 OpenYAMM 资源目录。导入的文件会复制到此 iPad。"
        @"\n\n原始目录导入后可转换游戏资源。可选大陆和素材缺项见导入结果；部分任务行为仍在完善。"
        @"转换资源可点击“开始游戏”，使用键鼠或触控，并通过顶部 Menu 保存、读取中文名称存档。"
        @"\n\n游戏中左侧拖动移动，右侧拖动转向；技能列表可上下滑动。"
        @"此测试版仍有未完成的玩法与缺失图像。"
        size:15 weight:UIFontWeightRegular];
    help.textColor = UIColor.secondaryLabelColor;
    [stack addArrangedSubview:help];
    self.importButton.enabled = NO;
    self.playButton.enabled = NO;
    self.renderButton.enabled = NO;
    self.previewButton.enabled = NO;
    self.outdoorButton.enabled = NO;
    self.statusLabel.text = @"正在检查上次导入…";
    [self.spinner startAnimating];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^
    {
        @autoreleasepool
        {
            NSError *failure = nil;
            NSDictionary *recovery = [OYResourceImport recoverInterruptedImportsInSupport:self.support error:&failure];
            NSDictionary *record = [OYResourceImport activeImportInSupport:self.support];
            dispatch_async(dispatch_get_main_queue(), ^
            {
                [self.spinner stopAnimating];
                self.record = record;
                self.importButton.enabled = YES;
                self.renderButton.enabled = YES;
                if (recovery == nil)
                    self.message = failure.localizedDescription ?: @"上次导入的检查未完成，请稍后重试。";
                else if ([recovery[@"removed_incomplete_directories"] unsignedIntegerValue] > 0)
                    self.message = @"已清理上次未完成的导入，可以重新导入或转换资源。";
                [self refreshStatus];
            });
        }
    });
}

- (void)refreshStatus
{
    BOOL runtime = [self.record[@"kind"] isEqual:@"runtime"];
    BOOL source = [self.record[@"kind"] isEqual:@"source"];
    NSDictionary *resourceStatus = self.record == nil ? nil
        : [OYResourceImport resourceStatusForImport:self.record inSupport:self.support];
    BOOL resourceGaps = [resourceStatus[@"has_known_gaps"] boolValue];
    self.playButton.enabled = runtime || source;
    NSString *playTitle = source ? (resourceGaps ? @"转换游戏资源" : @"转换并开始游戏")
        : (resourceGaps ? @"继续测试现有内容" : @"开始游戏");
    [self.playButton setTitle:playTitle forState:UIControlStateNormal];
    self.previewButton.enabled = [self.record[@"kind"] isEqual:@"source"];
    self.outdoorButton.enabled = self.previewButton.enabled;
    NSString *status = @"尚未导入游戏资源。";
    if (self.record != nil)
    {
        status = [NSString stringWithFormat:@"%@\n已导入 %@ 个文件。\n%@", self.record[@"name"],
            self.record[@"files"], runtime ? @"已识别转换资源，启动时将检查内容。"
                : @"原始资源已保存，可以转换后开始游戏。首次转换需要一些时间，请保持应用在前台。"];
        if (self.record[@"converted_maps"] != nil)
        {
            NSArray *maps = self.record[@"converted_maps"];
            NSString *continents = [maps containsObject:@"mm7:7out01.odm"]
                ? @"恩洛斯或安塔加里奇" : @"恩洛斯";
            status = [status stringByAppendingFormat:@"\n已转换 %lu 个场景；新游戏请选择%@。",
                (unsigned long)[maps count], continents];
        }
        status = [status stringByAppendingFormat:@"\n%@", resourceStatus[@"message"]];
    }
    self.statusLabel.text = self.message.length > 0
        ? [NSString stringWithFormat:@"%@\n\n%@", self.message, status] : status;
}

- (void)selectFolder
{
    UIDocumentPickerViewController *picker = [[UIDocumentPickerViewController alloc]
        initForOpeningContentTypes:@[UTTypeFolder] asCopy:NO];
    picker.delegate = self;
    picker.allowsMultipleSelection = NO;
    [self presentViewController:picker animated:YES completion:nil];
}

- (void)beginResourceOperation:(NSString *)status
{
    self.cancellation = [[OYImportCancellation alloc] init];
    self.cancelRequested = NO;
    self.cancelButton.hidden = NO;
    self.cancelButton.enabled = YES;
    for (UIButton *button in @[self.importButton, self.playButton, self.renderButton,
        self.previewButton, self.outdoorButton]) button.enabled = NO;
    self.statusLabel.text = status;
    [self.spinner startAnimating];
}

- (void)finishResourceOperation
{
    [self.spinner stopAnimating];
    self.cancelButton.hidden = YES;
    self.cancellation = nil;
    self.fileCoordinator = nil;
    self.importButton.enabled = YES;
    self.renderButton.enabled = YES;
}

- (void)cancelResourceOperation
{
    if (self.cancellation == nil) return;
    self.cancelRequested = YES;
    self.cancelButton.enabled = NO;
    if ([self.cancellation cancel])
    {
        self.statusLabel.text = @"正在取消，请等待当前资源处理与清理结束…";
        [self.fileCoordinator cancel];
    }
    else
    {
        self.statusLabel.text = @"资源处理已完成，正在保存结果…";
    }
}

- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls
{
    NSURL *source = urls.firstObject;
    if (source == nil)
    {
        return;
    }
    const BOOL scopedAccess = [source startAccessingSecurityScopedResource];
    [self beginResourceOperation:@"正在复制并检查资源，请保持应用在前台…"];
    OYImportCancellation *cancellation = self.cancellation;
    NSFileCoordinator *coordinator = [[NSFileCoordinator alloc] initWithFilePresenter:nil];
    self.fileCoordinator = coordinator;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^
    {
        @autoreleasepool
        {
            __block NSError *failure = nil;
            __block NSDictionary *record = nil;
            __block CFAbsoluteTime lastProgress = 0;
            if (!cancellation.isCancelled)
            {
                [coordinator coordinateReadingItemAtURL:source options:0 error:&failure
                    byAccessor:^(NSURL *coordinatedURL)
                {
                    record = [OYResourceImport importDirectory:coordinatedURL intoSupport:self.support
                        progress:^(unsigned long long bytes, NSUInteger files)
                        {
                            const CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
                            if (now - lastProgress < 0.1) return;
                            lastProgress = now;
                            dispatch_async(dispatch_get_main_queue(), ^
                            {
                                if (self.cancellation != cancellation || self.cancelRequested) return;
                                self.statusLabel.text = [NSString stringWithFormat:
                                    @"正在复制资源：已完成 %lu 个文件，已复制 %.1f MB…",
                                    static_cast<unsigned long>(files), bytes / (1024.0 * 1024.0)];
                            });
                        } cancellation:cancellation error:&failure];
                }];
            }
            if (scopedAccess)
            {
                [source stopAccessingSecurityScopedResource];
            }
            dispatch_async(dispatch_get_main_queue(), ^
            {
                [self finishResourceOperation];
                if (record != nil)
                {
                    self.record = record;
                    self.message = @"资源导入完成。";
                }
                else
                {
                    self.message = cancellation.isCancelled
                        ? @"已取消导入，原有资源保持不变。" : (failure.localizedDescription ?: @"导入失败，请重试。");
                }
                [self refreshStatus];
            });
        }
    });
}

- (void)startGame
{
    if ([self.record[@"kind"] isEqual:@"runtime"])
    {
        self.requestedAction = 1;
        return;
    }
    if (![self.record[@"kind"] isEqual:@"source"])
    {
        return;
    }
    NSURL *layouts = [NSBundle.mainBundle URLForResource:@"gameplay_layouts" withExtension:nil subdirectory:@"runtime"];
    if (layouts == nil)
    {
        self.message = @"应用缺少界面布局，无法转换资源。";
        [self refreshStatus];
        return;
    }
    NSDictionary *source = self.record;
    [self beginResourceOperation:@"正在准备转换资源，请保持应用在前台…"];
    OYImportCancellation *cancellation = self.cancellation;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^
    {
        @autoreleasepool
        {
            NSError *failure = nil;
            NSDictionary *converted = [OYResourceImport convertImport:source inSupport:self.support
                gameplayLayouts:layouts progress:^(NSString *stage, NSUInteger current, NSUInteger total)
                {
                    dispatch_async(dispatch_get_main_queue(), ^
                    {
                        if (self.cancellation != cancellation || self.cancelRequested) return;
                        self.statusLabel.text = [NSString stringWithFormat:@"正在转换游戏资源（%lu/%lu）…",
                            static_cast<unsigned long>(current), static_cast<unsigned long>(total)];
                    });
                } cancellation:cancellation error:&failure];
            dispatch_async(dispatch_get_main_queue(), ^
            {
                [self finishResourceOperation];
                if (converted != nil)
                {
                    self.record = converted;
                    NSDictionary *resourceStatus = [OYResourceImport resourceStatusForImport:converted
                        inSupport:self.support];
                    const BOOL resourceGaps = [resourceStatus[@"has_known_gaps"] boolValue];
                    self.message = resourceGaps ? @"转换已完成，但仍有资源缺项。可补齐后重新导入，或继续测试现有内容。"
                        : self.cancelRequested ? @"资源转换已完成，可以开始游戏。" : @"资源转换完成，正在开始游戏。";
                    [self refreshStatus];
                    if (!self.cancelRequested && !resourceGaps) self.requestedAction = 1;
                }
                else
                {
                    self.message = failure.localizedDescription ?: @"资源转换未完成，原始资源仍然保留。";
                    [self refreshStatus];
                }
            });
        }
    });
}

- (void)checkRendering
{
    self.requestedAction = 2;
}

- (void)previewMenu
{
    self.requestedAction = 3;
}

- (void)previewOutdoor
{
    self.requestedAction = 4;
}
@end

int showIosLauncher(NSURL *support, NSString *message, std::filesystem::path &assets)
{
    UIWindowScene *windowScene = nil;
    for (UIScene *scene in UIApplication.sharedApplication.connectedScenes)
    {
        if ([scene isKindOfClass:UIWindowScene.class])
        {
            windowScene = (UIWindowScene *)scene;
            break;
        }
    }
    if (windowScene == nil)
    {
        return 0;
    }
    UIWindow *window = [[UIWindow alloc] initWithWindowScene:windowScene];
    OYLauncherController *controller = [[OYLauncherController alloc] init];
    controller.support = support;
    controller.message = message;
    window.rootViewController = controller;
    [window makeKeyAndVisible];
    bool running = true;
    while (running && controller.requestedAction == 0)
    {
        @autoreleasepool
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_TERMINATING)
                {
                    running = false;
                }
            }
            SDL_Delay(16);
        }
    }
    const int action = running ? static_cast<int>(controller.requestedAction) : 0;
    if (action == 1 || action == 3 || action == 4)
    {
        assets = [OYResourceImport contentURLForImport:controller.record support:support].path.UTF8String;
    }
    window.hidden = YES;
    window.rootViewController = nil;
    return action;
}
