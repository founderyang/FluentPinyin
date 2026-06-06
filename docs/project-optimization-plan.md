# 流畅拼音项目优化方案

本文档整合用户反馈、两份外部优化意见、当前代码审计和 00.00.04 已完成优化，作为后续拆分执行、回滚和发布的依据。

## 目标

- 缩短安装完成后到可以稳定输入的等待时间。
- 降低候选框从紧凑到展开的首帧延迟。
- 确保设置页所有功能真实生效，尤其候选框字体大小、字体族、布局、DPI。
- 字体继续私有加载，卸载后不留下系统字体注册残留。
- 万象 Base 开启主 `translator` 调频，`wanxiang_pro` 保持关闭；不修改 RIME 和万象上游文件。
- 版本保持 `00.00.04`，发布物、README、GitHub About 和 Release 文案使用中文。
- 只维护 `main` 和 `codex/optimization-test` 两个分支；每批完成后推送测试分支，验证通过后合并 `main` 并刷新 Release。

## 硬约束

- 不修改 `schemas/wanxiang/current` 中的万象上游数据，不修改 RIME 上游源码或预编译二进制。
- 不把 MiSans、Source Han Sans、Plangothic 等字体注册到系统字体表。
- MSI 安装验证必须限时，不能再使用可能长期卡住的无限等待方式。
- 大重构前必须先补测试、日志、基准或脚本，确保可回滚。
- 每个阶段都要能独立构建、测试、打包、安装验证。

## 当前状态

- `00.00.04` 已完成多轮性能、安全和安装修复。
- 已加入 Rime warmup/cache、候选窗布局缓存、候选框字体设置即时刷新、多显示器 DPI 适配、Windows App Runtime 安装器、私有字体检查、万象 Base/Pro 调频 patch。
- 已加入日志文件句柄复用、INFO flush 节流、8MB 日志轮转。
- README、安装包 README、GitHub About、Release notes 已中文化。
- 已新增本机安装验证脚本，覆盖 ProductCode、中文 README、私有字体、Windows App Runtime、TSF smoke、万象 Base/Pro 调频。
- 已新增 TSF `settings.ini` 时间戳缓存，避免设置刷新时重复打开和解析设置文件。

## P0：安全网、可观测性和设置读写

### 1. 固化基准与日志

执行项：

- 保留 Rime 初始化、deploy、cache hit/miss、warmup、候选刷新、候选窗口 render 的耗时日志。
- 增加可重复的基准记录：安装后 warmup 耗时、暖缓存首键初始化耗时、候选展开 render 耗时。
- 高频日志继续使用 flush 节流和轮转，避免日志本身影响输入性能。

验收：

- 可以从日志判断慢点来自文件复制、Rime deploy、librime 初始化、候选查询、布局还是渲染。
- 常规输入时日志不会无限增长。

### 2. 统一设置读写模块

问题：

- 设置读写分散在 TSF、Core、WinUI 设置、Sync 中。
- 各处编码、缓存、写入和迁移策略不同。

阶段执行：

- 短期：TSF 和 Core 增加本地时间戳缓存，减少初始化和设置刷新重复 I/O。
- 中期：抽出 `common/settings_store.*`，统一 UTF-8/UTF-16/ANSI 兼容读取、时间戳缓存、索引、原子写入、迁移和并发锁。
- 长期：WinUI 设置页和 Sync 配置逐步迁移到同一模块。

验收：

- 设置 App 修改后 TSF 能刷新到最新值。
- 候选框字体大小、字体族、布局、主题、热键、词库开关均能按设置生效。
- Rime 初始化不再为每个设置项重复读取文件。

### 3. 安装验证防卡

执行项：

- 使用 `tests/install_msi.ps1` 进行限时 MSI 安装验证。
- 脚本通过进程状态和 MSI 日志判断成功，超时输出日志尾部和 PID。

验收：

- 安装验证不会无限等待。
- 安装后 `tests/install_verify.ps1` 必须通过。

## P1：性能和用户体验

### 1. Rime 冷启动继续优化

执行项：

