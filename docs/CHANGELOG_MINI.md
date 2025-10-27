# HermesX Mini Change Log
（每次只寫極簡重點，便於快速回顧）

## 2025-10-27
- 紀錄 HermesX BootHold 行為：若畫面出現 `Resuming...`，表示由 EXT1/RTC 喚醒，必須持續按住開機鍵直到超過 `BUTTON_LONGPRESS_MS` 才會繼續開機；提早放開將依設計重新進入深睡。
- 目前問題：雖然現在可以嘗按開機了，但開機後大約5秒又會再度開機，目前懷疑是門檻設定異常導致