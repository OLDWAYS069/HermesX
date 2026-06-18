# MSG Popup / Recent Send 已知問題

日期：2026-06-03  
狀態：程式碼已修正並完成編譯，尚未實機驗證  
範圍：`src/graphics/Screen.cpp`、`src/modules/CannedMessageModule.cpp`、TFT 色彩區域與 HermesX 中文字型

## 正確需求邊界

- `MSG` 的 Recent Send 列表必須維持原本的小字體與原本版面。
- 只有「詳細訊息」頁面的訊息內文需要放大。
- 詳細訊息內文需要自動換行。
- 詳細訊息超過一頁時，需要像 TraceRoute 詳細內容一樣可向下捲動。
- 新訊息 popup 顯示格式為：

```text
NEW MSG
FROM "短ID"

大略內文

查看 | 略過
```

- Popup 內容使用大字體、顯示 3 秒，並可由 `設定 > UI設定 > 新訊息提示` 開關。

## Popup 問題

### 1. Popup 會殘留 Home 頁面的紅色與綠色區域

**現象**

Popup 顯示時，畫面上仍可能看到 Home 頁面的電池、GPS 等紅色或綠色色塊。

**原因**

Popup 是疊加在原本 Home frame 上繪製，不是獨立的全畫面 frame。TFT 顏色仍會依照 Home frame 已註冊的座標色彩區域解析，因此 popup 位於相同座標的內容會繼承底層色彩。

### 2. Popup 的大字體與小尺寸版面互相衝突

**現象**

Popup 標題、內文或按鈕可能裁切、重疊，尤其在 ST7735 / TFT 小螢幕上明顯。

**原因**

TFT 的 `FONT_MEDIUM` 實際高度可達 28px，但 popup 的 compact layout 仍使用約 12 至 13px 的標題列、行高與按鈕高度。

### 3. Popup 並沒有讓所有文字真正放大

**現象**

英文很大，但中文仍然很小，中英文混排時大小與基線不一致。

**原因**

ASCII 會使用目前選取的 `FONT_MEDIUM`，但 HermesX 中文仍由固定 12x12 的 `HermesX_CN12` 字型繪製。

### 4. `查看 | 略過` 目前不是可選擇的兩個操作

**現象**

畫面顯示兩個按鈕，但沒有選取框或目前選項。按下只會查看，旋轉或方向鍵只會略過。

**原因**

Popup 沒有保存選取狀態；輸入處理將 Press 固定映射為查看，方向、旋鈕、返回固定映射為關閉。

### 5. 連續收到多則訊息時，Popup 顯示內容與查看內容可能不同

**現象**

Popup 顯示訊息 A，但按下查看後可能開啟訊息 B。

**原因**

Popup 顯示期間不接受新訊息取代，但 Recent Send 收到新訊息時會把最新訊息插入索引 0。查看操作固定開啟索引 0，而不是 popup 當下保存的封包。

### 6. Popup 與 TraceRoute popup 疊加時，畫面與輸入處理順序不一致

**現象**

畫面最上層看到 TraceRoute popup，但按鍵可能先作用在新訊息 popup。

**原因**

Overlay 的繪製順序會讓 TraceRoute popup 顯示在新訊息 popup 上方，但 `Screen::handleInputEvent()` 先處理新訊息 popup，再處理 TraceRoute popup。

### 7. 關閉未啟用的 Popup 也可能觸發 TFT 全畫面重繪

**現象**

某些沒有實際顯示 popup 的流程也可能造成閃爍或額外重繪。

**原因**

`dismissIncomingTextPopup()` 不論 popup 是否 active，都會設定 TFT palette reset flag。

### 8. Popup 按下查看後，Recent Send 與 CannedMessage 操作邏輯打架

**現象**

按下查看後確實進入 Recent Send 詳細頁，但滾動、返回或其他輸入可能異常。

**原因**

