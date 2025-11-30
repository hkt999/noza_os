# IRQ 整合除錯檢查表

## 當前狀態
- 分支：`irq-integration`
- 變更：新增 kernel IRQ 處理流程 (`noza_irq_handle_from_isr` 等)、RP2040 `platform_irq_*` API、`IRQ_SERVER_VID` 等定義
- 症狀：韌體成功建置與燒錄，但 USB console 無任何輸出（`noza>` 提示符缺失）

## 除錯目標
1. 確認 kernel IRQ 初始化與 PendSV/調度流程整合是否卡住 context switch。
2. 維持 micro-kernel 設計，暫不引入額外 syscall，仍透過 message passing 交付 IRQ 事件。
3. 確認 USB console 維持 `getchar_timeout_us(0)` 輪詢、可看到 boot log。
4. 建立可重複的測試流程（建置 → 燒錄 → USB console 驗證）。

## 進度紀錄
- [x] 1. 釐清 `irq_process_pending()` 在 scheduler/GO_RUN 中的影響：僅在 IRQ bitmap 有事件時才喚醒，與 console 無直接衝突。
- [x] 2. 釐清 `platform_irq_init()` 造成 console 靜默的關鍵：`IRQ_SERVER_VID` 預設為 1，撞到 root thread，導致根服務莫名收到 IRQ 訊息而卡死；改用高位 VID 並保留給 IRQ service。
- [x] 3. 為 `noza_init()`/IRQ handler、`platform_io_init()`、`noza_root_task()` 等關鍵路徑加入 debug log，方便追蹤 boot 進度與是否有 IRQ 觸發。
- [x] 4. 新增 `NOZA_OS_ENABLE_IRQ` 旗標，預設關閉可攜帶 kernel 變更但維持輪詢 console，確保後續能逐步開啟功能。
- [x] 5. 移除 boot 階段的 `printf`/`kernel_log` 以避免 USB 連線尚未建立時阻塞 console，回復原本即插即用行為。
- [x] 6. 重新建置並改用 UART console；IRQ service + client API 上線，`NOZA_OS_ENABLE_IRQ=1`，console RX 改為 UART0 IRQ 驅動且可回退 polling。
