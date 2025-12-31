# Computer Craphics Final Project

### 更詳細附圖片網頁版 ： https://hackmd.io/@DDK/SJ79WDem-x

### Demo之前(Before 12/18)的功能：

- 基本的XML的話可以去點擊物體，來達到像是專注在單一物體上的視覺效果，讓點擊物體之外的物件都變得較黑，而被點擊的物品能夠加上燈光的效果。
- 並且可以用滑鼠來移動，並用滑鼠滾輪來控制目前的相機視覺大小範圍。



Terminal 輸入參數並去做更改，以下是不同的範例。

##### 基本載入新的obj 並且設定他
- 介紹幾個基本的參數設定，首先"--base"就是要載入的xml場境。
- "--obj"則是要新Loading進來的Obj檔案，以下的參數也是針對於新Loading進來的obj做的一些operation
- tex則是對用obj所要用的貼圖檔案，因為mujoco的關係，只能一對一mapping。
- 剩下的"--pos, euler, scale"是針對於改變位置、座標的旋轉角度還有放大的大小，都是針對於說X, Y Z軸來去參照的。
- "--rgb"的話則是可以去改變load進來物體的顏色 分別為紅、綠、藍還有透明度。
- 最後比較有趣的部分是"--pin & --gravity"，pin是設定該物體是否會被固定住，不受重力影響，而gravity則是讓新load進來的物體會受重力影響，而對應的也是X Y Z三軸設定中會作用在哪一邊上。
```python=
mjpython src/editor.py  \
--base assets/venice/venice.xml \
--obj  assets/horse/horse.obj \
--tex  assets/horse/CH_NPC_MOB_WHorse_A02_MI_BYN_Diffuse.png \
--pos  "1 0 0" \
--euler "1 1 1" \
--scale "1 1 1" \

--rgba "1 1 1 1" \
--pin "true"  \
--gravity 0 1 0
```


##### 更動原本sence的obj
- 像是這邊的"--move-body"的參數，可以選定其坐標軸，讓他依照 X軸or Y軸 or Z軸來做移動，而0 0 0起點位置的話則是會根據物體原本所在的位置，當作原始座標移動。
```python=
mjpython src/editor.py \
--base assets/venice/venice_exploded.xml \
--move-body "venice__venice_43 1 10 0" \
```

##### 移動還有旋轉原本的物體
```python=
mjpython src/editor.py \
  --base assets/venice/venice_exploded.xml \
  --move-body "venice__venice_43 1 0 0" \
  --move-body "venice__venice_44 2 0 0" \
  --rot-body-euler "venice__venice_45 0 0 0.3" \

```

### Demo之後新增的功能
- 能夠將原本sence之中的一些obj來做替換，像是line3"--replace-body-mesh"的參數，能夠將原本的obj(e.g. venice__venice_43 (紅色機車))，來替換成木頭椅子的obj，並且還可以再加上texture的貼圖上去。
```python=
mjpython src/editor.py \
  --base assets/venice/venice_exploded.xml \
  --replace-body-mesh "venice__venice_43 assets/Wooden_Dining_Chair/Wooden_Dining_Chair.obj assets/Wooden_Dining_Chair/Dining_Chair_Colour.png 1 1 1" \
  --move-body "venice__venice_43 1 0 0" \
  --move-body "venice__venice_44 10 0 0" \
  --rot-body-euler "venice__venice_45 0 0 0.3" \

```