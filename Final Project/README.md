MacOS environment exec in terminal.

##### 基本載入新的obj 並且設定他
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

##### 更動原本舊的obj
```python=
mjpython src/editor.py \
--base assets/venice/venice_exploded.xml \
--move-body "venice__venice_43 1 10 0" \
```

##### 移動還有旋轉原本的物體
```python=
mjpython editor.py \
  --base assets/venice/venice_exploded.xml \
  --move-body "venice__venice_43 1 0 0" \
  --move-body "venice__venice_44 2 0 0" \
  --rot-body-euler "venice__venice_45 0 0 0.3" \

```
