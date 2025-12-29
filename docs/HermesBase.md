---
title: HermesBase說明

---


# HermesBase 是啥？
- HermesBase是一款針對純Lora網路節點的所開發的Meshtastic分支。
- 主要針對固定式節點設計，請確保這台裝置在網路中擁有較好的收訊位置。
- 我們提供了遠端管理功能，讓使用者僅需透過幾行文字訊息即可快速掌握節點資訊。
- 我們也引用了LobbS系統，讓地方網路也能有類似BBS的看板功能。
- 目前的資料庫僅存放在本機當中，未來我們會陸續開發可供享資源的系統。

![IMG_0578](https://hackmd.io/_uploads/Skp4Ote4Zl.jpg)


## 快速上手
- 將 HermesBase 韌體刷入你的節點當中，同時提供他良好的電源及位置以利節點運行。
- 開機後請“私訊”該節點，並輸入/hi以進入Lobbs系統。
- 開機後保持在主要頻道（預設頻道 0），所有的訊息都會發在上面。
- 確保本機已設定位置，Welcome 功能才會判斷距離並觸發歡迎訊息。
- 公頻指令請直接在聊天頻道輸入；指令（如登入Lobbs）需對 HermesBase 傳送私訊。

## 購買渠道
- HermesBase 是由 HermesTrack 團隊開發的開源韌體，你只需要下載我們的更新包即可安裝。
- 目前我們只支援ESP32S3的板子,NRF還在開發排程中。

## 關於Lobbs
- 加入 BBS：傳送私訊 /hi <username> <password> 給節點；已存在則登入，不存在則自動建立帳號。
- 登出：/bye 解除目前 session 與 NodeID 的綁定。
- 郵件 (Mail)：/mail 列出最近 10 則，/mail 3 讀第 3 則，/mail 5- 從第 5 則開始列。登入狀態下，在訊息裡使用 @username 可直接送私信。
- 公告 (News)：/news 用法同 mail；/news <內容> 可發布新公告。
- 用戶查詢：/users 顯示使用者名單，可附加篩選字串（如 /users mesh）。
- 其他提示：LoBBS 會在列表中標示未讀（＊）並顯示相對時間；所有資料儲存在裝置檔案系統，清除檔案系統或完整重置會刪除 BBS 內容。

- 未來我想要安置一些台灣地區各大避難所的位置進系統中，然後他會判斷你節點所在位置列出最近的幾個。
~~（但感覺是大工程啦，我感覺我會死掉。~~
    
**登入Lobbs系統（私訊）**    
<img src="https://hackmd.io/_uploads/H1Cw_YeEWl.jpg" width="300">

    
    
    

### WelcomeModule
    
- 觸發條件：收到新節點的定位資料，且我們已取得對方的基本資訊；距離在設定半徑內時會廣播歡迎訊息（預設 20 公里，每個節點只歡迎一次）。（註：未來應該會以24小時為判斷，也就是說你如果超過24小時沒回來他就會在歡迎你一次

- 歡迎訊息內容：
  ```
  歡迎 <對方暱稱或「新朋友」> 進入台灣妹婿-花蓮分區!

  LoBBS 指令（請私訊我）：
  登入： /hi <帳號> <密碼> 

  公頻指令：
  @BAT： 查看伺服器電量
  ＠戳 ：戳一下我
  ＠HermesBase：有關於HermesBase
  ```
- 管理： 私訊該節點並打上 `/welcome` 查看、`/welcome on|off` 開關，`/welcome radius <公里>` 調整半徑。


## LighthouseModule（節點管理）
- 公頻指令：
  - `@Status`：請 HermesBase 報告狀態。
  - `@BAT`：回報電池狀態。
  - `@戳`：回覆「討厭><」。
  - `@HermesBase`：簡介 HermesBase 與更多資訊連結。
  - `@HiHermes`：廣播自我介紹。
  - `@EmergencyActive:<密碼>` 或白名單來源的 `@EmergencyActive`：啟動緊急模式
  - `@GoToSleep` 進入節能輪詢；
  - `@Repeater` 切換回中繼站(Client)模式。(之後會更新成Client_Base)

## 建議的使用節奏
- 戶外部署：開機後設定裝置定位，Welcome 模組才會自動迎新；LoBBS 登入後可調整偵測半徑或暫停。
- 公頻互動：用 `@BAT` 看電量、`@Status` 了解 Lighthouse 狀態，或用 `＠戳`、`＠HermesBase` 進行簡單回覆。
- 緊急情境：授權用戶可送出 `@EmergencyActive`；需要恢復時用 `@GoToSleep` 或按流程退出。

# HermesBase 是啥？
- HermesBase是一款針對純Lora網路節點的所開發的Meshtastic分支。
- 主要針對固定式節點設計，請確保這台裝置在網路中擁有較好的收訊位置。
- 我們提供了遠端管理功能，讓使用者僅需透過幾行文字訊息即可快速掌握節點資訊。
- 我們也引用了LobbS系統，讓地方網路也能有類似BBS的看板功能。
- 目前的資料庫僅存放在本機當中，未來我們會陸續開發可供享資源的系統。

![IMG_0578](https://hackmd.io/_uploads/Skp4Ote4Zl.jpg)


## 快速上手
- 將 HermesBase 韌體刷入你的節點當中，同時提供他良好的電源及位置以利節點運行。
- 開機後請“私訊”該節點，並輸入/hi以進入Lobbs系統。
- 開機後保持在主要頻道（預設頻道 0），所有的訊息都會發在上面。
- 確保本機已設定位置，Welcome 功能才會判斷距離並觸發歡迎訊息。
- 公頻指令請直接在聊天頻道輸入；指令（如登入Lobbs）需對 HermesBase 傳送私訊。

## 購買渠道
- HermesBase 是由 HermesTrack 團隊開發的開源韌體，你只需要下載我們的更新包即可安裝。
- 目前我們只支援ESP32S3的板子,NRF還在開發排程中。

## 關於Lobbs
- 加入 BBS：傳送私訊 /hi <username> <password> 給節點；已存在則登入，不存在則自動建立帳號。
- 登出：/bye 解除目前 session 與 NodeID 的綁定。
- 郵件 (Mail)：/mail 列出最近 10 則，/mail 3 讀第 3 則，/mail 5- 從第 5 則開始列。登入狀態下，在訊息裡使用 @username 可直接送私信。
- 公告 (News)：/news 用法同 mail；/news <內容> 可發布新公告。
- 用戶查詢：/users 顯示使用者名單，可附加篩選字串（如 /users mesh）。
- 其他提示：LoBBS 會在列表中標示未讀（＊）並顯示相對時間；所有資料儲存在裝置檔案系統，清除檔案系統或完整重置會刪除 BBS 內容。

- 未來我想要安置一些台灣地區各大避難所的位置進系統中，然後他會判斷你節點所在位置列出最近的幾個。
~~（但感覺是大工程啦，我感覺我會死掉。~~
    
**登入Lobbs系統（私訊）**    
<img src="https://hackmd.io/_uploads/H1Cw_YeEWl.jpg" width="300">

    
    
    

### WelcomeModule
    
- 觸發條件：收到新節點的定位資料，且我們已取得對方的基本資訊；距離在設定半徑內時會廣播歡迎訊息（預設 20 公里，每個節點只歡迎一次）。（註：未來應該會以24小時為判斷，也就是說你如果超過24小時沒回來他就會在歡迎你一次

- 歡迎訊息內容：
  ```
  歡迎 <對方暱稱或「新朋友」> 進入台灣妹婿-花蓮分區!

  LoBBS 指令（請私訊我）：
  登入： /hi <帳號> <密碼> 

  公頻指令：
  @BAT： 查看伺服器電量
  ＠戳 ：戳一下我
  ＠HermesBase：有關於HermesBase
  ```
- 管理： 私訊該節點並打上 `/welcome` 查看、`/welcome on|off` 開關，`/welcome radius <公里>` 調整半徑。


## LighthouseModule（節點管理）
- 公頻指令：
  - `@Status`：請 HermesBase 報告狀態。
  - `@BAT`：回報電池狀態。
  - `@戳`：回覆「討厭><」。
  - `@HermesBase`：簡介 HermesBase 與更多資訊連結。
  - `@HiHermes`：廣播自我介紹。
  - `@EmergencyActive:<密碼>` 或白名單來源的 `@EmergencyActive`：啟動緊急模式
  - `@GoToSleep` 進入節能輪詢；
  - `@Repeater` 切換回中繼站(Client)模式。(之後會更新成Client_Base)

## 建議的使用節奏
- 戶外部署：開機後設定裝置定位，Welcome 模組才會自動迎新；LoBBS 登入後可調整偵測半徑或暫停。
- 公頻互動：用 `@BAT` 看電量、`@Status` 了解 Lighthouse 狀態，或用 `＠戳`、`＠HermesBase` 進行簡單回覆。
- 緊急情境：授權用戶可送出 `@EmergencyActive`；需要恢復時用 `@GoToSleep` 或按流程退出。
