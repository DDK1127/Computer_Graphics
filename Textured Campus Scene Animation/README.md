# Project layout

```
CampusAnimation/
├── CMakeLists.txt
├── README.md
├── assets/
│   └── SchoolSceneDay            # put your OBJ here (with its .mtl .png beside it)
├── shaders/
│   ├── vertex_shader.vs
│   └── fragment_shader.fs
├── src/
│   ├── main.cpp
│   ├── camera.h
│   ├── camera.cpp
│   ├── shader.h
│   ├── shader.cpp
│   ├── model.h
│   ├── model.cpp
│   ├── texture_cache.h
│   └── texture_cache.cpp
└── third_party/
    ├── stb_image.h            # https://github.com/nothings/stb (single header)
    └── tiny_obj_loader.h      # https://github.com/tinyobjloader/tinyobjloader (single header)
```

---

# README.md

## 專案說明
以 OpenGL 在即時渲染中載入校園模型與貼圖，並自動播放 30–60 秒的相機動畫。

## 目錄結構
```
CampusAnimation/
├── CMakeLists.txt
├── README.md
├── assets/
│   └── SchoolSceneDay/                 # put 3D Scene 放置
│       ├── SchoolSceneDay.obj
│       ├── SchoolSceneDay.mtl
│       └── *.png
├── shaders/
│   ├── vertex_shader.vs
│   └── fragment_shader.fs
├── src/                                # Source Code
│   ├── main.cpp
│   ├── camera.h / camera.cpp
│   ├── model.h / model.cpp
│   ├── shader.h / shader.cpp
│   ├── texture_cache.h / texture_cache.cpp
└── third_party/
│   ├── stb_image.h
│   └── tiny_obj_loader.h
├── Demo.mp4
```

## 需求
- C++17 編譯器
- OpenGL 3.2+（macOS 使用系統 `<OpenGL/gl3.h>`）
- 依賴：GLFW、GLM
- 單檔頭：`third_party/stb_image.h`、`third_party/tiny_obj_loader.h`

### macOS 安裝依賴
```bash
xcode-select --install
brew install glfw glm
```

### Linux（Ubuntu）安裝依賴
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake
sudo apt-get install -y libglfw3-dev libglm-dev
```
> 註：OpenGL 驅動與對應開發套件應由系統提供。

## 建置與執行
```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
./CampusAnimation
```

## 資產放置與自動尋找
評分者會把資產放在：
```
assets/SchoolSceneDay/SchoolSceneDay.obj
assets/SchoolSceneDay/SchoolSceneDay.mtl
assets/SchoolSceneDay/*.png
```
本程式會以**相對路徑**或**自動往上尋找**方式定位資產，不需修改程式碼或路徑。若找不到資產，會以清楚訊息中止。

## 執行行為（作業規範對應）
- 啟動即自動播放：主迴圈使用時間函式驅動相機，不需任何輸入。
- 動畫時長：預設約 45 秒；可於 `src/main.cpp` 的 `duration` 參數調整到 30–60 秒。
- 可分鏡（選配）：以多段 keyframes 或切換 `viewTarget` 實作。
- 無需改路徑：使用相對或自動尋找，滿足「Program runs without manual path changes」。

## 錄影 Demo（提交所需）
- 建議解析度：≥ 1280×720
- macOS：QuickTime Player → New Screen Recording → 錄製 30–60 秒全程動畫 → 匯出 MP4
- 檔名：`demo.mp4`

## 常見問題
- **黑畫面**：請檢查著色器是否成功編譯連結、`uDiffuse` 是否綁定到 0、光照計算是否返回可見顏色。
- **全白或無貼圖**：確認 `.mtl` 的 `map_Kd` 路徑與實際檔案相對路徑一致；`third_party/stb_image.h` 是否存在。
- **找不到資產**：確認 `assets/SchoolSceneDay/` 放置正確；工作目錄為專案根或 build 目錄；本程式會向上尋找數層。
- **編譯錯誤找不到 GLFW/GLM**：請先以套件管理器安裝對應開發套件。

## 提交（符合作業規範）
請打包為 `Your_Student_ID.zip`，**不要包含 3D 場景資產**：
```bash
# 於專案根目錄
zip -r Your_Student_ID.zip \
  src shaders CMakeLists.txt README.md \
  build/CampusAnimation \
  demo.mp4 \
  -x "assets/*"
```
> 助教將在 `assets/` 放入他們的場景再執行你的程式。

## 驗證流程（自我檢核）
1. 在乾淨資料夾解壓你的 zip。
2. 依「建置與執行」步驟產生 `CampusAnimation`。
3. 放入資產後執行，應自動播放 30–60 秒動畫；移除資產時，應出現清楚錯誤訊息而非崩潰。

## 靜態連結說明（可選）
- macOS：可把 GLFW 編成靜態，仍會動態連結系統 Framework（OpenGL/Cocoa/IOKit/CoreVideo）。
- Linux：建議使用 `-static-libstdc++ -static-libgcc`，其餘動態連結；完全靜態通常不可行。
