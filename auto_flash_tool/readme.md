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

# 教學影片
<iframe width="560" height="315" src="https://www.youtube.com/embed/xQYpQ_BsUMA?si=3C6SQx8p1HaAa6jq" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe>


## 使用方式

1. 下載 [自動設定工具](https://github.com/OLDWAYS069/HermesX/tree/master/auto_flash_tool)
   這是一個zip檔，內含：

`tool_windows`：設定工具本身所在的資料夾
點開裡頭有
`Meshtastic_Auto_Flash.exe`

:::info
ＭacOS版本的正在開發中
:::

`flash_and_config.log`：設定工具的LOG，
預設是不存在的
當你低一次執行他才會產生
你如果遇到問題請將這個檔案複製下來並寄送到我們的電子信箱中

`release`：目前最新版的韌體都會在裡頭

`target`：請將你想要刷寫的韌體丟到這裡面（.bin)


`CLI.md`：你想要設定的參數都可以透過這裡調整，參數設定請參閱[Meshtastic.org](https://meshtastic.org/docs/software/python/cli/)

`README.md` ：你現在正在看的東西

2. 將一根細的Pin針插入裝置正面、螢幕左上角的小洞
同時使用USB-C電纜連接到你的PC

3. 打開 `命令工具行`
此時自動設定工具會開始運行，接下來就請盯著序列視窗，他會指導你該怎麼做。

4. 設定完畢後，工具會自動關閉，然後就結束了！
:::warning
如果你在過程中遇到問題，請將當次輸出的`flash_and_config.log`傳送到我們的電子信箱
hermestw05@gmail.com
:::