Popup 的查看路徑直接呼叫 `showTextMessageDetailPage()`，沒有像正常進入 Recent Send 一樣先清理 active 的 `CannedMessageModule` 狀態。Recent Send 詳細頁又被列為允許 CannedMessage composer 運作的頁面，因此兩套輸入狀態可能同時存在。

## MSG / Recent Send 問題

### 9. Recent Send 列表被錯誤放大

**現象**

MSG 列表的標題、返回、傳送者、預覽與頻道名稱全部變成大字體。

**原因**

`drawRecentTextMessagesFrame()` 從 `FONT_SMALL` 改成 `FONT_MEDIUM`，列高、列表起始位置與預覽長度也被連帶修改。這超出需求範圍；需求只要求放大詳細訊息內文。

### 10. 詳細訊息頁只有英文內文變大，中文內文沒有變大

**現象**

英文訊息已放大，但中文訊息仍維持 12x12，中英文混合訊息顯示不一致。

**原因**

詳細頁內文切換到 `FONT_MEDIUM`，但中文仍使用固定尺寸的 `HermesX_CN12`。

### 11. 詳細訊息頁最後一段捲動可能蓋到標頭

**現象**

捲動到最底部時，第一個可見文字行可能畫到正文區域上方，覆蓋分隔線或傳送者資訊。

**原因**

最大捲動值可能不是完整行高的倍數，`drawVisibleWrappedLines()` 會用負的行內偏移繪製第一行，但沒有對正文區域做垂直裁切。

### 12. 詳細訊息頁讀取 payload 時沒有依照 payload 長度限制

**現象**

若文字 payload 沒有 NUL 結尾，可能出現亂碼、異常換行或讀取到訊息以外的資料。

**原因**

詳細頁使用 `%s` 讀取 `decoded.payload.bytes`，沒有依照 `decoded.payload.size` 複製並補上結尾字元。

### 13. 詳細訊息頁會錯誤顯示 Footer，確認鍵可能跳回主功能選單

**現象**

詳細頁可能顯示底部選單提示；按下確認鍵可能不是留在訊息頁，而是開啟主功能選單。

**原因**

`shouldShowHermesXMenuFooter()` 只排除 Recent Send 列表頁，沒有排除詳細頁。詳細頁輸入處理也沒有消耗 Select / Press，事件會繼續落到 Footer 快捷鍵。

### 14. 實體按鍵在 Recent Send 列表頁會忽略目前選項

**現象**

游標停在「返回」時，部分實體按鍵路徑仍會嘗試開啟詳細頁；沒有訊息時還可能前往下一個 frame。

**原因**

`Screen::handleOnPress()` 只判斷目前是否在 Recent Send 列表頁，沒有依照 `listCursor` 判斷選取的是返回或訊息。

### 15. Recent Send 頁面仍可能被自動輪播帶走

**現象**

停留在 MSG 列表或詳細頁時，畫面可能因自動輪播離開。

**原因**

Recent Send 雖然被註解為 action-menu destination，仍然放在正常 frame 集合中；自動輪播會呼叫 `handleOnPress()`，而 MSG 頁面沒有完整鎖定。

### 16. 查看舊訊息時，新訊息會強制切換目前內容

**現象**

使用者正在查看舊訊息時，收到新訊息會突然跳到最新一則，捲動位置也會被重設。

**原因**

`storeRecentTextMessage()` 每次收到訊息都會把 `listCursor`、`selectedIndex`、`detailIndex`、`detailScrollY` 重設到最新訊息。

### 17. 詳細訊息頁按下確認後無法退出

**現象**

進入 MSG 詳細訊息頁後，使用者按下確認 / Press 期待退出，但畫面停留在詳細頁。

**原因**

為了避免詳細頁的 Press 被 Footer、CannedMessage 或 frame switching 搶走，`handleRecentTextMessageDetailInput()` 將
Select / Press 直接消耗，但詳細頁本身沒有可選操作，也沒有提供可視的返回列。結果 Press 變成「吃掉但不動作」。