- 保持安装后非阻塞 warmup。
- Core 设置读取加缓存，减少 `CurrentInputSchemaSelection` 的重复 I/O。
- 继续审计 `EnsureWanxiangRuntimeFiles`、cache signature、deploy lock，避免暖缓存首键被后台 warmup 锁阻塞。

验收：

- 暖缓存首键不触发 deploy。
- 日志中 cache hit 初始化应稳定在低耗时范围。

### 2. 候选窗布局和渲染继续缓存

执行项：

- 保留候选 layout cache、text measure cache、fallback glyph cache、icon cache。
- 下一步拆出候选布局纯函数测试，先让布局计算可单测，再考虑拆文件。
- 审核鼠标 hover、tooltip、展开、翻页是否重复计算相同 layout。

验收：

- 紧凑到展开首帧 P95 目标小于 50ms。
- 鼠标 hover 和 tooltip 不造成明显重绘抖动。

### 3. DPI 和设置生效复查

执行项：

- 保留候选窗、工具栏、状态提示、菜单按所在显示器 DPI 定位。
- 设置页所有功能逐项核对：默认输入状态、候选框、外观、高级、词库、热键、同步、关于。
- 生成设置功能映射文档，标记“立即生效”“需重启 Rime”“需重新打开窗口”。

验收：

- 用户修改候选字体大小后 TSF 候选窗立即刷新。
- 跨显示器移动后候选窗不越界、不缩放异常。

## P1：构建和 CI

执行项：

- CI 保持 Release build、CTest、package smoke。
- 增加 Debug build。
- 增加 updater 离线 JSON 单测。
- 增加 payload 大小阈值检查。
- 评估 MSVC `/analyze` 或 clang-tidy，但先不阻断发布。
- 评估 PCH，优先给大目标降低本地编译时间。

验收：

- CI 能发现 Release/Debug 基础构建问题。
- payload 体积异常增长会失败或至少报警。

## P2：安全和鲁棒性

执行项：

- Updater 下载继续验证 Authenticode 签名。
- 给 WinINet HTTP 请求设置连接/发送/接收超时。
- 保持 updater JSON 解析的边界测试；如引入 JSON 库，必须评估包体和依赖。
- 审计 `LinkOrCopyFileIfNewer` 的删除到硬链接窗口期，优先降低 TOCTOU 风险。
- Sync 服务保持 AES-GCM 完整性校验，继续补网络错误模拟。

验收：

- updater 网络异常不会长时间挂起。
- 下载资产解析和签名验证失败会给出明确错误。
- 同步包篡改测试继续失败即通过。

## P2：维护性重构

### 1. 拆分 `tsf_text_service.cpp`

顺序：

1. 候选布局纯函数和测试。
2. 候选窗口渲染和交互。
3. 工具栏窗口。
4. 状态提示。
5. 右键菜单。
6. 按键处理。
7. Rime 协调层。

策略：

- 每次只拆一个职责。
- 不改变行为。
- 每次拆分后跑构建、CTest、package smoke、安装验证。

### 2. 拆分 `config_winui/main.cpp`

顺序：

1. UI helper。
2. General。
3. Appearance。
4. Lexicon。
5. Hotkeys。
6. Sync。
7. About。

### 3. 同步和更新器拆层

目标模块：

- `sync_config`
- `sync_package`
- `sync_crypto`
- `sync_remote_webdav`
- `sync_remote_s3`
- `sync_schedule`
- `release_client`
- `json_reader`
- `installer_verifier`

## P3：体积治理

执行项：

- payload-size-report 保持生成。
- 增加 CI 阈值。
- 评估基础版/完整版安装包，但当前 `00.00.04` 先保持离线可用体验。
- 不移除万象语法模型和字体，除非另行确认用户体验取舍。

## 当前执行队列

1. 完成本轮 Core 设置缓存和限时安装脚本。
2. 更新 CI/package smoke，纳入安装验证脚本的语法或轻量检查。
3. 增加 Debug CI 和 payload 大小阈值。
4. 增加 updater HTTP 超时。
5. 增加 updater JSON 离线边界测试。
6. 抽出 `common/settings_store.*`。
7. 候选布局纯函数测试和模块拆分。
8. 逐步拆分 TSF、设置 App、Sync、Updater。
