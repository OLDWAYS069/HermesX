# HermesX Update Build Handoff

這份文件給後續協作 AI 使用。

每次你修改 HermesX 更新功能，並且已經成功編譯 `heltec-wireless-tracker` 後，回覆使用者前必須做以下收尾。

## 1. 確認 build 版本

從編譯輸出找到：

```text
-DAPP_HERMES_VERSION=HXB0.2.9_YYYYMMDD_HHMM
```

後續檔名必須使用這個版本碼。

如果你沒有完整編譯，只做語法檢查或局部確認，不要假裝有新韌體產物。

## 2. 搬移韌體產物

成功編譯後，把產物搬到桌面資料夾：

```bash
mkdir -p '/Users/oldways/Desktop/HermesX韌體'
cp '/Users/oldways/HermesX/.pio/build/heltec-wireless-tracker/firmware.bin' '/Users/oldways/Desktop/HermesX韌體/<APP_HERMES_VERSION>.bin'
cp '/Users/oldways/HermesX/.pio/build/heltec-wireless-tracker/firmware.factory.bin' '/Users/oldways/Desktop/HermesX韌體/<APP_HERMES_VERSION>.factory.bin'
```

範例：

```bash
cp '/Users/oldways/HermesX/.pio/build/heltec-wireless-tracker/firmware.bin' '/Users/oldways/Desktop/HermesX韌體/HXB0.2.9_20260430_0428.bin'
```

## 3. 回覆 WiFi 更新指令

使用者目前常用的裝置 IP：

```text
192.168.43.21
```

回覆時提供這條格式：

```bash
curl -# -H 'Expect:' -H 'X-Hermes-Filename: <APP_HERMES_VERSION>.bin' -T /Users/oldways/Desktop/HermesX韌體/<APP_HERMES_VERSION>.bin http://192.168.43.21/upload-update-bin
```

範例：

```bash
curl -# -H 'Expect:' -H 'X-Hermes-Filename: HXB0.2.9_20260430_0428.bin' -T /Users/oldways/Desktop/HermesX韌體/HXB0.2.9_20260430_0428.bin http://192.168.43.21/upload-update-bin
```

## 4. 回覆格式

最終回覆要短，包含：

- 修改重點
- 編譯是否成功
- 產物位置
- WiFi 更新指令

不要只說「已編譯成功」而不搬檔。
不要只給 `/path/to/...` 這種 placeholder。
不要漏掉 `X-Hermes-Filename`，裝置會用它顯示待更新版本。
