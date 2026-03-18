---
title: '自動設定工具'
---

:::info
本工具尚在開發中，如遇到問題，請聯繫團隊粉絲專頁。  
目前已整理為新的自動安裝流程；Windows 為主要使用環境，其餘平台流程仍在調整中。
:::

# 自動設定工具
你是否對繁瑣的設定流程感到厭煩？  
裝置不小心設定壞了怎麼辦？？

來使用我們的 ***自動設定工具*** 吧！  
這是一個一鍵設定的自動化工具，允許使用者將 HermesX 刷入指定韌體，並自動套用預先準備好的設定。

## 使用方式

1. 下載 [自動設定工具](https://github.com/OLDWAYS069/HermesX/tree/master/auto_flash_tool)  
   這個資料夾目前主要包含：

   `auto_installer.py`：目前的主程式

   `flash_and_config.log`：設定工具的 Log

   `Target`：請將要刷寫的韌體放在這裡

   `CLI.md`：你想要設定的參數都可以透過這裡調整，參數設定請參閱 [Meshtastic.org](https://meshtastic.org/docs/software/python/cli/)

   `readme.md`：你現在正在看的說明文件

2. 將韌體 `.bin` 放進 `Target` 資料夾  
   如果裡面有多個檔案，工具會要求你手動選擇。

3. 將裝置透過 USB 連接到電腦，並讓裝置進入可刷寫狀態。

4. 執行 `auto_installer.py`，或執行打包後的 `dist/Meshtastic_Auto_Flash`  
   此時自動設定工具會開始運行，接下來請依照終端機提示操作。

5. 設定完畢後，工具會完成刷寫、重開機、套用設定與驗證流程。

1. 下載 [自動設定工具](https://github.com/OLDWAYS069/HermesX/tree/master/auto_flash_tool)
   這是一個zip檔，內含：
`flash_and_config.ps1`：設定工具本身
`flash_and_config.log`：設定工具的LOG
`release`：我們需要設定的韌體，請丟進去這裡
`CLI.md`：你想要設定的參數都可以透過這裡調整，參數設定請參閱[Meshtastic.org](https://meshtastic.org/docs/software/python/cli/)
`README.md` ：你現在正在看的東西
2. 將一根細的Pin針插入裝置正面、螢幕左上角的小洞
同時使用USB-C電纜連接到你的PC

3. 打開 `自動設定工具.exe`
此時自動設定工具會開始運行，接下來就請盯著序列視窗，他會指導你該怎麼做。

4. 設定完畢後，工具會自動關閉，然後就結束了！
:::warning
如果你在過程中遇到問題，請將當次輸出的 `flash_and_config.log` 一併保存，方便後續分析。
:::

## 已知問題

- 某些裝置在刷寫或設定過程中，可能因 USB 重新枚舉或裝置不穩定，而需要重新偵測序列埠。
- Windows 以外的平台流程目前仍在整理中。
