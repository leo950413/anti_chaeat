# 行程注入技術深入解析筆記 (Process Injection Techniques)

> 整理自資安研究文獻與實務分析（參考自 Medium @ril.sejal: *Different Approaches of Process Injection* 及相關逆向研究）

---

## 一、 核心概念：什麼是行程注入？

**行程注入（Process Injection）** 是一種將自訂或惡意代碼（Shellcode、DLL 或完整 PE 執行檔）寫入另一個正在運行的合法行程（Process）的記憶體位址空間，並迫使其執行的進階技術。

### 核心目的與動機：
1. **防禦規避（Defense Evasion）：**  
   直接執行未知或未簽章的 `.exe` 容易被防毒軟體（AV）、EDR 或遊戲防作弊系統直接攔截。透過將代碼寄生在受信任的系統程序（如 `svchost.exe`、`explorer.exe`）或遊戲本體內，能有效規避行程黑名單與靜態特徵檢測。
2. **權限繼承（Privilege Inheritance）：**  
   若目標程式具備高權限（如 `SYSTEM`、`Administrator` 或擁有驅動通訊權限），注入進去的代碼將直接繼承該程式的所有系統特權。
3. **隱匿蹤跡（Stealth）：**  
   工作管理員（Task Manager）中不會出現可疑的額外行程，所有行為（網路連線、記憶體存取）在系統稽核日誌中都會被記錄為宿主程式所為。

---

## 二、 行程注入的「四大通用步驟」

不論採用哪一種注入流派，底層本質幾乎都遵循以下四步基礎範式：

```
[1. 鎖定目標] ──> [2. 配置空間] ──> [3. 寫入載荷] ──> [4. 觸發執行]
 OpenProcess       VirtualAllocEx    WriteProcessMemory   CreateRemoteThread / APC / Context
```

1. **尋找與打開目標（Target Identification）：**  
   使用 `CreateToolhelp32Snapshot`、`Process32First` 遍歷行程取得目標 PID，呼叫 `OpenProcess` 取得具備寫入與執行緒建立權限的句柄（Handle）。
2. **在目標內部開闢空間（Memory Allocation）：**  
   呼叫 `VirtualAllocEx` 在目標行程的虛擬記憶體中申請一塊具備執行權限（如 `PAGE_EXECUTE_READWRITE`）的空間。
3. **將代碼複製進去（Payload Writing）：**  
   呼叫 `WriteProcessMemory` 將 DLL 路徑、Shellcode 或 PE 模組寫入剛剛申請的記憶體區段。
4. **觸發代碼執行（Execution Trigger）：**  
   **這是區分各注入手法最核心的關鍵點**（如何騙過作業系統去執行該段記憶體）。

---

## 三、 六大主流行程注入技術解析

### 1. 傳統 DLL 注入（Classic DLL Injection）
* **實作原理：**
  1. 在目標行程中申請記憶體並寫入 DLL 檔案的磁碟絕對路徑。
  2. 呼叫 `CreateRemoteThread`，並將起始位址指向 `Kernel32.dll` 中的 `LoadLibraryA`，把 DLL 路徑作為參數傳入。
* **優點：** 實作最簡單、最直觀，易於快速驗證功能。
* **缺點：** DLL 必須實體存放在磁碟上，容易觸發磁碟靜態掃描；且會觸發核心回調 `PsSetLoadImageNotifyRoutine`，極易被防作弊/防毒捕捉。

---

### 2. 反射式 DLL 注入（Reflective DLL Injection）
* **實作原理：** **檔案不落地（Fileless / Memory-Only）**。
  1. 外部程式直接將 DLL 的二進制原始數據讀取到記憶體中，寫入目標行程。
  2. DLL 內部自建一個微型的「反射載入器（Reflective Loader）」。
  3. 執行該載入器，由載入器在記憶體中自行解析 PE Header、修正重定位表（Relocations）並手動填寫導入位址表（IAT）。
