# Homework : Render a 3D Mesh

## 編譯／執行環境
>OS：Windows 11
>IDE：Visual Studio using C++ with OpenGL(version:3.3)
建置方法可參考->https://hackmd.io/@ShiroCiel/B1ygefMz6

- 使用Visual Studio 2022開啟此專案後，打開main.cpp並且按下"本機 Windows 偵錯工具" 或按下 F5即可進行編譯後，會出現視窗顯示Dino Render後的結果(如下圖)。  
- 編譯成功後可以滑鼠來移動Dino的視角，同時也可使用Q or E鍵來縮放大物件大小。  

![image](https://hackmd.io/_uploads/Bygs1V2jge.png)

## 程式概要說明

整體架構分成幾個主要部分，包括**模型載入與處理**、**渲染流程**、**相機控制**以及**視覺效果的呈現**。

#### 模型載入和處理
利用 **tinyobjloader** 來解析 OBJ 檔案，讀取頂點座標與法向量的資訊。若檔案中沒有法向量的話，會由三角形的面積法向量去計算平均頂點法向量，確保後續光照能正常作用。   

讀入的模型會先進行正規化，將最大邊長縮放至單位大小，並將中心點對齊原點，讓不同模型都能以統一的比例與位置呈現。  

#### 渲染部分
程式實作了兩組著色器。第一組是用來處理模型本身的 Vertex 與 Fragment Shader，主要負責將頂點經過 Model-View-Projection 矩陣轉換，在像素層級套用 **Blinn–Phong 光照模型**，包含環境光、漫反射與鏡面反射，讓模型呈現出有立體感的效果。第二組著色器專門用來繪製背景，透過一個覆蓋整個螢幕的大三角形，在片段著色器中根據 y 座標混合出由橘色地平線漸層到藍色天空的效果，使場景更自然。  

整個模型資料會被整理成交錯格式的陣列 `[x,y,z, nx,ny,nz]`，並上傳到 GPU 的 VBO 中，再透過 VAO 綁定給著色器使用。這樣的資料組織方式有助於提升效能，也讓繪製流程更簡潔。  

#### 相機控制
採取了**環繞軌道式**設計，使用者可以透過滑鼠左鍵拖曳來旋轉視角，並以 Q/E 鍵調整遠近距離。程式會根據目前的 `yaw`、`pitch` 與 `distance` 參數計算相機位置，並搭配 GLM 的 `lookAt` 與 `perspective` 函式生成 View 與 Projection 矩陣  

在主要視窗的while loop中，程式會依序清除畫面，先繪製背景，再用深度測試繪製模型。這樣能避免背景與模型互相干擾，並確保渲染順序正確。  

最後，透過 GLFW 的Polling機制，讓能即時回應滑鼠與鍵盤輸入，增加一點互動性。  
 
- 參考資料:https://learnopengl-cn.github.io/ 、https://medium.com/maochinn/%E9%9B%BB%E8%85%A6%E5%9C%96%E5%AD%B800-opengl-fa7105f59ecd
- 網頁版:https://hackmd.io/@DDK/ByETpmhoxl