**修正**

MSG 詳細訊息頁的 Select / Press 與 Back / Cancel / Left / Right 都回到 Recent Send 列表；Up / Down / 旋鈕仍只負責正文捲動。

### 18. 詳細訊息頁中文被放得比英文大很多

**現象**

MSG 詳細訊息頁英文大小正常，但中文變成明顯過大的方塊字，和英文比例不一致。

**原因**

詳細頁正文使用 `FONT_MEDIUM` 顯示英文，但中文一開始把 12x12 HermesX glyph 直接套用 2x 縮放，
變成 24x24；改回 1x 後又只剩 12x12。兩者都沒有對齊英文正文的實際視覺高度。

**修正**

MSG 詳細訊息頁中文不再使用固定 1x / 2x；改以 `FONT_HEIGHT_MEDIUM` 推導中文目標像素高度，
並用同一個目標尺寸做繪製、換行 advance 與捲動步進。

## 修正原則

- 先恢復 Recent Send 列表原本的 `FONT_SMALL`、列高、列表位置與預覽長度。
- 詳細頁標頭可維持原本尺寸，只放大訊息內文。
- 詳細頁的換行計算、繪製尺寸與捲動步進必須使用同一套字型度量。
- 若中文也要求真正放大，不能只切換 ASCII font；需要提供可放大的中文字型或縮放繪製方案。
- 不可把 12x12 中文 glyph 直接 1x 或 2x 當作 `FONT_MEDIUM` 正文；中文目標尺寸必須跟英文正文視覺高度對齊。
- Popup 的顯示封包與查看封包必須綁定，不可固定查看 Recent Send 索引 0。
- Popup、TraceRoute popup、Recent Send 與 CannedMessage 必須有明確且單一的輸入擁有者。
- MSG 列表與詳細頁都應排除 Footer 快捷鍵與自動輪播。
- 沒有可選操作的 detail 頁不可只 consume Select / Press；若頁面沒有按鈕，Press 必須是明確返回或有其他可見行為。
- 新訊息到達時，不應改變使用者正在查看的舊訊息與捲動位置。

## 2026-06-03 修正狀態

- Popup 顯示期間收到新訊息時，會更新 popup 綁定的封包與 3 秒倒數。
- Popup 新增 `查看 / 略過` 選取狀態，旋鈕或方向鍵切換，按下確認目前選項。
- Popup 查看會依照 popup 綁定封包尋找 Recent Send 索引，不再固定開啟索引 0。
- Popup 查看與 Recent Send 頁面會清理 CannedMessage 狀態；Recent Send 不再允許 CannedMessage composer 搶輸入。
- Popup 輸入處理順序改為配合 overlay 視覺層級，TraceRoute popup 優先於新訊息 popup。
- TFT popup 繪製時會清除底層 palette zones；只有實際關閉 active popup 才要求 palette reset。
- Recent Send 列表恢復原本小字體、列高、列表位置與摘要長度。
- 詳細訊息頁只放大正文，英文使用 `FONT_MEDIUM`；中文依英文正文高度重採樣，不再硬套固定 1x / 2x glyph。
- 詳細訊息 payload 改為依照 `payload.size` 安全複製並補上結尾字元。
- 詳細訊息捲動改為整行步進與整行最大捲動值，避免文字畫到標頭區域。
- Recent Send 列表與詳細頁排除 Footer 快捷鍵與自動輪播。
- 實體 Press 在列表頁會依照目前游標執行返回或開啟訊息；詳細頁 Press / Select 會回到 Recent Send 列表。
- 使用者正在查看 Recent Send 時，新訊息插入後會保留目前選取訊息、詳細訊息與捲動位置。

`platformio run -e heltec-wireless-tracker -j 4` 已編譯成功，CIV build 版本為
`HXB_C0.3.2_20260604_1433`。尚待實機驗證 popup、查看操作、詳細訊息捲動與 Press 返回列表行為。