* **優點：** 磁碟無檔案特徵，繞過 Windows 原生的 `LoadLibrary` 機制，PEB 模組清單中完全隱形。

---

### 3. PE / Shellcode 直接注入（Shellcode Injection）
* **實作原理：**
  1. 攻擊者將功能直接編譯為純機器碼（Shellcode），或將獨立的 PE 映像直接寫入目標記憶體。
  2. 呼叫 `CreateRemoteThread` 或現有執行緒跳轉，直接指到 Shellcode 的起始進入點執行。
* **優點：** 體積極小（數百 bytes），不依賴 Windows 模組載入機制，啟動速度極快。

---

### 4. 行程掏空（Process Hollowing）
* **實作原理：**
  1. 使用 `CreateProcess` 加上 `CREATE_SUSPENDED`（暫停旗標）啟動一個乾淨合法的系統程式（如 `svchost.exe` 或 `notepad.exe`）。
  2. 使用底層 API（如 `ZwUnmapViewOfSection`）將該程式原本的代碼段從記憶體中徹底「挖空」。
  3. 在挖空出的位址上寫入惡意程式的 PE 鏡像，並修復 Base Address 與重定位。
  4. 使用 `SetThreadContext` 將執行緒進入點指到新寫入的代碼，最後呼叫 `ResumeThread` 喚醒執行。
* **優點：** 行程名稱、路徑、數位簽章完全是正版微軟軟體，偽裝度極高。

---

### 5. APC 佇列注入（Asynchronous Procedure Call Injection）
* **實作原理：**
  1. 不建立新的執行緒（避免觸發執行緒建立監控）。
  2. 透過 `QueueUserAPC` 將惡意任務插入到目標程式**現有的合法執行緒佇列**中。
  3. 當該執行緒進入「可警醒等待狀態（Alertable State，例如呼叫了 `SleepEx`、`WaitForSingleObjectEx`）」時，CPU 會自動跳轉執行排隊中的 APC 函式。
* **優點：** 不會產生新的未受信任執行緒，規避 `CreateRemoteThread` 的核心監控。

---

### 6. 執行緒劫持（Thread Hijacking / Context Manipulation）
* **實作原理：**
  1. 呼叫 `SuspendThread` 暫停目標程式現有的一條正常執行緒。
  2. 呼叫 `GetThreadContext` 讀取 CPU 暫存器。
  3. 將指令指標暫存器（64 位元的 `RIP` 或 32 位元的 `EIP`）篡改為我們寫入的 Shellcode 位址。
  4. 呼叫 `SetThreadContext` 寫回修改後的暫存器，最後呼叫 `ResumeThread`。
* **優點：** 在已受信任的既有執行緒內直接執行，堆疊隱蔽性高。

---

## 四、 防作弊（Anti-Cheat）與端點安全（EDR）防禦策略

針對上述注入技術，現代防護系統的應對手段包括：

1. **核心權限剝奪（ObRegisterCallbacks）：**  
   防作弊驅動直接在核心攔截 `OpenProcess`，強制拔除外部程式對受保護遊戲的 `PROCESS_VM_WRITE` 與 `PROCESS_CREATE_THREAD` 權限。
2. **記憶體屬性與頁面審計（Memory Page Scanning）：**  
   定期掃描行程記憶體，檢測是否存在沒有實體檔案背書（Unbacked / `MEM_PRIVATE`），卻被賦予了可執行權限（`PAGE_EXECUTE_READWRITE`）的可疑區段。
3. **執行緒堆疊回溯（Stack Walking）：**  
   定期檢視各執行緒的呼叫堆疊（Call Stack），若發現某個函式的回傳位址（Return Address）或進入點位於未知的記憶體空間，直接判定為非法注入。
4. **核心回調監控（Kernel Callbacks）：**  
   使用 `PsSetCreateThreadNotifyRoutine` 與 `PsSetLoadImageNotifyRoutine` 即時監控所有新執行緒與 DLL 載入行為，凡未經驗證的模組一律阻斷。
