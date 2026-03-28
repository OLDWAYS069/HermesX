# APP中的各項功能是什麼？

ＯＫ，當你們第一次將裝置連線到手，一定會有幾個問題
我該如何用它以及我該設定啥？

好消息，你幾乎不需要特別設定什麼，因為我都幫你設定好了
壞消息，你需要學一下怎麼操做APPＸＤＤＤ

這邊直接分成兩大類 IOS、Android
:::info
我發現就連官方都沒寫過APP的使用說明，看在我這麼有誠意的份上，給我個讚吧QQ
:::
<br>



# iOS上的MESHTASTIC APP


<img src="https://hackmd.io/_uploads/SyAD02SjWe.png" style="width: 30%;">


這是你第一次配對完成後會看到的畫面
（如果你還不知道該如何配對，請見[配對教學](https://youtu.be/Y76uIQsr1Hw?si=wr-sX35B5sHpTmUc))

## 下方你會看到：
### - **訊息** : 主要的訊息收發都會在這裡操作
### - **Connect**：配對裝置用的地方
### - **節點**：查看目前已連線過的節點
### - **地圖**：查看裝置位置
### - **設定**：設定 

<br><br>
<br>
<br>




# 訊息

#### 其實這部分相對簡單好理解，上面是頻道的聊天室
#### 下面是私訊的聊天室
<img src="https://hackmd.io/_uploads/S1cFx6Hi-l.png" style="width: 30%;">

<img src="https://hackmd.io/_uploads/HJcYeTHobl.png" style="width: 30%;">

<img src="https://hackmd.io/_uploads/r1cFlarjbg.png" style="width: 30%;">




而我們在出廠時都已經幫你們設定好台灣主要的公用頻道了
大部分的台灣節點都是使用這組頻道<br>
<br>


簡單介紹一下：
- MESHTW：抬槓用，你可以上去發問
- Signal Test：給通訊測試用的，你可以用這個頻道與其他節點做通訊測試
- Emergency：台灣社群主要所使用的緊急救難頻道 *請不要在這裡聊天*:-1: 
<br>



頻道中有分上鎖的跟沒上鎖的
==***上鎖的代表有加密、沒上鎖的代表沒有加密！***==
<br>


#### 而細心的你也會發現，頻道前面有`0` \ `1` \ `2` \ `3`的字樣

`0` 代表著 **“主要頻道”**
你的定位主要會分享到這個頻道
所以未來在自行設定頻道時，請不要將==未加密==的頻道設為 `0` 


:::info
要能互傳訊息的前提是 你們在同一個頻道中
如果不在同一個頻道，看不到對方的節點資料喔
:::
<br>
<br>


# 節點

<img src="https://hackmd.io/_uploads/Sk1utaSsbx.png" style="width: 30%;">
<img src="https://hackmd.io/_uploads/SkPntprsWl.png" style="width: 30%;">
<img src="https://hackmd.io/_uploads/r183K6rsZe.png" style="width: 30%;">


#### 這部分其實功能滿雜的，我認為要一次講清楚也有點難
#### 不過簡單來說，這是一個讓你去查看 ==你OR別人== 的==節點資料==的地方
<br>

### 常用的功能==TraceRoute==
<br>

![IMG_3079](https://hackmd.io/_uploads/rJEmoarobe.jpg)

這是一個讓使用者**測試訊號收發強度**的功能
當你找到你想要測試的機器後
在他的節點資料庫下方找到==TraceRoute==就可以測試你與他之間的通聯強度了

:::info 
這個功能主要針對LORA情境做使用
你如果開著MQTT並對著一個你靠MQTT連到的裝置做測試

你會看到以下大便（可以注意到訊號強度是**0db**）

<img src="https://hackmd.io/_uploads/HyTkhaHs-g.jpg" style="width: 70%;"> 
:::

<br>

## 地圖
# 他就是地圖，圖片我傳不上來:(

<br>

## 設定
