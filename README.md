# ap5 — コード中心の最小UEサンプル

Windows上でスクリプトを実行すると、床・球1個・固定カメラ・照明をC++で生成するサンプルです。
球が高さ250 cmから落下し、床の上に止まります。移動操作はまだありません。終了はウィンドウを閉じるかAlt+F4です。
Blueprint作成、手動のモデル配置、手動のマップ保存は不要です。

## 開発用チャット

- [ap5の開発相談・作業履歴（ChatGPT）](https://chatgpt.com/c/6ab5f3bb-5964-83ee-b116-f59050211fe5)

## 対象環境

- Windows 11 x64、Windows PowerShell 5.1またはPowerShell 7（WSLでは実行しません）
- Unreal Engine **5.6.x**：Epic Games Launcherのインストール済みバイナリ版
- Visual Studio 2022のC++ビルドツール（17.8以上、17.14推奨）
- MSVC v143 **14.38（VS 17.8）**、Windows SDK **10.0.22621.0**
- DirectX 11 / Shader Model 5対応GPUとドライバ
- 最初の依存ソフト取得にはインターネット接続とインストール権限

最新UEを自動採用せず、再現性のため5.6系を明示しています。他バージョンはスクリプトが拒否します。
UEとビルドツールは大容量です。必要容量は各インストーラーの表示を確認してください。
一般のPython、Docker、Visual StudioのIDE画面操作は必須ではありません。

## 1. リポジトリ取得（WindowsのPowerShell）

Gitがなければ `winget install --id Git.Git --exact --source winget` で導入し、PowerShellを開き直します。
GitHub認証を求められたら、リポジトリへアクセスできるアカウントで認証してください。

```powershell
git clone https://github.com/SuzukiReiya/ap5.git
cd ap5
```

初回は短いローカルパス（例：`C:\dev\ap5`）を推奨します。OneDriveやネットワークドライブは避けてください。

## 2. 環境構築（WindowsのPowerShell）

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Scripts\Setup.ps1
```

このコマンドは、そのプロセス内のみ実行ポリシーを指定します。PC全体の実行ポリシーは変更しません。
会社のポリシー等で禁止されている場合は管理者に確認してください。

`Setup.ps1` は次を行います。

1. VS 2022＋MSVC 14.38が見つからなければ、wingetでBuild ToolsとSDKのインストールを開始。
2. UE 5.6を探し、未導入ならEpic Games Launcherのインストールを試み、手動工程を案内して終了コード1で停止。
3. UE・MSVC・SDKが揃ったら、UEのパスを `.local/engine.json` に保存。
4. `SETUP_READY` と表示して終了コード0で完了。

UAC、ソフトウェアのライセンス、インストーラー画面での確認が必要になる場合があります。
ライセンス同意はスクリプトで代行しません。再起動を要求された場合は再起動後に再実行してください。

### `SETUP FAILED: Manual step...` と表示された場合

次の順で表示された場合は、**最初からやり直したり、導入済みソフトを削除したりする必要はありません**。

| 表示 | 意味 |
|---|---|
| Visual Studio Build Toolsの「インストールが完了しました」 | Build Toolsのインストーラーは正常終了 |
| `WARNING: UE 5.6.x was not found...` | UE本体が未導入、またはインストール先を検出できていない |
| Epic Games Launcherの「インストールが完了しました」 | Launcherの導入は完了。UE本体の導入とは別 |
| `SETUP FAILED: Manual step: open Epic Games Launcher...` | UE本体の手動導入待ち。準備未完了を示すため、スクリプトが終了コード1で停止 |

赤字の `FAILED` はこの場合、直前のBuild ToolsやLauncherのインストール失敗を意味しません。
MSVC・SDKを含む最終チェックはUEが見つかった後に行うため、まだセットアップ完了ではありません。
警告中の `D:\Epic Games\UE_5.6` は指定例であり、その場所にインストールする必要はありません。

再開手順：

1. **Windowsのスタートメニュー**からEpic Games Launcherを開く。
2. **Launcher画面**で、次節の手順に従ってUE **5.6系** をインストールし、完了を待つ。
3. **WindowsのPowerShell**で、リポジトリのフォルダ（例：`D:\work\ap5`）へ戻り、同じコマンドを再実行する。

   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Scripts\Setup.ps1
   ```

4. `SETUP_READY` が表示されたら、ビルドして起動する。

   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Scripts\BuildAndRun.ps1
   ```

UE 5.6を既に導入済みなら再インストールせず、次節の `-EngineRoot` で実際のインストール先を指定してください。
再実行では検出できたMSVC 14.38のインストールをスキップします。別のエラーが表示された場合は、その内容に対応してください。

### 初回のみ手動：UE本体の導入（Epic Games Launcher画面）

今回の導入方法では、**Epic Games Launcherの起動とEpic Gamesアカウントでのサインインが必要**です。
アカウントを持っていない場合は、Launcherの「アカウントを作成」から登録してください。既存アカウントがあれば新規登録は不要です。
「後でサインイン」ではUE本体をダウンロードできません。

LauncherはUE本体のダウンロード・更新管理に使用します。インストールとセットアップが完了した後の通常のビルド・ゲーム起動は、`BuildAndRun.ps1` がUEの実行ファイルを直接呼び出すため、毎回Launcherから起動する操作は不要です。

1. Epic Games Launcherを開き、Epic Gamesアカウントを作成するか、既存アカウントでサインイン。
2. **Unreal Engine → ライブラリ → エンジンバージョンの「＋」**。
3. バージョン一覧から **5.6系** を選び、規約等を確認してインストール。
4. **Windows用エンジン本体とEngine Contentを残す**。Android/iOS等の追加プラットフォームは不要。
5. インストール完了後、PowerShellで `Setup.ps1` をもう一度実行。

Epicログイン・規約同意・UE本体のダウンロード指定は完全自動化していません。
Launcherの配置が標準外で再インストール案内が出る場合も、既存のLauncherを使って構いません。

UEを標準と異なる場所に導入した場合：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Scripts\Setup.ps1 -EngineRoot "D:\Epic Games\UE_5.6"
```

`EngineRoot` は `Engine` フォルダの**親**です。次回から保存されたパスを利用します。
検索優先度は `-EngineRoot` → 環境変数 `AP5_UE_ROOT` → `.local/engine.json` → レジストリ → 標準パスです。

### VSが既にあり自動追加に失敗した場合（Visual Studio Installer画面）

既存のBuild Toolsではwingetが「導入済み」と判断し、追加コンポーネントを入れないことがあります。
**Visual Studio Installer → VS 2022 / Build Tools 2022 → 変更**から次を追加してください。

- C++によるデスクトップ開発（Build ToolsではC++ビルドツール）
- 個別のコンポーネント：MSVC v143 - VS 2022 C++ x64/x86 ビルドツール **v14.38-17.8**
- 個別のコンポーネント：Windows 11 SDK **10.0.22621.0**

既にMSVC 14.38があってSDKだけ不足している場合も、SDKを上記の画面から追加してください。
その後 `Setup.ps1` を再実行します。既存のVSやSDKを削除する処理はありません。

導入処理をせず確認だけ行う場合：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Scripts\Setup.ps1 -CheckOnly
```

## 3. ビルドして実行（WindowsのPowerShell）

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Scripts\BuildAndRun.ps1
```

処理順序：

1. UnrealBuildToolで `Ap5Editor / Win64 / Development` をコンパイル。
2. 初回はUE同梱Pythonをコマンドレットから実行し、空マップ `Content/Generated/Minimal.umap` を保存。
3. UEの `-game` モードで1280×720のゲームウィンドウを起動。
4. C++のGameModeが床・球・カメラ・ライトを生成。

初回はシェーダーコンパイルなどで時間がかかります。完了するまで待ってください。
再ビルド前に前のゲームやUEエディタを終了してください（DLLが使用中だとリンクに失敗します）。

**この起動は開発用のStandalone Gameです。配布用exeではなく、UE本体を使用します。**
低負荷から始めるためDX11、Lumenなし、Naniteなし、レイトレーシングなし、60fps上限です。
低スペックPC上の性能を保証するものではなく、対象機での計測が必要です。

コンパイルとマップ生成だけの場合：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Scripts\BuildAndRun.ps1 -BuildOnly
```


### サンプルの見方と終了方法

- ボールは中心の高さ250cmから一度だけ落下して床で止まります。半径は約50cmで、落下時間は約0.6秒のため、初回描画の準備中に着地している場合があります。
- マウスカーソルを表示し、ウィンドウ内に固定しません。右上の×またはAlt＋F4で終了できます。Alt＋F4は通常の終了操作です。
- 2026-10-04のユーザー提供ログで、UE 5.6.1のWindowsビルド・マップ生成・サンプル生成・正常終了を確認しました。画像で床と球の描画を確認しましたが、落下中の動きは未確認です。
- カーソル表示と主光源の優先順位を修正した版は、Windowsでの再ビルド・実画面確認が必要です。

## 4. 配布用のWindowsゲームを作る（任意）

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Scripts\Package.ps1
```

Shipping構成でビルド・Cook・Stage・Pak・Archiveを実行します。
出力は `Builds/Windows/Ap5.exe` を含むフォルダです。**exeだけでなくWindowsフォルダ全体を配布**します。
プレイヤー側にはUEエディタは不要ですが、UEの実行時前提コンポーネントが必要です。
`-prereqs` で同梱される前提コンポーネントのインストーラーも配布物に含めてください。

## 確認の目安とログ

- 灰色の四角い床と球1個が見え、球が落ちて床に止まる。
- 固定の斜めカメラ。画面外の背景は黒。HUD・操作・空・外部アセットはまだありません。
- `.local/logs/run.log` または `Saved/Logs/Ap5.log` に `AP5_SAMPLE_READY` が出る。

| 問題 | 確認するもの |
|---|---|
| UEが見つからない / 別バージョン | `Setup.ps1 -EngineRoot` で5.6のパスを指定 |
| MSVC / SDK不足 | Visual Studio Installerで上記コンポーネントを追加 |
| ビルド失敗 | `.local/logs/build.log` の最初のエラー |
| マップ生成失敗 | `.local/logs/create-map.log`。Python/Editor Scriptingプラグインの有効化を確認 |
| 黒画面 / 起動失敗 | `.local/logs/run.log`、`Saved/Logs/Ap5.log`、GPUドライバ |
| パッケージ失敗 | `.local/logs/package.log` |
| DLLが使用中 | 起動中のAp5ゲームとUEエディタを終了して再実行 |

ログは次回実行で上書きされるので、問題が出たら保存して共有してください。`AP5_SAMPLE_READY` は生成完了の目印であり、描画・衝突の正しさは実画面でも確認してください。

## 構成と変更箇所

- `Source/Ap5/Ap5GameMode.cpp`：床・球のサイズ、配置、照明、カメラ、物理設定。
- `Config/DefaultEngine.ini`：描画・既定マップ・GameMode。
- `Scripts/CreateMap.py`：空のマップを生成。既存マップは上書きしません。
- `Scripts/Setup.ps1`：Windows依存環境の導入・確認。
- `Scripts/BuildAndRun.ps1`：ビルド・初回マップ生成・ゲーム起動。
- `Scripts/Package.ps1`：配布用Shippingビルド。
- `.local/`、`Content/Generated/`、`Binaries/`、`Intermediate/`、`Saved/`、`Builds/`：ローカル生成物。Git対象外。

生成マップはC++サンプル用の空マップです。手編集するようになったら `Content/Maps/` などGit管理する場所へ保存し、マップ設定を変更してください。
現在 `.uasset/.umap` はGit LFS設定を用意していますが、コミットされたバイナリアセットはありません。将来独自アセットを追加するときにGit LFSを導入してください。

## 検証状況

この初期実装の作成環境はLinuxで、Windows版UE・MSVC・PowerShellがありません。
初期実装時にはWindows/UEの実行検証はできませんでした。その後、上記のユーザー提供ログと画像でUE 5.6.1のビルド・マップ生成・床と球の描画・正常終了を確認しています。物理動作の推移、今回のカーソル・照明修正、Shippingパッケージは未検証です。
リポジトリ構成の整合性とマップ生成スクリプトの分岐は、標準Pythonだけのテストで検証します。
PowerShellの構文検証用スクリプトも付属しますが、構文成功はUE動作保証ではありません。
GitHub Actionsにも同じテストとPowerShell 5.1/7の構文検証を設定しています。UE本体を使うCIビルドではありません。

```powershell
python -m unittest discover -s Tests -v
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tests\Test-PowerShell.ps1
```

これらのテスト用Pythonは任意です。通常のビルド起動にはUE同梱Pythonを使用します。

## 参考

- [UE 5.6 / Visual Studio対応表](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine?application_version=5.6)
- [UE Pythonコマンドレット](https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python?application_version=5.6)
- [Cook・Package・BuildCookRun](https://dev.epicgames.com/documentation/en-us/unreal-engine/build-operations-cooking-packaging-deploying-and-running-projects-in-unreal-engine?application_version=5.6)
- [Microsoft Build ToolsのコンポーネントID](https://learn.microsoft.com/en-us/visualstudio/install/workload-component-id-vs-build-tools?view=vs-2022)
