---
title: '自動設定工具'
---

:::info
本工具尚在開發中，如遇到問題，請聯繫團隊粉絲專頁
目前版本僅支援Windows作業系統，MacOS及Linus的版本將會在後續推出
:::

# 自動設定工具
你是否對繁瑣的設定流程感到厭煩？
裝置不小心設定壞了怎麼辦？？

來使用我們的 ***自動設定工具*** 吧！
這是一個 一鍵設定的自動化工具，允許使用者將我們的HermesX恢復成 **預先備份**  好的設定



## 使用方式

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
如果你在過程中遇到問題，請將當次輸出的`flash_and_config.log`傳送到我們的電子信箱
hermestw05@gmail.com
:::

## 已知問題
- HermesX_0.2.9_civ-alpha0002會因裝置不穩定等問題導致需要在設定過程中多次手動重新開機。