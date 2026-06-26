import tkinter as tk
from tkinter import messagebox
import re
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

# --- 設定區 ---
CODEPOINT_FILE = "HermesX_CN12.codepoints.inc"
GLYPH_FILE = "HermesX_CN12.glyphs.inc"
FONT_PATH = "C:\\Windows\\Fonts\\msjh.ttc"  
FONT_SIZE = 12
GLYPH_WIDTH = 12
GLYPH_HEIGHT = 12
GLYPH_BYTES = 18  # <--- 核心修正！144 Bits / 8 = 18 Bytes

class HermesFontTool:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("HermesX 字庫擴充 - 18Bytes 精準打包版")
        
        self.font_data = {}
        self.load_existing_fonts()
        self.build_ui()

    def load_existing_fonts(self):
        cp_path = Path(CODEPOINT_FILE)
        gl_path = Path(GLYPH_FILE)
        
        if not cp_path.exists() or not gl_path.exists():
            print("找不到原廠的 .inc 檔案，請確保檔案在同一個資料夾中！")
            return

        cp_text = cp_path.read_text(encoding="utf-8", errors="ignore")
        gl_text = gl_path.read_text(encoding="utf-8", errors="ignore")

        cp_hex = re.findall(r'0x([0-9A-Fa-f]{2})', cp_text)
        gl_hex = re.findall(r'0x([0-9A-Fa-f]{2})', gl_text)

        cp_bytes = bytes(int(x, 16) for x in cp_hex)
        gl_bytes = bytes(int(x, 16) for x in gl_hex)

        count = len(cp_bytes) // 4
        
        if len(gl_bytes) != count * GLYPH_BYTES:
            print(f"⚠️ 警告：字碼有 {count} 個，但圖形資料大小不吻合！請確認是否使用乾淨的原廠檔案。")

        for i in range(count):
            cp = int.from_bytes(cp_bytes[i*4 : i*4+4], "little")
            
            start_idx = i * GLYPH_BYTES
            end_idx = start_idx + GLYPH_BYTES
            glyph = list(gl_bytes[start_idx:end_idx])
            
            if len(glyph) < GLYPH_BYTES:
                glyph.extend([0] * (GLYPH_BYTES - len(glyph)))
                
            self.font_data[cp] = glyph
            
        print(f"啟動成功！已完美載入 {len(self.font_data)} 個既有字元。")

    def build_ui(self):
        tk.Label(self.root, text="1. 請輸入你要測試的文字或句子：").pack(pady=(10, 0))
        self.input_text = tk.Text(self.root, height=8, width=60)
        self.input_text.pack(pady=5)
        tk.Button(self.root, text="↓ 檢查缺字 ↓", command=self.check_chars).pack(pady=5)
        tk.Label(self.root, text="2. 以下是字庫目前沒有的字元：").pack()
        self.result_box = tk.Text(self.root, height=4, width=60)
        self.result_box.pack(pady=5)
        tk.Button(self.root, text="★ 生成新字並完美排序寫入 ★", command=self.generate_and_save, bg="lightgreen", font=("Arial", 10, "bold")).pack(pady=15)

    def check_chars(self):
        text = self.input_text.get("1.0", "end")
        missing = []
        for ch in text:
            if ch.isspace(): continue
            if ord(ch) not in self.font_data:
                if ch not in missing:
                    missing.append(ch)
                    
        self.result_box.delete("1.0", "end")
        self.result_box.insert("end", "".join(missing))

    def generate_and_save(self):
        chars = self.result_box.get("1.0", "end").strip()
        
        if not chars:
            messagebox.showinfo("提示", "目前沒有缺字需要新增！")
            return
            
        try:
            font = ImageFont.truetype(FONT_PATH, FONT_SIZE)
        except IOError:
            messagebox.showerror("錯誤", f"找不到字型檔 {FONT_PATH}")
            return

        added_count = 0
        for ch in set(chars):
            cp = ord(ch)
            if cp in self.font_data: continue

            img = Image.new("1", (GLYPH_WIDTH, GLYPH_HEIGHT), color=0)
            draw = ImageDraw.Draw(img)
            draw.text((0, -2), ch, font=font, fill=1) 

            # --- 核心修正：將 144 Bits 緊密打包成 18 Bytes ---
            bits = []
            for y in range(GLYPH_HEIGHT):
                for x in range(GLYPH_WIDTH):
                    bits.append(1 if img.getpixel((x, y)) > 0 else 0)
            
            glyph_data = []
            for i in range(0, 144, 8):
                byte_val = 0
                for j in range(8):
                    byte_val |= (bits[i+j] << (7 - j))
                glyph_data.append(byte_val)
            # -----------------------------------------------

            self.font_data[cp] = glyph_data
            added_count += 1

        if added_count == 0: return

        sorted_cps = sorted(self.font_data.keys())
        cp_hex_list = []
        gl_hex_list = []

        for cp in sorted_cps:
            cp_b = cp.to_bytes(4, byteorder='little')
            cp_str = ",".join([f"0x{b:02X}" for b in cp_b])
            cp_hex_list.append(cp_str)

            gl_b = self.font_data[cp]
            if len(gl_b) < GLYPH_BYTES:
                gl_b.extend([0] * (GLYPH_BYTES - len(gl_b)))
            gl_str = ",".join([f"0x{b:02X}" for b in gl_b])
            gl_hex_list.append(gl_str)

        def format_inc(hex_list, items_per_line):
            lines = []
            for i in range(0, len(hex_list), items_per_line):
                lines.append(",".join(hex_list[i:i+items_per_line]))
            return ",\n".join(lines) + ",\n" 

        with open(CODEPOINT_FILE, "w", encoding="utf-8") as f:
            f.write(format_inc(cp_hex_list, 6))

        with open(GLYPH_FILE, "w", encoding="utf-8") as f:
            f.write(format_inc(gl_hex_list, 1))  # 這裡每 18 Bytes 換行一次，維持原廠風格

        self.result_box.delete("1.0", "end")
        messagebox.showinfo("完成", f"成功新增 {added_count} 個字元！\n總共 {len(sorted_cps)} 個字已精準重新排序。\n\n請重新編譯 ESP32。")

    def run(self):
        self.root.mainloop()

if __name__ == "__main__":
    app = HermesFontTool()
    app.run()