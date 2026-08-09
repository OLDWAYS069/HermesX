# HermesX 注音輸入核心

這個目錄保存 HermesX MSG composer 使用的注音組字與候選字核心。程式與精簡詞庫移植自
[`Kent-Liu/meshtastic-firmware-zhtw`](https://github.com/Kent-Liu/meshtastic-firmware-zhtw)，
原始基準為 `zhtw-2.7.26` 分支的 `02b872550fb4b7d3f92cc69daa963e35cd3f545d`。

HermesX 的移植版維持大千畫面鍵盤排列，並做以下調整：

- 改為 C++11 可編譯的 header-only 寫法。
- 類別與檔名加上 `HermesX` 前綴。
- 使用較小的 `zhuyin_single_static` 詞庫，保留 HermesX OTA app slot。
- 顯示與輸入 ownership 仍由 `src/graphics/Screen.cpp` 管理。
- `Aa` 與 `中/EN` 是兩個獨立功能；前者控制英文大小寫，後者切換輸入法。

字典資料來源與授權如下：

- McBopomofo：MIT，見 `licenses/hermesx-zhuyin/LICENSE-mcbopomofo.txt`
- libtabe：BSD 3-Clause，見 `licenses/hermesx-zhuyin/LICENSE-libtabe.txt`
- libchewing-data：LGPL-2.1-or-later；只作詞條篩選，見
  `licenses/hermesx-zhuyin/LICENSE-libchewing-lgpl-2.1.txt`

完整大型詞庫約需 2.5 MB app rodata，本移植不使用該版本，以免為了輸入法取消 OTA 雙分區。
