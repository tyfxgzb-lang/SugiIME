# SugiIME 修复与更新计划（v0.2.1）

## 概述

四项修改：标点图标、假名候选、版本号、证书内嵌。全部完成后统一推送 GitHub 打包。

---

## 任务 1：标点图标改为日文标点

**问题**：悬浮工具栏第三个图标（标点切换）使用 PUA 码点 `U+F111` + fallback `。`，显示的不是日文标点样式。

**方案**：将中文标点 fallback 文字改为日文标点「。」并确保英文标点为「,」。D2D 端改 `kIconPuncCn`/`kIconPuncEn` 的 fallback 文字；WebView2 端改 10 个 HTML 的 `data-fallback`。

**修改文件**：
- `server/src/window/floating_toolbar_presenter.cpp` — `kIconPuncCn` fallback `。`→`。`（确认是日文全角句号），`kIconPuncEn` fallback `,`→`,`
- `ui-html/webview2/ftb/*.html`（10 个主题）— 标点 `data-fallback` 确认/修正
- `ui-html/webview2/settings/ime-settings/src/locales/{zh,ja,en}.ts` — 标点相关描述文案

**验收**：工具栏第三个图标显示正确的日文句号「。」。

---

## 任务 2：平片假名切换后候选栏全部为假名

**问题**：切换到平假名/片假名模式后，候选栏仍显示汉字词（只有第1候选是假名）。

**根因**：
1. `engine_input_session.cpp:14` — `character_set="hiragana"` 时 `default_japanese_kana_form` 被设为 `Auto`（应为 `Hiragana`）
2. `japanese_candidate_provider.cpp` — `kana_form` 非 `Auto` 时，`word_pool` 中的汉字词仍加入候选列表

**方案**：
1. `engine_input_session.cpp`：`hiragana` → `JapaneseKanaForm::Hiragana`，`katakana` → `JapaneseKanaForm::Katakana`
2. `japanese_candidate_provider.cpp`：当 `request.japanese_kana_form != Auto` 时，候选列表只返回 `kana_leads`（假名候选），不返回 `common`/`association`（汉字词）

**修改文件**：
- `server/src/session/engine_input_session.cpp` — 第 14 行映射修正
- `engine/providers/japanese_candidate_provider.cpp` — `candidates` 组装逻辑，非 Auto 时跳过汉字词

**验收**：切换平假名后候选栏全部为平假名；切换片假名后全部为片假名。

---

## 任务 3：版本号改为 0.2.1

**修改文件**：
- `installer/msime_setup.iss:22` — `MyAppVersion "0.1.0"` → `"0.2.1"`
- `windows/src/IME/MetasequoiaIME.rc:38-39` — `FILEVERSION 0,7,1,0` → `0,2,1,0`，`PRODUCTVERSION` 同步

**验收**：安装包文件名含 `v0.2.1`，TSF DLL 属性版本为 0.2.1.0。

---

## 任务 4：证书融入安装包

**问题**：用户需先手动导入 `SugiIME-Test.cer` 才能安装。

**方案**：在 Inno Setup 安装流程中自动将证书导入「受信任的根证书颁发机构」。

**修改文件**：
- `installer/msime_setup.iss`：
  - `[Files]` 段添加 `SugiIME-Test.cer` 到 `{app}`
  - `[Run]` 段或 `CurStepChanged(ssPostInstall)` 中执行 `certutil -addstore -f "Root" "{app}\SugiIME-Test.cer"`
- `.github/workflows/package-sugi.yml`：确认 `SugiIME-Test.cer` 被打包进 installer（当前已上传为独立 artifact，需确认是否进安装包）

**验收**：双击安装包即可完成安装，无需手动导入证书。

---

## 执行顺序

任务 1 → 任务 2 → 任务 3 → 任务 4 → 提交推送 → 触发 `package-sugi.yml` 打包验证
